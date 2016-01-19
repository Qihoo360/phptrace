/**
 * Copyright 2015 Yuchen Wang <phobosw@gmail.com>
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
#include <error.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>

#include "cli.h"
#include "trace.h"
#include "version.h"

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

    CMD_TRACE,
    CMD_STACK,
    CMD_VERSION,
};

static void sighandler(int signal)
{
    pt_log(PT_ERROR, "Catch signal(%d)", signal);
    interrupted = signal;
}

int pt_log(int level, const char *format, ...)
{
    int ret;
    va_list ap;

    if (level > clictx.verbose) {
        return 0;
    }

    va_start(ap, format);
    fputs("", stderr);
    ret = vfprintf(stderr, format, ap);
    fputs("\n", stderr);
    va_end(ap);

    return ret;
}

void context_init(void)
{
    clictx.command = CMD_UNKNOWN;
    clictx.verbose = 0;

    clictx.pid = PT_PID_INVALID;
}

void context_show(void)
{
    char *cmdname;

    /* command name */
    if (clictx.command == CMD_TRACE) {
        cmdname = "trace";
    } else if (clictx.command == CMD_STACK) {
        cmdname = "stack";
    } else if (clictx.command == CMD_VERSION) {
        cmdname = "version";
    } else {
        cmdname = "unknown";
    }

    printf(
        "command: %s\n"
        "verbose: %d\n"
        "pid: %d\n",
        cmdname, clictx.verbose, clictx.pid
    );
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
"    stack               Display call-stack of a PHP process\n"
"    version             Show version\n"
"\n"
"Options:\n"
"    -p, --pid           Process id\n"
"    -h, --help          Show this screen\n"
"    -v, --verbose\n"
    );
    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char **argv)
{
    int c, long_index;
    char *end;

    struct option long_opts[] = {
        {"pid",     required_argument,  NULL, 'p'},
        {"help",    no_argument,        NULL, 'h'},
        {"verbose", no_argument,        NULL, 'v'},
        {0, 0, 0, 0}
    };

    if (argc < 2) {
        usage();
        return;
    }

    /* sub-command */
    if (argv[1][0] == '-') {
        clictx.command = CMD_TRACE;
    } else {
        if (strcmp("trace", argv[1]) == 0) {
            clictx.command = CMD_TRACE;
        } else if (strcmp("version", argv[1]) == 0) {
            clictx.command = CMD_VERSION;
        } else if (strcmp("stack", argv[1]) == 0) {
            clictx.command = CMD_STACK;
        } else {
            clictx.command = CMD_UNKNOWN;
        }
    }

    /* options */
    while ((c = getopt_long(argc, argv, "vhp:", long_opts, &long_index)) != -1) {
        switch (c) {
            case 'p':
                if (strcmp("all", optarg) == 0) {
                    clictx.pid = PT_PID_ALL;
                    break;
                }

                clictx.pid = (int) strtol(optarg, &end, 10);
                if (errno || *end != '\0' || clictx.pid < 0 || clictx.pid > PT_PID_MAX) {
                    error(EXIT_FAILURE, errno, "Invalid process id \"%s\"", optarg);
                }
                break;

            case 'h':
                usage_full();
                break;

            case 'v':
                ++clictx.verbose;
                break;

            default:
                usage();
                break;
        }
    }
}

int main(int argc, char **argv)
{
    int status;

    context_init();

    parse_args(argc, argv);

    if (clictx.verbose > 0) {
        context_show();
    }

    if (signal(SIGINT, sighandler) == SIG_ERR) {
        pt_log(PT_ERROR, "Register signal handler failed");
        exit(EXIT_FAILURE);
    }

    switch (clictx.command) {
        case CMD_VERSION:
            status = 0;
            printf("php-trace version %s (cli:%s)\n", TRACE_VERSION, TRACE_CLI_VERSION);
            break;

        case CMD_TRACE:
            status = pt_trace_main();
            break;

        default:
            usage();
            break;
    }

    return status;
}
