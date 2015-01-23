#include "phptrace_util.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

static const char *ERR_MSG[] = {
    "",
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
    if (ctx->in_filename) {
        sdsfree(ctx->in_filename);
        ctx->in_filename = NULL;
    }
    if (ctx->out_filename) {
        fclose(ctx->out_fp);

        sdsfree(ctx->out_filename);
        ctx->out_filename = NULL;
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

    ctx->php_pid = -1;
    ctx->start_time = phptrace_time_usec();
    ctx->log = stdout;
    ctx->mmap_filename = NULL;

    ctx->in_filename = NULL;
    ctx->out_fp = stdout;
    ctx->out_filename = NULL;

    ctx->max_print_len = MAX_PRINT_LENGTH;
    ctx->seg.shmaddr = MAP_FAILED;

    ctx->record_printer = standard_print_record;
    log_level_set(LL_ERROR + 1);
}

void trace_start(phptrace_context_t *ctx)
{
    int8_t num = -1;
    uint8_t flag = 1;
    struct stat st;

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

    if (ctx->mmap_filename == NULL) {
        ctx->mmap_filename = sdscatprintf(sdsempty(),"%s.%d", PHPTRACE_LOG_DIR "/" PHPTRACE_TRACE_FILENAME,  ctx->php_pid);
    }

    if (stat(ctx->mmap_filename, &st) == 0) {
        if (unlink(ctx->mmap_filename) < 0) {
            error_msg(ctx, ERR_TRACE, "can not delete the old traced file %s(%s)", ctx->mmap_filename, strerror(errno));
            die(ctx, -1);
        }
    }
    /*force to reopen when start a new trace*/
    flag |= 1<<1 ;
    phptrace_ctrl_set(&(ctx->ctrl), flag, ctx->php_pid);
    ctx->trace_flag = flag;
    log_printf (LL_DEBUG, " [ok] set ctrl data: ctrl[pid=%d]=%u is opened!\n", ctx->php_pid, flag);
}

void process_opt_c(phptrace_context_t *ctx)
{
    if (!phptrace_ctrl_init(&(ctx->ctrl))) {
        error_msg(ctx, ERR_CTRL, "cannot open control mmap file %s (%s)",
                  PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME, (errno ? strerror(errno) : "null"));
        die(ctx, -1);
    }
    log_printf (LL_DEBUG, " [ok] ctrl_init open (%s)", PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME);
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
        ctx->mmap_filename = NULL;
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

void usage()
{
    printf ("usage: phptrace [ -chslvw ]  [-p pid] [ -r ]\n\
    -h          -- show this help\n\
    -c          -- clear the trace switches of pid, or all the switches if no pid parameter\n\
    -p pid      -- access the php process i\n\
    -s          -- print call stack of php process by the pid\n\
    -l size     -- specify the max string length to print\n\
    -v          -- print verbose information\n\
    -w outfile  -- dump the trace log to file in original format\n\
    -r infile   -- read the trace file of original format, instead of the process\n\
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

void standard_print_record(phptrace_context_t *ctx, phptrace_file_record_t *r, size_t raw_size)
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
    fprintf (ctx->out_fp, "%s\n", buf);
    sdsfree (buf);
    fflush(NULL);
}

void dump_print_record(phptrace_context_t *ctx, phptrace_file_record_t *r, size_t raw_size)
{
    sds buf;

    buf = sdsempty();
    buf = sdsMakeRoomFor(buf, raw_size);
    phptrace_mem_write_record(r, buf);
    fwrite(buf, sizeof(char), raw_size, (ctx->out_fp));

    log_printf(LL_DEBUG, "[dump] record(sequence=%lld) raw_size=%u", r->seq, raw_size);
    sdsfree(buf);
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

extern int interrupted;

void trace(phptrace_context_t *ctx)
{
    int rc;
    phptrace_file_record_t rcd;
    int state = STATE_OPEN;
    uint64_t flag;
    uint64_t seq = 0;

    sds buf;
    size_t raw_size;
    uint32_t len;
    uint64_t magic_number;
    void *mem_ptr = NULL;
    void *tmp_ptr = NULL;
    int opendata_wait_interval = OPEN_DATA_WAIT_INTERVAL;
    int data_wait_interval = DATA_WAIT_INTERVAL;

    while (1) {
        if (interrupted) {
            break;
        }

        if (!ctx->in_filename) {
            phptrace_ctrl_heartbeat_ping(&(ctx->ctrl), ctx->php_pid);
        }

        if (state != STATE_OPEN) {
            phptrace_mem_read_64b(flag, mem_ptr);
            log_printf (LL_DEBUG, "flag=(%lld) (0x%llx)", flag, flag);

            if (flag == WAIT_FLAG) {                        /* wait flag */
                if (ctx->in_filename) {
                    break;
                }

                log_printf (LL_DEBUG, "  WAIT_FLAG, will wait %d ms.\n", data_wait_interval);
                phptrace_msleep(data_wait_interval);
                data_wait_interval = grow_interval(data_wait_interval, MAX_DATA_WAIT_INTERVAL);
                continue; 
            } else if (flag == MAGIC_NUMBER_TAILER) {       /* check tailer */
                state = STATE_TAILER;
            }
            data_wait_interval = DATA_WAIT_INTERVAL;        /* reset the data wait interval */
        }
        
        switch (state) { 
            case STATE_OPEN:
                rc = update_mmap_filename(ctx);
                if (rc < 0) {
                    error_msg(ctx, ERR_TRACE, "update trace mmap filename failed");
                    die(ctx, -1);
                }

                log_printf(LL_DEBUG, "phptrace_mmap_read '%s' before if", ctx->mmap_filename);
                if (!ctx->seg.shmaddr || ctx->seg.shmaddr == MAP_FAILED) {
                    ctx->seg = phptrace_mmap_read(ctx->mmap_filename);
                    log_printf(LL_DEBUG, "phptrace_mmap_read '%s' after if", ctx->mmap_filename);
                    if (ctx->seg.shmaddr == MAP_FAILED) {
                        if (ctx->in_filename) {             /* -r option, open failed */
                            error_msg(ctx, ERR_TRACE, "Can not open %s to read, %s!", ctx->in_filename, strerror(errno));
                            die(ctx, -1);
                        }

                        if (errno == ENOENT) {              /* file not exist, should wait */
                            log_printf(LL_DEBUG, "trace mmap file not exist, will sleep %d ms.\n", opendata_wait_interval);
                            phptrace_msleep(opendata_wait_interval);
                            opendata_wait_interval = grow_interval(opendata_wait_interval, MAX_OPEN_DATA_WAIT_INTERVAL);
                            continue;
                        } else {
                            log_printf(LL_NOTICE, "phptrace_mmap_read '%s' failed!", ctx->mmap_filename);
                            die(ctx, -1);
                        }
                    }

                }

                /* open tracelog success */
                ctx->rotate_cnt++;
                if (!ctx->in_filename) {
                    unlink(ctx->mmap_filename);
                }
                opendata_wait_interval = OPEN_DATA_WAIT_INTERVAL;
                state = STATE_HEADER;

                mem_ptr = ctx->seg.shmaddr;
                break;
            case STATE_HEADER:
                if (flag != MAGIC_NUMBER_HEADER) {
                    error_msg(ctx, ERR_TRACE, "header's magic number is not correct, trace file may be damaged");
                    die(ctx, -1);
                }
                tmp_ptr = phptrace_mem_read_header(&(ctx->file.header), mem_ptr);
                raw_size = tmp_ptr - mem_ptr;
                mem_ptr = tmp_ptr;

                state = STATE_RECORD;
                log_printf (LL_DEBUG, " [ok load header]");

                if (ctx->rotate_cnt == 1 && ctx->opt_w_flag) {                          /* dump header to out_fp */
                    buf = sdsempty();
                    buf = sdsMakeRoomFor(buf, raw_size);
                    phptrace_mem_write_header(&(ctx->file.header), buf);
                    fwrite(buf, sizeof(char), raw_size, (ctx->out_fp));

                    log_printf(LL_DEBUG, "[dump] header raw_size=%u", raw_size);
                    sdsfree(buf);
                }
                break;
            case STATE_RECORD:
                if (flag != seq) {
                    error_msg(ctx, ERR_TRACE, "sequence number is incorrect, trace file may be damaged");
                    die(ctx, -1);
                }

                tmp_ptr = phptrace_mem_read_record(&(rcd), mem_ptr, flag);
                if (!tmp_ptr) {
                    error_msg(ctx, ERR_TRACE, "read record failed, maybe write too fast");
                    die(ctx, -1);
                }
                raw_size = tmp_ptr - mem_ptr;
                mem_ptr = tmp_ptr;

                ctx->record_printer(ctx, &(rcd), raw_size);
                phptrace_record_free(&(rcd));
                seq++;
                break;
            case STATE_TAILER:
                if (flag != MAGIC_NUMBER_TAILER) {
                    error_msg(ctx, ERR_TRACE, "tailer's magic number is not correct, trace file may be damaged");
                    die(ctx, -1);
                }

                if (ctx->file.tailer.filename) {                    /* free tailer.filename */
                    sdsfree(ctx->file.tailer.filename);
                    ctx->file.tailer.filename = NULL;
                }
                tmp_ptr = phptrace_mem_read_tailer(&(ctx->file.tailer), mem_ptr);
                raw_size = tmp_ptr - mem_ptr;
                mem_ptr = tmp_ptr;

                state = STATE_OPEN;
                log_printf (LL_DEBUG, " [ok load tailer]");

                if (ctx->in_filename) {                             /* -r option, read from file */
                    goto TRACE_END;
                }

                break;
        }
    }

TRACE_END:
    if (ctx->opt_w_flag) {                                          /* dump empty tailer to out_fp */
        len = 0;
        magic_number = MAGIC_NUMBER_TAILER;
        fwrite(&magic_number, sizeof(uint64_t), 1, ctx->out_fp);
        fwrite(&len, sizeof(uint32_t), 1, ctx->out_fp);

        flush(NULL);
        log_printf(LL_DEBUG, "[dump] empty tailer");
    }

    die(ctx, 0);
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

