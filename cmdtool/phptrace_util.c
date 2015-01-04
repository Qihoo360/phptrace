#include "phptrace_util.h"

static address_info_t address_templates[] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 64, 0, 968, 0, 16, 0, 8, 112, 64, 0, 168, 0, 112},
    {0, 64, 0, 1360, 0, 8, 0, 8, 80, 40, 0, 168, 0, 0, 112},
    {0, 64, 0, 1152, 0, 8, 0, 8, 80, 40, 0, 144, 0, 0, 40},
    {0, 64, 0, 1120, 0, 8, 0, 8, 48, 24, 0, 152, 0, 0, 40},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

enum {
    ERR = 0,
    ERR_INVALID_PARAM,
    ERR_STACK,
    ERR_CTRL,
    ERR_TRACE,
};

static const char *ERR_MSG[] = {
    "ERROR",
    "INVALID PARAM",
    "STACK ERROR",
    "CONTROL ERROR",
    "TRACE ERROR",
};

long hexstring2long(const char*s, size_t len)
{
    int i;
    long l, n;
    char c;

    if (!s || len <= 2 || s[0] != '0' || s[1] != 'x') {
        return -1;
    }
    n = 0;
    l = 0;
    for (i = 2; i < len; i++) {
        c = s[i];
        if (c >= 'a' && c <= 'f') {
            n = (long)c - (long)'a' + 10;
        } else if (c >= '0' && c <= '9') {
            n = (long)c - (long)'0';
        }
        l = l * 16 + n;
    }
    return l;
}
unsigned int string2uint(const char *str)
{
    char *error;
    long value;

    if (!*str) {
        return -1;
    }
    errno = 0;
    value = strtol(str, &error, 10);
    if (errno || *error || value < 0 || (long)(int)value != value) {
        return -1;
    }
    return (unsigned int)value;
}

void trace_cleanup(phptrace_context_t *ctx)
{
    log_printf (LL_DEBUG, " [func:trace_cleanup]\n");

    if (ctx->trace_flag) {
        phptrace_ctrl_clean_one(&(ctx->ctrl), ctx->php_pid);
        log_printf (LL_DEBUG, " clean ctrl pid=%d~~~~~\n", ctx->php_pid);
    }

    if (!ctx->seg.shmaddr && ctx->seg.shmaddr != MAP_FAILED) {
        phptrace_unmap(&(ctx->seg));
    }
    if (ctx->mmap_filename) {
        sdsfree(ctx->mmap_filename);
        ctx->mmap_filename = NULL;
    }
    if (ctx->file.tailer.filename) {
        sdsfree(ctx->file.tailer.filename);
    }
}

void verror_msg(phptrace_context_t *ctx, int err_code, const char *fmt, va_list p)
{
    char msg[1024] = {0};
    int ret;

    fflush(ctx->log);
    if (err_code) {
        fprintf(ctx->log, "%s", ERR_MSG[err_code]);
    }
    ret = vsnprintf(msg, 1024, fmt, p);
    if (ret > 0) {
        fprintf(ctx->log, ": %s", msg);
    }
    fprintf(ctx->log, "\n");
}

void error_msg(phptrace_context_t *ctx, int err_code, const char *fmt, ...)
{
    va_list p;
    va_start(p, fmt);
    verror_msg(ctx, err_code, fmt, p);
    va_end(p);
}

void die(phptrace_context_t *ctx, int exit_code)
{
    trace_cleanup(ctx);
    exit(exit_code);
}

void phptrace_context_init(phptrace_context_t *ctx)
{
    memset(ctx, 0, sizeof(phptrace_context_t));

    ctx->start_time = phptrace_time_usec();

    ctx->log = stdout;
    ctx->mmap_filename = NULL;
    ctx->php_pid = -1;
    ctx->max_print_len = MAX_PRINT_LENGTH;

    log_level_set(LL_ERROR + 1);
}

void trace_start_ctrl(phptrace_context_t *ctx)
{
    int8_t num = -1;

    if (!phptrace_ctrl_init(&(ctx->ctrl))) {                    
        error_msg(ctx, ERR_CTRL, "cannot open control mmap file %s (%s)", 
                  PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME, (errno ? strerror(errno) : "null"));
        die(ctx, -1);
    }
    log_printf (LL_DEBUG, " [ok] ctrl_init open (%s)", PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME);

    phptrace_ctrl_get(&(ctx->ctrl), &num, ctx->php_pid);
    if (num > 0) {
        error_msg(ctx, ERR_CTRL, "process %d has been traced", ctx->php_pid);
        die(ctx, -1);
    }
    log_printf (LL_DEBUG, " [ok] read ctrl data: ctrl[pid=%d]=%d is closed!\n", ctx->php_pid, num);

    phptrace_ctrl_set(&(ctx->ctrl), (uint8_t)1, ctx->php_pid);
    ctx->trace_flag = 1;
}

void process_opt_c(phptrace_context_t *ctx)
{
    if (ctx->php_pid >= 0) {
        phptrace_ctrl_clean_one(&(ctx->ctrl), ctx->php_pid);
        printf ("clean process %d.\n", ctx->php_pid);
    } else {
        phptrace_ctrl_clean_all(&(ctx->ctrl));
        printf ("clean all process.\n");
    }
    phptrace_ctrl_destroy(&(ctx->ctrl));
}

int update_mmap_filename(phptrace_context_t *ctx)
{
    char buf[MAX_TEMP_LENGTH];

    if (ctx->mmap_filename == NULL) {
        snprintf(buf, MAX_TEMP_LENGTH, "%s.%d", PHPTRACE_LOG_DIR "/" PHPTRACE_TRACE_FILENAME,  ctx->php_pid);
        ctx->mmap_filename = sdsnew(buf);
    } else if (!ctx->file.tailer.filename) {
        return 0;
    } else if (!sdscmp(ctx->file.tailer.filename, ctx->mmap_filename)) {
        return 0;
    } else {
        sdsfree(ctx->mmap_filename);
        if (ctx->seg.shmaddr && ctx->seg.shmaddr != MAP_FAILED) {
            if (phptrace_unmap(ctx->seg.shmaddr) < 0) {
                return -1;
            }
        }
        ctx->seg.shmaddr = MAP_FAILED;
        ctx->mmap_filename = sdsnew(ctx->file.tailer.filename);
    }
    if (ctx->mmap_filename == NULL) {
        return -1;
    }

    return 0;
}

void interrupt(int sig)
{
   log_printf (LL_DEBUG, " Handle Ctrl+C interrupt\n");
   interrupted = sig;
}

void usage()
{
    printf ("usage: phptrace [ -chsel ]  [-p pid] \n\
    -h          -- show this help\n\
    -c          -- clear the trace switches of pid, or all the switches if no pid parameter\n\
    -p pid      -- access the php process i\n\
    -s          -- print call stack of php process by the pid\n\
    -l size     -- specify the maximun string length to print\n\
    -v          -- set log level to debug\n\
    \n");
}

sds print_indent_str(sds s, char* str, int32_t size)
{
    int32_t i;
    for (i = 0; i < size; i++)
        s = sdscatprintf (s, "%s", str);
    return s;
}

sds print_time(sds s, uint64_t t)
{
    return sdscatprintf (s, "%lld.%06d", (long long)(t / 1000000), (int)(t % 1000000));
}

sds sdscatrepr_noquto(sds s, const char *p, size_t len)
{
    while (len--) {
        switch (*p) {
            case '\\':
            case '\n': s = sdscatlen(s,"\\n",2); break;
            case '\r': s = sdscatlen(s,"\\r",2); break;
            case '\t': s = sdscatlen(s,"\\t",2); break;
            case '\a': s = sdscatlen(s,"\\a",2); break;
            case '\b': s = sdscatlen(s,"\\b",2); break;
            default:
                       if (isprint(*p)) {
                           s = sdscatprintf(s,"%c",*p);
                       } else {
                           s = sdscatprintf(s,"\\x%02x",(unsigned char)*p);
                       }
                       break;
        }
        p++;
    }
    return s;
}

void print_record(phptrace_context_t *ctx, phptrace_file_record_t *r)
{
    sds buf = sdsempty();
    if (r->flag == RECORD_FLAG_ENTRY) {
        buf = print_time(buf, r->start_time);
        buf = print_indent_str(buf, "  ", r->level);
        buf = sdscatprintf (buf, "%s(", r->function_name);
        buf = sdscatrepr_noquto(buf, RECORD_ENTRY(r, params),
                MIN(sdslen(RECORD_ENTRY(r, params)), ctx->max_print_len));
        buf = sdscatprintf (buf, ")  %s:%u", RECORD_ENTRY(r, filename), RECORD_ENTRY(r, lineno));
    } else {
        buf = print_time(buf, r->start_time + RECORD_EXIT(r, cost_time));
        buf = print_indent_str(buf, "  ", r->level);
        buf = sdscatprintf (buf, "%s  =>  ", r->function_name);
        buf = sdscatrepr_noquto(buf, RECORD_EXIT(r, return_value),
                MIN(sdslen(RECORD_EXIT(r, return_value)), ctx->max_print_len));
        buf = sdscatprintf (buf, "    ");
        buf = print_time(buf, RECORD_EXIT(r, cost_time));
    }
    log_printf (LL_DEBUG, " record_sds[%s]\n", buf);
    printf ("%s\n", buf);
    sdsfree (buf);
    fflush(NULL);
}

void phptrace_record_free(phptrace_file_record_t *r)
{
    if (r->function_name != NULL) {
        sdsfree (r->function_name);
        r->function_name = NULL;
    }
    if (r->flag == RECORD_FLAG_ENTRY) {
        if (RECORD_ENTRY(r, params)) {
            sdsfree (RECORD_ENTRY(r, params)); 
            RECORD_ENTRY(r, params) = NULL;
        }
        if (RECORD_ENTRY(r, filename)) { 
            sdsfree (RECORD_ENTRY(r, filename));
            RECORD_ENTRY(r, filename) = NULL;
        }
    } else {
        if (RECORD_EXIT(r, return_value)) { 
            sdsfree (RECORD_EXIT(r, return_value));
            RECORD_EXIT(r, return_value) = NULL;
        }
    }
}

void trace(phptrace_context_t *ctx)
{
    int rc;
    phptrace_file_record_t rcd;
    int state = STATE_START;
    int64_t flag;
    int64_t seq = -1;

    void *mem_ptr = NULL;
    int opendata_wait_interval = OPEN_DATA_WAIT_INTERVAL;
    int data_wait_interval = DATA_WAIT_INTERVAL;

    while (1) {
        if (interrupted) {
            die(ctx, 0);
        }

        if (state == STATE_ERROR) {
            break;
        }

        /* open mmap has 2 entries:
         *  a. first time(STATE_START);
         *  b. another time(STATE_TAILER)
         */
        while (state == STATE_START || state == STATE_TAILER) {
            if (interrupted) {
                die(ctx, 0);
            }

            log_printf(LL_DEBUG, "open trace mmap file, sleep %d ms.\n", opendata_wait_interval);
            phptrace_msleep(opendata_wait_interval);
            opendata_wait_interval = grow_interval(opendata_wait_interval, MAX_OPEN_DATA_WAIT_INTERVAL);

            rc = update_mmap_filename(ctx);
            if (rc < 0) {
                error_msg(ctx, ERR_TRACE, "update trace mmap filename failed");
                die(ctx, -1);
            }

            if (!ctx->seg.shmaddr || ctx->seg.shmaddr == MAP_FAILED) {
                ctx->seg = phptrace_mmap_read(ctx->mmap_filename);
                if (ctx->seg.shmaddr == MAP_FAILED) {
                    log_printf(LL_NOTICE, "phptrace_mmap_read '%s' failed!", ctx->mmap_filename);
                    continue;
                }
            }

            log_printf(LL_DEBUG, "phptrace_mmap_read '%s' successfully!", ctx->mmap_filename);
            state = STATE_OPEN;
            mem_ptr = ctx->seg.shmaddr;
        }
        opendata_wait_interval = OPEN_DATA_WAIT_INTERVAL;

        phptrace_ctrl_heartbeat_ping(&(ctx->ctrl), ctx->php_pid);

        /* read header */
        phptrace_mem_read_64b(flag, mem_ptr);

        log_printf (LL_DEBUG, "flag=(%lld) (0x%llx)", flag, flag);
        if (flag != WAIT_FLAG) {
            data_wait_interval = DATA_WAIT_INTERVAL;
        } else {
            log_printf (LL_DEBUG, "  WAIT_FLAG, will wait %d ms.\n", flag, data_wait_interval);
        }

        switch (flag) {
            case MAGIC_NUMBER_HEADER:
                if (state != STATE_OPEN) {
                    error_msg(ctx, ERR_TRACE, "state is not correct (STATE_OPEN), trace file may be damaged");
                    die(ctx, -1);
                }

                mem_ptr = phptrace_mem_read_header(&(ctx->file.header), mem_ptr);
                state = STATE_HEADER;
                log_printf (LL_DEBUG, " [ok load header]");
                break;
            case MAGIC_NUMBER_TAILER:
                if (state != STATE_HEADER && state != STATE_RECORD) {
                    error_msg(ctx, ERR_TRACE, "state is not correct (STATE_HEADER or STATE_RECORD). trace file may be damaged");
                    die(ctx, -1);
                }

                state = STATE_TAILER;
                mem_ptr = phptrace_mem_read_tailer(&(ctx->file.tailer), mem_ptr);
                log_printf (LL_DEBUG, " [ok load tailer]");
                break;
            case WAIT_FLAG:
                phptrace_msleep(data_wait_interval);
                data_wait_interval = grow_interval(data_wait_interval, MAX_DATA_WAIT_INTERVAL);
                continue;
            default:
                if (state == STATE_HEADER) {
                    state = STATE_RECORD;
                }
                if (state != STATE_RECORD) {
                    error_msg(ctx, ERR_TRACE, "state is not correct (STATE_RECORD). trace file may be damaged");
                    die(ctx, -1);
                }
                if (flag != seq + 1) {
                    error_msg(ctx, ERR_TRACE, "sequence number is incorrect. trace file may be damaged");
                    die(ctx, -1);
                }

                mem_ptr = phptrace_mem_read_record(&(rcd), mem_ptr, flag);
                if (!mem_ptr) {
                    error_msg(ctx, ERR_TRACE, "read record failed. maybe write too fast");
                    die(ctx, -1);
                }

                print_record(ctx, &(rcd));
                phptrace_record_free(&(rcd));
                seq++;
                break;
        }
    }
}

int stack_dump_once(phptrace_context_t* ctx)
{
    static const int buf_size = 1024;
    char buf[buf_size];
    long l, execute_data;
    pid_t pid = ctx->php_pid;

    if (0 > sys_trace_get_strz(pid, buf, buf_size, (long) (ctx->addr_info.sapi_globals_addr + ctx->addr_info.sapi_globals_path_offset))) {
        log_printf(LL_NOTICE, "sys_trace_get_strz failed");
        return -1;
    }
    printf("script_filename = %s\n", buf);
    if (0 > sys_trace_get_long(pid, ctx->addr_info.executor_globals_addr + ctx->addr_info.executor_globals_ed_offset, &l)) {
        log_printf(LL_NOTICE, "sys_trace_get_long failed");
        return -1;
    }
    ctx->addr_info.execute_data_addr = l;
    execute_data = ctx->addr_info.execute_data_addr;

    while (execute_data) {
        printf("[0x%" PTR_FMT "lx] ", execute_data);

        if (0 > sys_trace_get_long(pid, execute_data + ctx->addr_info.execute_data_f_offset, &l)) {
            log_printf(LL_NOTICE, "sys_trace_get_long failed");
            return -1;
        }
        ctx->addr_info.function_addr = l;

        if (valid_ptr(ctx->addr_info.function_addr)) {
            if (0 > sys_trace_get_strz(pid, buf, buf_size, (long) (ctx->addr_info.function_addr + ctx->addr_info.function_fn_offset))) {
                log_printf(LL_NOTICE, "sys_trace_get_strz failed");
                return -1;
            }
            printf(" %s", buf);
        }

        if (0 > sys_trace_get_long(pid, execute_data + ctx->addr_info.execute_data_oparray_offset, &l)) {
            log_printf(LL_NOTICE, "sys_trace_get_long failed");
            return -1;
        }
        ctx->addr_info.oparray_addr = l;

        if (valid_ptr(ctx->addr_info.oparray_addr)) {
            if (0 > sys_trace_get_strz(pid, buf, buf_size, (long) (ctx->addr_info.oparray_addr + ctx->addr_info.oparray_fn_offset))) {
                log_printf(LL_NOTICE, "sys_trace_get_strz failed");
                return -1;
            }
            printf(" %s", buf);
        }

        if (0 > sys_trace_get_long(pid, execute_data + ctx->addr_info.execute_data_opline_offset, &l)) {
            log_printf(LL_NOTICE, "sys_trace_get_long failed");
            return -1;
        }
        ctx->addr_info.opline_addr = l;
        if (valid_ptr(ctx->addr_info.opline_addr)) {
            uint *lu = (uint *) &l;
            uint lineno;
            if (0 > sys_trace_get_long(pid, ctx->addr_info.opline_addr + ctx->addr_info.opline_ln_offset, &l)) {
                log_printf(LL_NOTICE, "sys_trace_get_long failed");
                return -1;
            }
            lineno = *lu;
            printf(":%d", lineno);
        }

        printf("\n");

        if (0 > sys_trace_get_long(pid, execute_data + ctx->addr_info.execute_data_prev_offset, &l)) {
            log_printf(LL_NOTICE, "sys_trace_get_long failed");
            return -1;
        }
        execute_data = l;
#if 0
        log_printf(LL_DEBUG, "sapi_globals_addr %lld", ctx->addr_info.sapi_globals_addr);
        log_printf(LL_DEBUG, "sapi_globals_path_offset %lld", ctx->addr_info.sapi_globals_path_offset);
        log_printf(LL_DEBUG, "executor_globals_addr %lld", ctx->addr_info.executor_globals_addr);
        log_printf(LL_DEBUG, "executor_globals_ed_offset %lld", ctx->addr_info.executor_globals_ed_offset);
        log_printf(LL_DEBUG, "execute_data_addr %lld", ctx->addr_info.execute_data_addr);
        log_printf(LL_DEBUG, "execute_data_f_offset %lld", ctx->addr_info.execute_data_f_offset);
        log_printf(LL_DEBUG, "function_addr %lld", ctx->addr_info.function_addr);
        log_printf(LL_DEBUG, "function_fn_offset %lld", ctx->addr_info.function_fn_offset);
        log_printf(LL_DEBUG, "execute_data_prev_offset %lld", ctx->addr_info.execute_data_prev_offset);
        log_printf(LL_DEBUG, "execute_data_oparray_offset %lld", ctx->addr_info.execute_data_oparray_offset);
        log_printf(LL_DEBUG, "oparray_addr %lld", ctx->addr_info.oparray_addr);
        log_printf(LL_DEBUG, "oparray_fn_offset %lld", ctx->addr_info.oparray_fn_offset);
        log_printf(LL_DEBUG, "execute_data_opline_offset %lld", ctx->addr_info.execute_data_opline_offset);
        log_printf(LL_DEBUG, "opline_addr %lld", ctx->addr_info.opline_addr);
        log_printf(LL_DEBUG, "opline_ln_offset %lld", ctx->addr_info.opline_ln_offset);
#endif
        ctx->stack_deep++;
        if (ctx->stack_deep >= MAX_STACK_DEEP) {
            break;
        }
    }

    return 0;
}

int stack_dump(phptrace_context_t* ctx)
{
    int ret;
    ret = 0;
    while (1) {
        if ((ret = stack_dump_once(ctx)) >= 0) {
            return 0;
        }

        log_printf(LL_NOTICE, "stack_dump_once failed: %d", ctx->php_pid);
        sleep(1);

        ctx->retry++;
        if (ctx->retry >= MAX_RETRY) {
            break;
        }
    }
    return -1;
}

void process_opt_s(phptrace_context_t *ctx)
{
    int ret;

    if (sys_trace_attach(ctx->php_pid)) {
        log_printf(LL_NOTICE, "sys_trace_attach failed");
        error_msg(ctx, ERR_STACK, "sys_trace_attach failed (%s)", (errno ? strerror(errno) : "null"));
        return; 
    }

    ret = stack_dump(ctx);
    if (ret < 0) {
        error_msg(ctx, ERR_STACK, "dump stack failed. maybe the process is not executing a php script");
    }

    if (sys_trace_detach(ctx->php_pid) < 0) {
        log_printf(LL_NOTICE, "sys_trace_detach failed");
        error_msg(ctx, ERR_STACK, "sys_trace_detach failed (%s)", (errno ? strerror(errno) : "null"));
    }

    sys_trace_kill(ctx->php_pid, SIGCONT);
}

void init(phptrace_context_t *ctx, int argc, char *argv[])
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
        {0, 0, 0, 0}
    };

    if (argc < 2) {      /* no  options */
        usage();
        exit(-1);
    }

    phptrace_context_init(ctx);

    while ((c = getopt_long(argc, argv, "hcl:p:sv", long_options, &opt_index)) != -1) {
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
                    error_msg(ctx, ERR_INVALID_PARAM, "process '%s' is out of the range (0, 32768)", optarg);
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
            default:
                usage();
                exit(0);
        }
    }

    if (!ctx->opt_p_flag) {
        error_msg(ctx, ERR_INVALID_PARAM, "no process id");
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

    if (signal(SIGINT, interrupt) == SIG_ERR) {
        error_msg(ctx, ERR, "install signal handler failed (%s)", (errno ? strerror(errno) : "null"));
        exit(-1);
    }
}

void process(phptrace_context_t *ctx)
{
    printf("%s %s, published by %s\n", PHPTRACE_NAME, PHPTRACE_VERSION, PHPTRACE_DEVELOPER);

    /* stack */
    if (ctx->opt_s_flag > 0) {
        process_opt_s(ctx);
        exit(0);
    }

    /* clean */
    if (ctx->opt_c_flag > 0) {
        process_opt_c(ctx);
        exit(0);
    }

    /* trace */
    trace_start_ctrl(ctx);
    trace(ctx);
}
