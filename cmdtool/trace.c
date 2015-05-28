/**
 * Copyright 2015 Qihoo 360
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

/*
 *  trace cmdtools
 */

#include "trace.h"
#include "trace_util.h"

#include "trace_status.h"

enum {
    OPTION_STATUS = CHAR_MAX + 1, /* FIXME useless, remove later */
    OPTION_CLEANUP,
    OPTION_MAX_LEVEL,
    OPTION_EXCLUSIVE,
    OPTION_FORMAT
};

static void interrupt(int sig)
{
    interrupted = sig;
    log_printf(LL_DEBUG, "catch SIGINT signal interrupted=%d", interrupted);
}

static void parse_args(pt_context_t *ctx, int argc, char *argv[])
{
    int c;
    int len;
    int opt_index;
    static struct option long_options[] = {
        {"cleanup",  no_argument, 0, OPTION_CLEANUP},                   /* clean switches of pid | all */
        {"max-level",  required_argument, 0, OPTION_MAX_LEVEL},         /* max level to trace or count */
        {"exclusive",  no_argument, 0, OPTION_EXCLUSIVE},               /* use exclusive time when count */
        {"help",   no_argument, 0, 'h'},                                /* help */
        {"count",  optional_argument, 0, 'c'},                          /* count time, calls  */
        {"sortby",  required_argument, 0, 'S'},                         /* sort the output of count results */
        {"max-function",  required_argument, 0, 'n'},                   /* max record to trace or count */
        {"max-string-length",  required_argument, 0, 'l'},              /* max string length to print */
        {"pid",  required_argument, 0, 'p'},                            /* trace pid */
        {"status",  no_argument, 0, 's'},                               /* dump status */
        {"verbose",  no_argument, 0, 'v'},                              /* print verbose information */
        {"dump",  required_argument, 0, 'w'},                           /* dump trace format data to file */
        {"read",  required_argument, 0, 'r'},                           /* read trace format data from file */
        {"output", required_argument, 0, 'o'},                          /* write trace data to file */
        {"format", required_argument, 0, OPTION_FORMAT},                /* specify the format to write */
        {0, 0, 0, 0}
    };

    if (argc < 2) { /* no options */
        usage();
        exit(-1);
    }

    while ((c = getopt_long(argc, argv, "hc::S:n:l:p:o:svw:r:", long_options, &opt_index)) != -1) {
        switch (c) {
            case OPTION_CLEANUP:
                ctx->opt_flag |= OPT_FLAG_CLEANUP;
                break;
            case OPTION_MAX_LEVEL:
                len = string2uint(optarg);
                if (len < 0) {
                    error_msg(ctx, ERR_INVALID_PARAM, "max level should be larger than 0");
                    exit(-1);
                }
                ctx->max_level = MAX(DEFAULT_MAX_LEVEL, len);
                log_printf (LL_DEBUG, "[parse arg] parse option --max-level=%d", ctx->max_level);
                break;
            case OPTION_EXCLUSIVE:
                ctx->exclusive_flag = 1;
                log_printf (LL_DEBUG, "[parse arg] parse option --exclusive");
                break;
            case 'h':
                usage();
                exit(0);
            case 'c':
                len = DEFAULT_TOP_N;
                if (optarg) {
                    len = string2uint(optarg);
                    if (len < 0) {
                        error_msg(ctx, ERR_INVALID_PARAM, " should be larger than 0");
                        exit(-1);
                    }
                }
                ctx->top_n = len;
                ctx->opt_flag |= OPT_FLAG_COUNT;
                log_printf (LL_DEBUG, "[top_n] len=%d\n", len);
                break;
            case 'S':
                if (!set_sortby(ctx, optarg)) {
                    error_msg(ctx, ERR_COUNT, "invalid sortby: '%s'", optarg);
                    exit(-1);
                }
                break;
            case 'n':
                ctx->max_function = string2uint(optarg);
                if (ctx->max_function < 0) {
                    error_msg(ctx, ERR_INVALID_PARAM, "max function should be larger than 0");
                    exit(-1);
                }
                log_printf (LL_DEBUG, "[parse arg] parse option -n=%d", ctx->max_function);
                break;
            case 'l':
                len = string2uint(optarg);
                if (len < 0) {
                    error_msg(ctx, ERR_INVALID_PARAM, "max string length should be larger than 0");
                    exit(-1);
                }
                ctx->max_print_len = len;
                break;
            case 'p':
                ctx->php_pid = string2uint(optarg);
                if (ctx->php_pid <= 0 || ctx->php_pid > PT_PID_MAX) {
                    error_msg(ctx, ERR_INVALID_PARAM, "process '%s' is out of the range (0, %d)", optarg, PT_PID_MAX);
                    exit(-1);
                }
                if (ctx->php_pid == getpid()) {
                    error_msg(ctx, ERR_INVALID_PARAM, "cannot trace the self process '%d'", ctx->php_pid);
                    exit(-1);
                }
                if (kill(ctx->php_pid, 0) == -1 && errno == ESRCH) { /* test process existence */
                    error_msg(ctx, ERR_INVALID_PARAM, "process '%d' does not exists", ctx->php_pid);
                    exit(-1);
                }
                break;
            case 'o':
                ctx->out_filename = sdsnew(optarg);
                log_printf (LL_DEBUG, "[parse arg] parse option -o out_filename=(%s)", ctx->out_filename);
                break;
            case OPTION_FORMAT:
                if (strcmp("json", optarg) == 0 || strcmp("JSON", optarg) == 0) {
                    ctx->output_flag |= OUTPUT_FLAG_JSON;
                } else {
                    error_msg(ctx, ERR_INVALID_PARAM, "invalid format '%d'", optarg);
                    exit(-1);
                }
                break;
            case 's':
                ctx->opt_flag |= OPT_FLAG_STATUS;
                break;
            case 'v':
                log_level_set(log_level_get() - 1);
                break;
            case 'w':
                ctx->out_filename = sdsnew(optarg);
                ctx->output_flag |= OUTPUT_FLAG_WRITE;
                break;
            case 'r':
                ctx->in_filename = sdsnew(optarg);
                break;
            default:
                usage();
                exit(0);
        }
    }

    if (ctx->opt_flag == OPT_FLAG_STATUS) {                                /* status */
        if (ctx->php_pid < 0) {
            error_msg(ctx, ERR_INVALID_PARAM, "status needs an pid, please add -p pid!");
            exit(-1);
        }
    } else if (ctx->opt_flag == OPT_FLAG_COUNT) {                          /* count */
        if ((ctx->php_pid >= 0) + (ctx->in_filename != NULL) != 1) {
            error_msg(ctx, ERR_INVALID_PARAM, "count needs either an pid(option -p) or input file(option -r), not both!");
            exit(-1);
        }
    } else if (ctx->opt_flag == OPT_FLAG_CLEANUP) {                        /* cleanup */
        /* do nothing... */
    } else if (ctx->opt_flag == 0) {                                       /* trace */
        if ((ctx->php_pid >= 0) + (ctx->in_filename != NULL) != 1) {
            error_msg(ctx, ERR_INVALID_PARAM, "trace needs either an pid(option -p) or input file(option -r), not both!");
            exit(-1);
        }
    } else {
        error_msg(ctx, ERR_INVALID_PARAM, "");
        usage();
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    char buf[MAX_TEMP_LENGTH];
    pt_context_t context;

    pt_context_init(&context);

    parse_args(&context, argc, argv);

    /* status */
    if (context.opt_flag & OPT_FLAG_STATUS) {
        process_opt_s(&context);
        exit(0);
    }

    /* clean */
    if (context.opt_flag & OPT_FLAG_CLEANUP) {
        process_opt_e(&context);
        exit(0);
    }

    /* trace */
    if (signal(SIGINT, interrupt) == SIG_ERR) {
        error_msg(&context, ERR, "install signal handler failed (%s)", (errno ? strerror(errno) : "null"));
        exit(-1);
    }

    /* -r option, read from file */
    if (context.in_filename) {
        context.mmap_filename = sdsdup(context.in_filename);
    } else {
        snprintf(buf, MAX_TEMP_LENGTH, "%s.%d", PHPTRACE_LOG_DIR "/" PT_COMM_FILENAME, context.php_pid);
        context.mmap_filename = sdsnew(buf);
        trace_start(&context);
    }

    /* output to file */
    if (context.out_filename) {
        context.out_fp = fopen(context.out_filename, "w");
        if (!context.out_fp) {
            error_msg(&context, ERR, "Can not open %s to dump. (%s)", context.out_filename, (errno ? strerror(errno) : "null"));
            die(&context, -1);
        }

        if (context.output_flag & OUTPUT_FLAG_WRITE) {           /* -w option, dump */
            context.record_transform = dump_transform;
        } else if (context.output_flag & OUTPUT_FLAG_JSON) {     /* -o option, --format json */
            context.record_transform = json_transform;
        }
    }

    trace(&context);

    return 0;
}

