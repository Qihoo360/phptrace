/**
 * Copyright 2017 Yuchen Wang <phobosw@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <libgen.h>
#include <limits.h>

#include "trace_version.h"
#include "trace_filter.h"

#include "cli.h"
#include "trace.h"
#include "status.h"

void context_init(void);
void context_show(void);
void usage(void);
void usage_full(void);
void parse_args(int argc, char **argv);

/* True Global */
pt_context_t clictx;

volatile int interrupted = 0; /* flag of interrupt */

/* Commands */
enum {
    CMD_UNKNOWN = -1,
    CMD_INVALID,

    CMD_TRACE,
    CMD_STATUS,
    CMD_VERSION,
};

static void sighandler(int signal)
{
    pt_info("Catch signal (%d)", signal);
    interrupted = signal;
}

void pt_log(int level, char *filename, int lineno, const char *format, ...)
{
    va_list ap;

    if (level > clictx.verbose) {
        return;
    }

    if (level == PT_ERROR) {
        fputs("ERROR: ", stderr);
    } else if (level == PT_WARNING) {
        fputs("WARN:  ", stderr);
    } else if (level == PT_INFO) {
        fputs("INFO:  ", stderr);
    } else if (level == PT_DEBUG) {
        fputs("DEBUG: ", stderr);
    }

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    if (errno) {
        fprintf(stderr, " \"%s(%d)\"", strerror(errno), errno);
        errno = 0;
    }

    if (level == PT_DEBUG) {
        fprintf(stderr, " %s:%d", basename(filename), lineno);
    }

    fputs("\n", stderr);
}

void context_init(void)
{
    clictx.command = CMD_UNKNOWN;
    clictx.verbose = PT_ERROR;

    clictx.pid = PT_PID_INVALID;
    clictx.ptrace = 0;

    memset(&clictx.ctrl, 0, sizeof(clictx.ctrl));
    memset(clictx.ctrl_file, 0, sizeof(clictx.ctrl_file));

    clictx.sfd = -1;
    memset(clictx.listen_addr, 0, sizeof(clictx.listen_addr));

    pt_filter_ctr(&clictx.pft);

    clictx.limit.frame = PT_LIMIT_UNLIMITED; 
    clictx.limit.request = PT_LIMIT_UNLIMITED; 
}

void context_show(void)
{
    char *cmdname;

    /* command name */
    if (clictx.command == CMD_TRACE) {
        cmdname = "trace";
    } else if (clictx.command == CMD_STATUS) {
        cmdname = "status";
    } else if (clictx.command == CMD_VERSION) {
        cmdname = "version";
    } else {
        cmdname = "unknown";
    }

    pt_info("command = %s", cmdname);
    pt_info("verbose = %d", clictx.verbose);
    pt_info("pid = %d", clictx.pid);
    pt_info("ptrace = %d", clictx.ptrace);
    pt_info("ctrl_file = %s", clictx.ctrl_file);
    pt_info("sfd = %d", clictx.sfd);
    pt_info("listen_addr = %s", clictx.listen_addr);
}

void usage(void)
{
    printf(
"Usage: phptrace -p <pid>...\n"
"       phptrace -h | --help\n"
    );
    exit(EXIT_SUCCESS);
}

void usage_full(void)
{
    printf(
"phptrace - A low-overhead tracing tool for PHP\n"
"\n"
"Usage: phptrace -p <pid>...\n"
"       phptrace -h | --help\n"
"\n"
"Commands:\n"
"    trace               Trace a running PHP process [default]\n"
"    status              Display current status of specified PHP process\n"
"    version             Show version\n"
"\n"
"Options:\n"
"        --ptrace        Fetch data using ptrace, only in status mode\n"
"    -p, --pid           Process id\n"
"    -h, --help          Show this screen\n"
"    -v, --verbose\n"
"    -f, --filter        Filter data by type [url,function,class] and content\n"
"    -l, --limit         Limit output count\n"
    );
    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char **argv)
{
    int c, long_index;
    char *end;
    char *limit_end;
    char *sub_value;
    char *subopts;
    int limit = 0;

    struct option long_opts[] = {
        {"pid",     required_argument,  NULL, 'p'},
        {"filter",  required_argument,  NULL, 'f'},
        {"limit",   required_argument,  NULL, 'l'},
        {"help",    no_argument,        NULL, 'h'},
        {"verbose", no_argument,        NULL, 'v'},
        {"ptrace",  no_argument,        NULL, 0},
        {0, 0, 0, 0}
    };

    enum {
        FILTER_TYPE = 0,
        FILTER_CONTENT,
        FILTER_COUNT,
    };

    char *const filter_token[] = {
        [FILTER_TYPE]       = "type",
        [FILTER_CONTENT]    = "content",
        NULL
    };

    if (argc < 2) {
        return;
    }

    /* sub-command */
    if (strcmp("trace", argv[1]) == 0) {
        clictx.command = CMD_TRACE;
    } else if (strcmp("version", argv[1]) == 0) {
        clictx.command = CMD_VERSION;
    } else if (strcmp("status", argv[1]) == 0) {
        clictx.command = CMD_STATUS;
    } else if (argv[1][0] != '-') {
        clictx.command = CMD_INVALID;
    } else {
        clictx.command = CMD_UNKNOWN;
    }

    /* options */
    while ((c = getopt_long(argc, argv, "vhp:f:l:", long_opts, &long_index)) != -1) {
        switch (c) {
            case 'p':
                if (strcmp("all", optarg) == 0) {
                    clictx.pid = PT_PID_ALL;
                    break;
                }

                clictx.pid = (int) strtol(optarg, &end, 10);
                if (errno || *end != '\0' || clictx.pid < 0 || clictx.pid > PT_PID_MAX) {
                    pt_error("Invalid process id \"%s\"", optarg);
                    exit(EXIT_FAILURE);
                }

                /* check process exists */
                if (kill(clictx.pid, 0) == -1 && errno == ESRCH) {
                    pt_error("Process %s not exists", optarg);
                    exit(EXIT_FAILURE);
                }

                break;
            case 'f':
                subopts = optarg;
                while (*subopts != '\0') {
                    switch (getsubopt(&subopts, filter_token, &sub_value)) {
                        case FILTER_TYPE:
                            if (sub_value == NULL) {
                                pt_error("Invalid filter type");
                                exit(EXIT_FAILURE);
                            }
                            pt_filter_search_filter_type(sub_value, &(clictx.pft.type));
                            break;
                        case FILTER_CONTENT:
                            if (strlen(sub_value) >= MAX_FILTER_LENGTH - 1) {
                               pt_error("Invalid filter content size overflow");
                               exit(EXIT_FAILURE);
                            }
                            sdscpy(clictx.pft.content, sub_value);
                            break;
                        default:
                            pt_error("Invalid filter param \"%s\"", sub_value);
                            exit(EXIT_FAILURE);
                    }
                }

                if (sdslen(clictx.pft.content) == 0 || clictx.pft.type == PT_FILTER_EMPTY) {
                    pt_error("Invalid filter param , content or type is empty");
                    exit(EXIT_FAILURE);
                } 

                break;
            case 'l':
                limit = (int) strtol(optarg, &limit_end, 10);
                if (errno || *limit_end != '\0' || limit < PT_LIMIT_UNLIMITED) {
                    pt_error("Invalid limit \"%s\"", optarg);
                    exit(EXIT_FAILURE);
                }
                
                break;
            case 'h':
                usage_full();
                break;

            case 'v':
                ++clictx.verbose;
                break;

            case 0:
                if (long_index == 3) { /* --ptrace */
                    clictx.ptrace = 1;
                }
                break;

            default:
                usage();
                break;
        }
    }

    /* default sub-command */
    if (clictx.command == CMD_UNKNOWN && clictx.pid != PT_PID_INVALID) {
        clictx.command = CMD_TRACE;
    }

    /* all process only in trace mode */
    if (clictx.command != CMD_TRACE && clictx.pid == PT_PID_ALL) {
        pt_error("Access all process only available in trace mode");
        exit(EXIT_FAILURE);
    }

    /* set limit */
    if (limit != 0) {
        if (clictx.pft.type & PT_FILTER_URL) {
            clictx.limit.request = limit;
        } else {
            clictx.limit.frame = limit; 
        }
    }
}

int main(int argc, char **argv)
{
    int status;

    context_init();

    parse_args(argc, argv);

    if (signal(SIGINT, sighandler) == SIG_ERR || signal(SIGTERM, sighandler) == SIG_ERR) {
        pt_error("Register signal handler failed");
        return EXIT_FAILURE;
    }

    strcpy(clictx.ctrl_file, "/tmp/" PT_CTRL_FILENAME);
    if (pt_ctrl_create(&clictx.ctrl, clictx.ctrl_file) == -1) {
        pt_error("Control module open failed");
        return EXIT_FAILURE;
    }

    strcpy(clictx.listen_addr, "/tmp/" PT_COMM_FILENAME);
    clictx.sfd = pt_comm_listen(clictx.listen_addr);
    if (clictx.sfd == -1) {
        pt_error("Socket listen failed");
        return EXIT_FAILURE;
    }

    context_show();

    status = 0;
    switch (clictx.command) {
        case CMD_VERSION:
            printf("php-trace version %s (cli:%s)\n", TRACE_VERSION, TRACE_CLI_VERSION);
            break;

        case CMD_TRACE:
            status = pt_trace_main();
            break;

        case CMD_STATUS:
            status = pt_status_main();
            break;

        default:
            usage();
            break;
    }

    pt_comm_close(clictx.sfd, clictx.listen_addr);

    pt_ctrl_close(&clictx.ctrl);

    pt_filter_dtr(&clictx.pft);

    return status;
}
