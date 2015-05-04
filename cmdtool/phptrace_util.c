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
    "COUNT ERROR"
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
    log_printf (LL_DEBUG, " [trace_cleanup]");

    if (ctx->trace_flag) {
        phptrace_ctrl_clean_one(&(ctx->ctrl), ctx->php_pid);
        log_printf (LL_DEBUG, " clean ctrl pid=%d~~~~~\n", ctx->php_pid);
    }

    if (ctx->exclusive_flag) {
        if (ctx->sub_cost_time) {
            free(ctx->sub_cost_time);
        }
        if (ctx->sub_cpu_time) {
            free(ctx->sub_cpu_time);
        }
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

    ctx->max_level = DEFAULT_MAX_LEVEL;

    ctx->max_print_len = MAX_PRINT_LENGTH;
    //ctx->seg.shmaddr = MAP_FAILED;

    ctx->record_transform = standard_transform;
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
    log_printf (LL_DEBUG, " [trace_start] ctrl_init open (%s) successful", PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME);

    phptrace_ctrl_get(&(ctx->ctrl), &num, ctx->php_pid);
    if (num > 0) {
        error_msg(ctx, ERR_CTRL, "[trace_start] process %d has been traced", ctx->php_pid);
        die(ctx, -1);
    }
    log_printf (LL_DEBUG, "[trace_start] read ctrl data ok: ctrl[pid=%d]=%d!", ctx->php_pid, num);

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
    log_printf (LL_DEBUG, "[trace_start] set ctrl data ok: ctrl[pid=%d]=%u is opened!", ctx->php_pid, flag);
}

void process_opt_e(phptrace_context_t *ctx)
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

void usage()
{
    printf ("usage: phptrace [-hvvv]  [-p pid] [--cleanup]\n\
            -h                   -- show this help\n\
            --cleanup            -- cleanup the trace switches of pid, or all the switches if no pid parameter\n\
            -p pid               -- access the php process pid\n\
            -v                   -- print verbose information\n\
            \n");
}

sds print_indent_str(sds s, char* str, int32_t size)
{
    int32_t i;
    for (i = 0; i < size; i++)
        s = sdscatprintf (s, "%s", str);
    return s;
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

sds phptrace_repr_function(sds buf, phptrace_frame *f)
{
    if ((f->functype & PT_FUNC_TYPES) == PT_FUNC_NORMAL) {
        buf = sdscatprintf (buf, "%s", f->function);
    } else if ((f->functype & PT_FUNC_TYPES) == PT_FUNC_MEMBER) {
        buf = sdscatprintf (buf, "%s->%s", f->class, f->function);
    } else if ((f->functype & PT_FUNC_TYPES) == PT_FUNC_STATIC) {
        buf = sdscatprintf (buf, "%s::%s", f->class, f->function);
    } else if (f->functype & PT_FUNC_TYPES & PT_FUNC_INCLUDES) {
        buf = sdscatprintf (buf, "%s", f->function);
    } else {
        buf = sdscatprintf (buf, "unknown");
    }
    return buf;
}

sds standard_transform(phptrace_context_t *ctx, phptrace_frame *f)
{
    int i;
    sds buf = sdsempty();

    if (f->type == PT_FRAME_ENTRY) {
        buf = sdscatprintf (buf, "%lld.%06d", (long long)(f->entry.wall_time / 1000000), (int)(f->entry.wall_time % 1000000));   
        buf = print_indent_str (buf, "  ", f->level + 1);
        buf = phptrace_repr_function(buf, f);

        if ((f->functype & PT_FUNC_TYPES & PT_FUNC_INCLUDES) == 0) {
            buf = sdscatprintf (buf, "(");
        }

        if (f->arg_count) {
            for (i = 0; i < f->arg_count; i++) {
                buf = sdscatprintf (buf, "%s", f->args[i]);
                if (i < f->arg_count - 1) {
                    buf = sdscatprintf (buf, ", ");
                }
            }
        }

        if ((f->functype & PT_FUNC_TYPES & PT_FUNC_INCLUDES) == 0) {
            buf = sdscatprintf (buf, ")");
        }
        buf = sdscatprintf (buf, " at [%s:%u]\n", f->filename, f->lineno);
    } else if (f->type == PT_FRAME_EXIT) {
        buf = sdscatprintf (buf, "%lld.%06d", (long long)(f->exit.wall_time / 1000000), (int)(f->exit.wall_time % 1000000));   
        buf = print_indent_str (buf, "  ", f->level + 1);
        buf = phptrace_repr_function(buf, f);
        buf = sdscatprintf (buf, "  =>  ");
        if (f->retval) {
            buf = sdscatprintf (buf, "%s", f->retval);
        }

        buf = sdscatprintf(buf, "   wt: %.6f ct: %.6f\n", 
                ((f->exit.wall_time - f->entry.wall_time) / 1000000.0),
                ((f->exit.cpu_time - f->entry.cpu_time) / 1000000.0));
    }

    log_printf (LL_DEBUG, " record_sds[%s]", buf);
    return buf;
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
    int state = STATE_OPEN;
    uint64_t seq = 0;

    sds buf;
    uint32_t type;
    int opendata_wait_interval = OPEN_DATA_WAIT_INTERVAL;
    int data_wait_interval = DATA_WAIT_INTERVAL;

    /* new protocol API */
    phptrace_comm_socket sock;
    phptrace_comm_message *msg;
    phptrace_frame frame;

    memset(&sock, 0, sizeof(sock));

    /* exclusive time mode */
    if (ctx->exclusive_flag) {
        ctx->sub_cost_time = calloc(ctx->max_level + 2, sizeof(int64_t));
        ctx->sub_cpu_time = calloc(ctx->max_level + 2, sizeof(int64_t));
    }

    while (phptrace_comm_sopen(&sock, ctx->mmap_filename, 1) < 0) {
        if (interrupted) {
            return;
        }

        if (ctx->in_filename || errno != ENOENT) {             /* -r option or open failed (except for non-exist) */
            error_msg(ctx, ERR_TRACE, "Can not open %s to read, %s!", ctx->mmap_filename, strerror(errno));
            die(ctx, -1);
        } else {                                                /* file not exist, should wait */
            log_printf(LL_DEBUG, "trace mmap file not exist, will sleep %d ms.\n", opendata_wait_interval);
            phptrace_msleep(opendata_wait_interval);
            opendata_wait_interval = grow_interval(opendata_wait_interval, MAX_OPEN_DATA_WAIT_INTERVAL);
        }
    }

    ctx->rotate_cnt++;
    if (!ctx->in_filename) {
        //unlink(ctx->mmap_filename);
    }
    state = STATE_HEADER;

    if (!ctx->in_filename) {                                            /* not -r option */
        phptrace_comm_swrite(&sock, PT_MSG_DO_TRACE, NULL, 0);
    }

    while (1) {
        if (interrupted) {
            break;
        }

        if (state == STATE_END) {
            break;
        }

        if (!ctx->in_filename) {
            phptrace_comm_swrite(&sock, PT_MSG_DO_PING, NULL, 0);           /* send to do ping */
        }

        type = phptrace_comm_sread_type(sock);
        log_printf (LL_DEBUG, "msg type=(%u)", type);

        if (type == PT_MSG_EMPTY) {                         /* wait flag */
            if (ctx->in_filename) {
                break;
            }

            log_printf (LL_DEBUG, "  wait_type will wait %d ms.\n", data_wait_interval);
            phptrace_msleep(data_wait_interval);
            data_wait_interval = grow_interval(data_wait_interval, MAX_DATA_WAIT_INTERVAL);
            continue; 
        }
        //else if (flag == MAGIC_NUMBER_TAILER) {       /* check tailer */
        //   state = STATE_TAILER;
        //}

        data_wait_interval = DATA_WAIT_INTERVAL;            /* reset the data wait interval */

        msg = phptrace_comm_sread(&sock);

        switch (msg->type) {
            case PT_MSG_RET:    /* @TODO  data type */
                phptrace_type_unpack_frame(&frame, msg->data);


                buf = ctx->record_transform(ctx, &(frame));
                fwrite(buf, sizeof(char), sdslen(buf), ctx->out_fp);
                fflush(NULL);
                sdsfree(buf);

                seq++;
                break;
        }

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

int check_phpext_installed(phptrace_context_t *ctx)
{
    if (access(PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME, R_OK|W_OK) < 0) {
        if(errno != ENOENT) {
            log_printf(LL_ERROR, "check_phpext_installed: %s", strerror(errno));
        }
        return 0;
    }
    return 1;
}
int status_dump(phptrace_context_t *ctx, int timeout /*milliseconds*/)
{
    int wait;
    FILE *fp;
    char filename[256];
    char buf[256];
    int  blen = sizeof(buf);
    int  nread;
    wait = timeout < OPEN_DATA_WAIT_INTERVAL ? timeout : OPEN_DATA_WAIT_INTERVAL;
    sprintf(filename, PHPTRACE_LOG_DIR "/" PHPTRACE_STATUS_FILENAME ".%d", ctx->php_pid);
    log_printf(LL_DEBUG, "dump status %s", filename);
    fp = NULL;
    while (!interrupted) {
        fp = fopen(filename, "r");
        if (fp == NULL) {
            if (errno != ENOENT) {
                log_printf(LL_ERROR, "open status file %s: %s", filename, strerror(errno));
                return -1;
            }
            if (timeout > 0) {
                log_printf(LL_DEBUG, "open status file %s: %s", filename, strerror(errno));
                timeout -= wait;
                phptrace_msleep(wait);
                wait = grow_interval(wait, MAX_OPEN_DATA_WAIT_INTERVAL);
                continue;
            }
        }
        break;
    }
    /*timedout*/
    if (fp == NULL) {
        if (timeout <=0 ) {
            log_printf(LL_DEBUG, "dump status failed: timedout(%d)", timeout);
        } else {
            log_printf(LL_DEBUG, "dump status failed: signal interrupt");
        }
        return -1;
    }
    unlink(filename);
    /*success*/
    while (! feof(fp)) {
        nread = fread(buf, 1, blen, fp);
        fwrite(buf, nread, 1, stdout);
    }
    fclose(fp);
    return 0;
}

void process_opt_s(phptrace_context_t *ctx)
{
    int ret;
    int8_t num;

    num = -1;

    /*dump stauts from the extension*/
    if (!ctx->addr_info.sapi_globals_addr && check_phpext_installed(ctx)) {
        log_printf(LL_DEBUG, "phptrace extension has been installed, use extension\n");
        if (!phptrace_ctrl_init(&(ctx->ctrl))) {
            error_msg(ctx, ERR_CTRL, "cannot open control mmap file %s (%s)",
                    PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME, (errno ? strerror(errno) : "null"));
            die(ctx, -1);
        }

        phptrace_ctrl_get(&(ctx->ctrl), &num, ctx->php_pid);
        if (num > 0) {
            error_msg(ctx, ERR_CTRL, "process %d is being dumped by others, please retry later", ctx->php_pid);
            die(ctx, -1);
        }
        phptrace_ctrl_set(&(ctx->ctrl), 1<<2, ctx->php_pid);
        if (status_dump(ctx, 500) == 0){
            return;
        }
        /*clear the flag if dump failed*/
        phptrace_ctrl_set(&(ctx->ctrl), 0, ctx->php_pid);
        die(ctx, -1);
    }

    /*dump stack without extension if above failed*/
    log_printf(LL_DEBUG, "phptrace extension was not been installed, use ptrace\n");
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

