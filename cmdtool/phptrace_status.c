#include "phptrace_status.h"

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

sds type_dump_frame(phptrace_frame *f)
{
    int i;
    sds buf = sdsempty();

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
    log_printf (LL_DEBUG, " record_sds[%s]", buf);
    return buf;
}

sds type_dump_status(phptrace_status *st)
{
    int i;
    sds buf = sdsempty();

    buf = sdscatprintf(buf, "Memory\nusage: %"PRId64"\npeak_usage: %"PRId64"\nreal_usage: %"PRId64"\nreal_peak_usage: %"PRId64"\n\n",
            st->mem, st->mempeak, st->mem_real, st->mempeak_real);

    buf = sdscatprintf(buf, "Request\n");
    if (st->request_method) {
        buf = sdscatprintf (buf, "method: %s\n", st->request_method);
    }
    if (st->request_uri) {
        buf = sdscatprintf (buf, "request_uri: %s\n", st->request_uri);
    }
    if (st->request_query) {
        buf = sdscatprintf (buf, "request_query: %s\n", st->request_query);
    }
    if (st->request_script) {
        buf = sdscatprintf (buf, "request_script: %s\n", st->request_script);
    }
    buf = sdscatprintf (buf, "request_time: %f\n\nStack\n", st->request_time);

    for (i = 0; i < st->frame_count; i++) {
        buf = sdscatprintf (buf, "#%-4d ", i + 1);
        buf = sdscatsds(buf, type_dump_frame(&(st->frames[i])));
    }
    return buf;
}

void type_status_free(phptrace_status *st)
{
   // sds php_version;            /* php version eg: 5.5.24 */
   // sds sapi_name;              /* sapi name eg: fpm-fcgi */
   //
  //  sds request_method;         /* [optional] */
  //  sds request_uri;            /* [optional] */
  //  sds request_query;          /* [optional] */
  //  sds request_script;         /* [optional] */

  //  int argc;                   /* arguments part, available in cli */
  //  sds *argv;

  //  int proto_num;

  //  uint32_t frame_count;       /* backtrace depth */
  //  phptrace_frame *frames;     /* backtrace frames */
    int i;
    sdsfree(st->php_version);
    sdsfree(st->sapi_name);
    sdsfree(st->request_method);
    sdsfree(st->request_uri);
    sdsfree(st->request_query);
    sdsfree(st->request_script);

    if (st->argc) {
        for (i = 0; i < st->argc; i++) {
            sdsfree(st->argv[i]);
        }
        free(st->argv);
    }

    if (st->frame_count) {
        for (i = 0; i < st->frame_count; i++) {
            frame_free_sds(&st->frames[i]);
        }
        free(st->frames);
    }
}

int status_dump(phptrace_context_t *ctx, int timeout /*milliseconds*/)
{
    char filename[256];
    uint32_t type;
    sds buf;
    int opendata_wait_interval = OPEN_DATA_WAIT_INTERVAL;
    int data_wait_interval = DATA_WAIT_INTERVAL;

    sprintf(filename, PHPTRACE_LOG_DIR "/" PHPTRACE_COMM_FILENAME ".%d", ctx->php_pid);
    log_printf(LL_DEBUG, "dump status %s", filename);

    /* new protocol API */
    phptrace_comm_message *msg;
    phptrace_status st;
    phptrace_comm_socket sock;

    while (phptrace_comm_sopen(&sock, filename, 1) < 0) {    /* meta: recv|send  */
        if (interrupted) {
            goto status_end;
        }

        if (timeout < 0 || errno != ENOENT) {                      /* -r option or open failed (except for non-exist) */
            error_msg(ctx, ERR_TRACE, "Can not open %s to read, %s!", filename, strerror(errno));
            goto status_end;
        } else {                                                        /* file not exist, should wait */
            log_printf(LL_DEBUG, "trace mmap file not exist, will sleep %d ms.\n", opendata_wait_interval);
            timeout -= opendata_wait_interval;
            phptrace_msleep(opendata_wait_interval);
            opendata_wait_interval = grow_interval(opendata_wait_interval, MAX_OPEN_DATA_WAIT_INTERVAL);
        }
    }

    unlink(ctx->mmap_filename);
    phptrace_comm_swrite(&sock, PT_MSG_DO_STATUS, NULL, 0);

    while (!interrupted && timeout > 0) {
        phptrace_comm_swrite(&sock, PT_MSG_DO_PING, NULL, 0);           /* send to do ping */

        type = phptrace_comm_sread_type(&sock);
        log_printf (LL_DEBUG, "msg type=(%u)", type);

        if (type == PT_MSG_EMPTY) {                         /* wait flag */
            log_printf (LL_DEBUG, "  wait_type will wait %d ms.\n", data_wait_interval);
            timeout -= data_wait_interval;
            phptrace_msleep(data_wait_interval);
            data_wait_interval = grow_interval(data_wait_interval, MAX_DATA_WAIT_INTERVAL);
            continue;
        }

        if ((msg = phptrace_comm_sread(&sock)) == NULL) {
            error_msg(ctx, ERR_TRACE, "read record failed, maybe write too fast");
            break;
        }

        if (msg->type == PT_MSG_RET) {
            phptrace_type_unpack_status(&st, msg->data);
            buf = type_dump_status(&st);
            fwrite(buf, sizeof(char), sdslen(buf), ctx->out_fp);
            fflush(NULL);
            sdsfree(buf);
        }

        type_status_free(&st);
        //free(msg);
        break;
    }
    return 0;

status_end:
    /*timedout*/
    if (timeout <=0 ) {
        log_printf(LL_DEBUG, "dump status failed: timedout(%d)", timeout);
    }
    return -1;
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
        phptrace_ctrl_set(&(ctx->ctrl), 1, ctx->php_pid);
        if (status_dump(ctx, 3000) == 0){
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
