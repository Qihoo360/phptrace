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

#include "trace_status.h"

int status_dump_once(pt_context_t* ctx)
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
        ctx->status_deep++;
        if (ctx->status_deep >= MAX_STACK_DEEP) {
            break;
        }
    }

    return 0;
}

int status_dump_ptrace(pt_context_t* ctx)
{
    int ret;
    ret = 0;
    while (1) {
        if ((ret = status_dump_once(ctx)) >= 0) {
            return 0;
        }

        log_printf(LL_NOTICE, "status_dump_once failed: %d", ctx->php_pid);
        sleep(1);

        ctx->retry++;
        if (ctx->retry >= MAX_RETRY) {
            break;
        }
    }
    return -1;
}

int check_phpext_installed(pt_context_t *ctx)
{
    if (access(PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME, R_OK|W_OK) < 0) {
        if(errno != ENOENT) {
            log_printf(LL_ERROR, "check_phpext_installed: %s", strerror(errno));
        }
        return 0;
    }
    return 1;
}

sds type_dump_frame(pt_frame_t *f)
{
    int i;
    sds buf = sdsempty();

    buf = pt_repr_function(buf, f);

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

sds type_dump_status(pt_status_t *st)
{
    int i;
    sds buf = sdsempty();
    sds s_f;

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
        s_f = type_dump_frame(&(st->frames[i]));
        buf = sdscatsds(buf, s_f);
        sdsfree(s_f);
    }
    return buf;
}

void type_status_free(pt_status_t *st)
{
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

int status_dump(pt_context_t *ctx, int timeout /*milliseconds*/)
{
    char filename[256];
    uint32_t type;
    sds buf;
    int opendata_wait_interval = OPEN_DATA_WAIT_INTERVAL;
    int data_wait_interval = DATA_WAIT_INTERVAL;

    sprintf(filename, PHPTRACE_LOG_DIR "/" PT_COMM_FILENAME ".%d", ctx->php_pid);
    log_printf(LL_DEBUG, "dump status %s", filename);

    /* new protocol API */
    pt_comm_message_t *msg;
    pt_status_t st;
    pt_comm_socket_t sock;

    while (pt_comm_sopen(&sock, filename, 1) < 0) {    /* meta: recv|send  */
        if (interrupted) {
            goto status_end;
        }

        if (timeout < 0 || errno != ENOENT) {                      /* -r option or open failed (except for non-exist) */
            error_msg(ctx, ERR_TRACE, "Can not open %s to read, %s!", filename, strerror(errno));
            goto status_end;
        } else {                                                        /* file not exist, should wait */
            log_printf(LL_DEBUG, "trace mmap file not exist, will sleep %d ms.\n", opendata_wait_interval);
            timeout -= opendata_wait_interval;
            pt_msleep(opendata_wait_interval);
            opendata_wait_interval = grow_interval(opendata_wait_interval, MAX_OPEN_DATA_WAIT_INTERVAL);
        }
    }

    unlink(ctx->mmap_filename);
    pt_comm_swrite(&sock, PT_MSG_DO_STATUS, NULL, 0);

    while (!interrupted && timeout > 0) {
        pt_comm_swrite(&sock, PT_MSG_DO_PING, NULL, 0);           /* send to do ping */

        type = pt_comm_sread_type(&sock);
        log_printf (LL_DEBUG, "msg type=(%u)", type);

        if (type == PT_MSG_EMPTY) {                         /* wait flag */
            log_printf (LL_DEBUG, "  wait_type will wait %d ms.\n", data_wait_interval);
            timeout -= data_wait_interval;
            pt_msleep(data_wait_interval);
            data_wait_interval = grow_interval(data_wait_interval, MAX_DATA_WAIT_INTERVAL);
            continue;
        }

        if (pt_comm_sread(&sock, &msg, 1) == PT_MSG_INVALID) {
            error_msg(ctx, ERR_TRACE, "read record failed, maybe write too fast");
            break;
        }

        if (msg->type == PT_MSG_RET) {
            pt_type_unpack_status(&st, msg->data);
            buf = type_dump_status(&st);
            fwrite(buf, sizeof(char), sdslen(buf), ctx->out_fp);
            fflush(NULL);
            sdsfree(buf);
        }

        type_status_free(&st);
        break;
    }
    return 0;

status_end:
    if (timeout <= 0) {
        log_printf(LL_DEBUG, "dump status failed: timedout(%d)", timeout);
    }
    return -1;
}

void process_opt_s(pt_context_t *ctx)
{
    int ret;

    /*dump stauts from the extension*/
    if (!ctx->addr_info.sapi_globals_addr && check_phpext_installed(ctx)) {
        log_printf(LL_DEBUG, "pt extension has been installed, use extension\n");
        if (pt_ctrl_open(&(ctx->ctrl), PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME) < 0) {
            error_msg(ctx, ERR_CTRL, "cannot open control mmap file %s (%s)",
                    PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME, (errno ? strerror(errno) : "null"));
            die(ctx, -1);
        }

        ctx->trace_flag = 1;
        if (pt_ctrl_is_active(&(ctx->ctrl), ctx->php_pid)) {
            error_msg(ctx, ERR_CTRL, "process %d is being dumped by others, please retry later", ctx->php_pid);
            die(ctx, -1);
        }
        pt_ctrl_set_active(&(ctx->ctrl), ctx->php_pid);
        if (status_dump(ctx, 3000) == 0){
            die(ctx, 0);
        }
        /*clear the flag if dump failed*/
        pt_ctrl_set_inactive(&(ctx->ctrl), ctx->php_pid);
        die(ctx, -1);
    }

    /*dump status without extension if above failed*/
    log_printf(LL_DEBUG, "pt extension was not been installed, use ptrace\n");
    if (sys_trace_attach(ctx->php_pid)) {
        log_printf(LL_NOTICE, "sys_trace_attach failed");
        error_msg(ctx, ERR_STACK, "sys_trace_attach failed (%s)", (errno ? strerror(errno) : "null"));
        return;
    }

    ret = status_dump_ptrace(ctx);
    if (ret < 0) {
        error_msg(ctx, ERR_STACK, "dump status failed. maybe the process is not executing a php script");
    }

    if (sys_trace_detach(ctx->php_pid) < 0) {
        log_printf(LL_NOTICE, "sys_trace_detach failed");
        error_msg(ctx, ERR_STACK, "sys_trace_detach failed (%s)", (errno ? strerror(errno) : "null"));
    }

    sys_trace_kill(ctx->php_pid, SIGCONT);
}
