/*
 *  trace cmdtools
 */

#include "phptrace_util.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# include <stdint.h>
#endif

enum {
    OPTION_STATUS = CHAR_MAX + 1,
    OPTION_CLEANUP,
    OPTION_MAX_LEVEL,
    OPTION_EXCLUSIVE,
    OPTION_FORMAT
};

static address_info_t address_templates[] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 64, 0, 968, 0, 16, 0, 8, 112, 64, 0, 168, 0, 112},
    {0, 64, 0, 1360, 0, 8, 0, 8, 80, 40, 0, 168, 0, 0, 112},
    {0, 64, 0, 1152, 0, 8, 0, 8, 80, 40, 0, 144, 0, 0, 40},
    {0, 64, 0, 1120, 0, 8, 0, 8, 48, 24, 0, 152, 0, 0, 40},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

volatile int interrupted;        /* flag of interrupt (CTRL + C) */

static void interrupt(int sig)
{
    log_printf(LL_DEBUG, "catch SIGINT signal");
    interrupted = sig;
}

static void parse_args(phptrace_context_t *ctx, int argc, char *argv[])
{
    int c;
    int len;
    long addr;
    int opt_index;
    long sapi_globals_addr = 0;
    long executor_globals_addr = 0;
    static struct option long_options[] = {
        {"php-version", required_argument, 0, OPTION_STATUS},
        {"sapi-globals", required_argument, 0, OPTION_STATUS},
        {"executor-globals", required_argument, 0, OPTION_STATUS},
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
        error_msg(ctx, ERR_INVALID_PARAM, "too few arguments");
        usage();
        exit(-1);
    }

    while ((c = getopt_long(argc, argv, "hc::S:n:l:p:o:svw:r:", long_options, &opt_index)) != -1) {
        switch (c) {
            case OPTION_STATUS:             /* args for stack */
                if (opt_index == 0) {
                    if (!optarg) {
                        error_msg(ctx, ERR_INVALID_PARAM, "php-version: null");
                        exit(-1);
                    }
                    if (strlen(optarg) < 5 || optarg[0] != '5' || optarg[1] != '.') {
                        error_msg(ctx, ERR_INVALID_PARAM, "php-version: %s", optarg);
                        exit(-1);
                    }
                    ctx->php_version = optarg[2] - '0';
                } else if (opt_index == 1) {
                    if (!optarg || (addr = hexstring2long(optarg, strlen(optarg))) == -1) {
                        error_msg(ctx, ERR_INVALID_PARAM, "sapi-globals: %s", (optarg ? optarg : "null"));
                        exit(-1);
                    }
                    sapi_globals_addr = addr;
                } else if (opt_index == 2) {
                    if (!optarg || (addr = hexstring2long(optarg, strlen(optarg))) == -1) {
                        error_msg(ctx, ERR_INVALID_PARAM, "executor-globals: %s", (optarg ? optarg : "null"));
                        exit(-1);
                    }
                    executor_globals_addr = addr;
                }
                break;
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
                if (ctx->php_pid <= 0 || ctx->php_pid > PID_MAX) {
                    error_msg(ctx, ERR_INVALID_PARAM, "process '%s' is out of the range (0, %d)", optarg, PID_MAX);
                    exit(-1);
                }
                if (ctx->php_pid == getpid()) {
                    error_msg(ctx, ERR_INVALID_PARAM, "cannot trace the self process '%d'", ctx->php_pid);
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
        if (ctx->php_pid < 0) {
            error_msg(ctx, ERR_INVALID_PARAM, "cleanup needs an pid, please add -p pid!");
            exit(-1);
        }
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

    if (ctx->opt_flag == OPT_FLAG_STATUS) {
        if (ctx->php_version && (ctx->php_version < PHP52 || ctx->php_version > PHP55)) {
            error_msg(ctx, ERR_INVALID_PARAM, "php version is not supported\n");
            exit(-1);
        }

        memcpy(&(ctx->addr_info), &address_templates[ctx->php_version], sizeof(address_info_t));
        ctx->addr_info.sapi_globals_addr = sapi_globals_addr;
        ctx->addr_info.executor_globals_addr = executor_globals_addr;
        log_printf(LL_DEBUG, "php version: %d", ctx->php_version);
        log_printf(LL_DEBUG, "sapi_globals_addr: %d", ctx->addr_info.sapi_globals_addr);
        log_printf(LL_DEBUG, "executor_globals_addr: %d", ctx->addr_info.executor_globals_addr);
    }
}

int main(int argc, char *argv[])
{
    char buf[MAX_TEMP_LENGTH];
    phptrace_context_t context;

    printf("%s %s, published by %s\n", PHPTRACE_NAME, PHPTRACE_VERSION, PHPTRACE_DEVELOPER);

    phptrace_context_init(&context);

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
        snprintf(buf, MAX_TEMP_LENGTH, "%s.%d", PHPTRACE_LOG_DIR "/" PHPTRACE_TRACE_FILENAME,  context.php_pid);
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

