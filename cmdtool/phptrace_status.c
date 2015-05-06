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

//extern int interrupted;

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
