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
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# include <stdint.h>
#endif

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
        {"php-version", required_argument, 0, 0},
        {"sapi-globals", required_argument, 0, 0},
        {"executor-globals", required_argument, 0, 0},
        {"help",   no_argument, 0, 'h'},                    /* help */
        {"clear",  no_argument, 0, 'c'},                    /* clean switches of pid | all */
        {"max-string-length",  required_argument, 0, 'l'},  /* max string length to print */
        {"pid",  required_argument, 0, 'p'},                /* trace pid */
        {"stack",  no_argument, 0, 's'},                    /* dump stack */
        {"verbose",  no_argument, 0, 'v'},                  /* print verbose information */
        {"dump",  required_argument, 0, 'w'},                     /* dump trace log */
        {"read",  required_argument, 0, 'w'},                     /* dump trace log */
        {0, 0, 0, 0}
    };

    if (argc < 2) { /* no options */
        error_msg(ctx, ERR_INVALID_PARAM, "too few arguments");
        usage();
        exit(-1);
    }

    while ((c = getopt_long(argc, argv, "hcl:p:svw:r:", long_options, &opt_index)) != -1) {
        switch (c) {
            case 0:             /* args for stack */
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
            case 'h':
                usage();
                exit(0);
            case 'c':
                ctx->opt_c_flag = 1;
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
                    error_msg(ctx, ERR_INVALID_PARAM, "cannot trace the current process '%d'", ctx->php_pid);
                    exit(-1);
                }
                ctx->opt_p_flag = 1;
                break;
            case 's':
                ctx->opt_s_flag = 1;
                break;
            case 'v':
                log_level_set(log_level_get() - 1);
                break;
            case 'w':
                ctx->out_filename = sdsnew(optarg);
                break;
            case 'r':
                ctx->in_filename = sdsnew(optarg);
                break;
            default:
                usage();
                exit(0);
        }
    }

    if (!ctx->opt_p_flag && !ctx->in_filename) {
        error_msg(ctx, ERR_INVALID_PARAM, " need option -p process id or -r file to read");
        exit(-1);
    }

    if (ctx->opt_s_flag > 0 && ctx->opt_c_flag > 0) {
        error_msg(ctx, ERR_INVALID_PARAM, "-s conflicts with -c");
        exit(-1);
    }

    if (ctx->opt_s_flag > 0) {
        if (!sapi_globals_addr || !executor_globals_addr) {
            error_msg(ctx, ERR_INVALID_PARAM, "address info not enough");
            exit(-1);
        }
        if (ctx->php_version < PHP52 || ctx->php_version > PHP55) {
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
    phptrace_context_t context;

    printf("%s %s, published by %s\n", PHPTRACE_NAME, PHPTRACE_VERSION, PHPTRACE_DEVELOPER);

    phptrace_context_init(&context);

    parse_args(&context, argc, argv);

    /* stack */
    if (context.opt_s_flag > 0) {
        process_opt_s(&context);
        exit(0);
    }

    /* clean */
    if (context.opt_c_flag > 0) {
        process_opt_c(&context);
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
        trace_start(&context);
    }

    /* -w option, dump to file */
    if (context.out_filename) {
        context.out_fp = fopen(context.out_filename, "w");
        if (!context.out_fp) {
            error_msg(&context, ERR, "Can not open %s to dump. (%s)", context.out_filename, (errno ? strerror(errno) : "null"));
            die(&context, -1);
        }
        context.record_printer = dump_print_record;
    }
    trace(&context);

    return 0;
}

