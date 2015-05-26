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

#if defined(__APPLE__) && defined(__MACH__) /* darwin */
#include "libproc.h"
#endif

static address_info_t address_templates[] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 64, 0, 968, 0, 16, 0, 8, 112, 64, 0, 168, 0, 112},      /* 5.2 */
    {0, 64, 0, 1360, 0, 8, 0, 8, 80, 40, 0, 168, 0, 0, 112},    /* 5.3 */
    {0, 64, 0, 1152, 0, 8, 0, 8, 80, 40, 0, 144, 0, 0, 40},     /* 5.4 */
    {0, 64, 0, 1120, 0, 8, 0, 8, 48, 24, 0, 152, 0, 0, 40},     /* 5.5 */
    {0, 40, 0, 1120, 0, 8, 0, 8, 48, 24, 0, 152, 0, 0, 40},     /* 5.6 */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

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
    /* TODO check exactly */
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
    uint32_t type;
    sds buf;
    int data_wait_interval = DATA_WAIT_INTERVAL;

    /* new protocol API */
    pt_comm_message_t *msg;
    pt_status_t st;

    while (!interrupted && timeout > 0) {
        pt_comm_swrite(&ctx->sock, PT_MSG_DO_PING, NULL, 0);           /* send to do ping */

        type = pt_comm_sread_type(&ctx->sock);
        log_printf (LL_DEBUG, "msg type=(%u)", type);

        if (type == PT_MSG_EMPTY) {                         /* wait flag */
            log_printf (LL_DEBUG, "  wait_type will wait %d ms.\n", data_wait_interval);
            timeout -= data_wait_interval;
            pt_msleep(data_wait_interval);
            data_wait_interval = grow_interval(data_wait_interval, MAX_DATA_WAIT_INTERVAL);
            continue;
        }

        if (pt_comm_sread(&ctx->sock, &msg, 1) == PT_MSG_INVALID) {
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

    if (timeout <= 0) {
        log_printf(LL_DEBUG, "dump status failed: timedout(%d)", timeout);
        return -1;
    } else {
        return 0;
    }
}

int try_ext(pt_context_t *ctx)
{
    char filename[PATH_MAX];

    if (pt_ctrl_open(&(ctx->ctrl), PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME) < 0) {
        error_msg(ctx, ERR_CTRL, "cannot open control mmap file %s (%s)",
                PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME, (errno ? strerror(errno) : "null"));
        return -1;
    }

    ctx->trace_flag = 1;
    if (pt_ctrl_is_active(&(ctx->ctrl), ctx->php_pid)) {
        error_msg(ctx, ERR_CTRL, "process %d is being dumped by others, please retry later", ctx->php_pid);
        return -1;
    }

    /* create comm socket before set active */
    sprintf(filename, PHPTRACE_LOG_DIR "/" PT_COMM_FILENAME ".%d", ctx->php_pid);
    log_printf(LL_DEBUG, "dump status %s", filename);
    if (pt_comm_screate(&ctx->sock, filename, 1, PT_COMM_E2T_SIZE, PT_COMM_T2E_SIZE) < 0) {
        return -1;
    }

    /* write DO_STATUS op then set active */
    pt_comm_swrite(&ctx->sock, PT_MSG_DO_STATUS, NULL, 0);
    pt_ctrl_set_active(&(ctx->ctrl), ctx->php_pid);

    if (status_dump(ctx, 3000) == 0){
        return 0;
    }

    /*clear the flag if dump failed*/
    pt_ctrl_set_inactive(&(ctx->ctrl), ctx->php_pid);

    return -1;
}

int fetch_php_bin(pid_t pid, char *bin, size_t bin_len)
{
#if defined(__APPLE__) && defined(__MACH__) /* Darwin */
    if (proc_pidpath(pid, bin, bin_len) == 0) {
        return -1;
    }
#elif defined(__linux__) /* Linux */
    int rlen;
    char path[128];

    sprintf(path, "/proc/%d/exe", pid);
    rlen = readlink(path, bin, bin_len);
    if (rlen == -1 || rlen >= bin_len) {
        return -1;
    }
    bin[rlen] = '\0';
#else
    char cmd[128];
    FILE *fp;

    /* prepare command, requirement: lsof, awk, head */
    sprintf(cmd, "lsof -p %d | awk '$4 == \"txt\" {print $NF}' | head -n1", pid);

    /* open process */
    log_printf(LL_DEBUG, "popen cmd: %s", cmd);
    if ((fp = popen(cmd, "r")) == NULL) {
        log_printf(LL_DEBUG, "popen failed");
        return -1;
    }

    /* read output */
    if (fgets(bin, bin_len, fp) == NULL) {
        log_printf(LL_DEBUG, "popen fgets failed");
        pclose(fp);
        return -1;
    }
    bin[strlen(bin) - 1] = '\0'; /* strip '\n' */
    log_printf(LL_DEBUG, "popen fgets: %s", bin);
    pclose(fp);
#endif

    return 0;
}

int fetch_php_versionid(const char *bin)
{
    FILE *fp;
    char buf[PATH_MAX];
    int version_id;

    /* prepare command, requirement: awk */
    sprintf(buf, "%s -v | awk 'NR == 1 {print $2}'", bin);

    /* open process */
    log_printf(LL_DEBUG, "popen cmd: %s", buf);
    if ((fp = popen(buf, "r")) == NULL) {
        log_printf(LL_DEBUG, "popen failed");
        return -1;
    }

    /* read output */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        log_printf(LL_DEBUG, "popen fgets failed");
        pclose(fp);
        return -1;
    }
    log_printf(LL_DEBUG, "popen fgets: %s", buf);
    pclose(fp);

    /* convert version string "5.6.3" to id 50603 */
    if (strlen(buf) < 5 || buf[0] != '5' || buf[1] != '.' || buf[3] != '.') {
        return -1;
    }
    version_id = 50000;
    version_id += (buf[2] - '0') * 100;
    version_id += (buf[4] - '0') * 10;
    version_id += (buf[5] - '0') * 1;

    return version_id;
}

int fetch_php_address(const char *bin, pid_t pid, long *addr_sg, long *addr_eg)
{
    FILE *fp;
    char buf[PATH_MAX];
    long addr;

    /* prepare command, requirement: gdb, awk */
    sprintf(buf, "gdb --batch -nx %s %d "
                 "-ex 'print &(sapi_globals)' "
                 "-ex 'print &(executor_globals)' "
                 "2>/dev/null | awk '$1 ~ /^\\$[0-9]/ {print $NF}'", bin, pid);

    /* open process */
    log_printf(LL_DEBUG, "popen cmd: %s", buf);
    if ((fp = popen(buf, "r")) == NULL) {
        log_printf(LL_DEBUG, "popen failed");
        return -1;
    }

    /* sapi_globals */
    addr = 0;
    if (fgets(buf, sizeof(buf), fp) == NULL || (addr = hexstring2long(buf, strlen(buf) - 1)) == -1) { /* strip '\n' */
        if (addr != -1) {
            log_printf(LL_DEBUG, "popen fgets failed");
        } else {
            log_printf(LL_DEBUG, "convert hex string to long failed \"%s\"", buf);
        }
        pclose(fp);
        return -1;
    }
    log_printf(LL_DEBUG, "popen fgets: %s", buf);
    *addr_sg = addr;

    /* executor_globals */
    if (fgets(buf, sizeof(buf), fp) == NULL || (addr = hexstring2long(buf, strlen(buf) - 1)) == -1) {
        /* TODO it's totally duplicated code here, refactor it if we want
         * process more address */
        if (addr != -1) {
            log_printf(LL_DEBUG, "popen fgets failed");
        } else {
            log_printf(LL_DEBUG, "convert hex string to long failed \"%s\"", buf);
        }
        pclose(fp);
        return -1;
    }
    log_printf(LL_DEBUG, "popen fgets: %s", buf);
    *addr_eg = addr;

    pclose(fp);
    return 0;
}

int try_ptrace(pt_context_t *ctx)
{
    char php_bin[PATH_MAX];
    int php_version_id;

    /* Fetch PHP bin file */
    if (fetch_php_bin(ctx->php_pid, php_bin, sizeof(php_bin)) == -1) {
        error_msg(ctx, ERR_STACK, "fetch PHP bin file failed");
        return -1;
    }

    /* Fetch PHP version */
    php_version_id = fetch_php_versionid(php_bin);
    if (php_version_id == -1) {
        error_msg(ctx, ERR_STACK, "fetch PHP version failed");
        return -1;
    }
    ctx->php_version = php_version_id / 100 % 10; /* extract minor version */
    log_printf(LL_DEBUG, "PHP version: %d minor: %d", php_version_id, ctx->php_version);

    /* Set address info */
    if (ctx->php_version && (ctx->php_version < PHP52 || ctx->php_version > PHP56)) {
        error_msg(ctx, ERR_STACK, "PHP version id %d not supported", php_version_id);
        return -1;
    }
    memcpy(&(ctx->addr_info), &address_templates[ctx->php_version], sizeof(address_info_t));

    /* Fetch globals address */
    if (fetch_php_address(php_bin, ctx->php_pid,
                &ctx->addr_info.sapi_globals_addr,
                &ctx->addr_info.executor_globals_addr) == -1) {
        error_msg(ctx, ERR_STACK, "fetch PHP globals address failed");
        return -1;
    }
    log_printf(LL_DEBUG, "sapi_globals_addr: 0x%lx", ctx->addr_info.sapi_globals_addr);
    log_printf(LL_DEBUG, "executor_globals_addr: 0x%lx\n", ctx->addr_info.executor_globals_addr);

    if (sys_trace_attach(ctx->php_pid)) {
        log_printf(LL_NOTICE, "sys_trace_attach failed");
        error_msg(ctx, ERR_STACK, "sys_trace_attach failed (%s)", (errno ? strerror(errno) : "null"));
        return -1;
    }

    if (status_dump_ptrace(ctx) < 0) {
        error_msg(ctx, ERR_STACK, "dump status failed. maybe the process is not executing a php script");

        /* detach after ptrace failed, the process may be killed if we won't
         * detach in OS X */
        sys_trace_detach(ctx->php_pid);

        return -1;
    }

    if (sys_trace_detach(ctx->php_pid) < 0) {
        log_printf(LL_NOTICE, "sys_trace_detach failed");
        error_msg(ctx, ERR_STACK, "sys_trace_detach failed (%s)", (errno ? strerror(errno) : "null"));
        return -1;
    }

    sys_trace_kill(ctx->php_pid, SIGCONT);
    return 0;
}

void process_opt_s(pt_context_t *ctx)
{
    /*dump stauts from the extension*/
    if (check_phpext_installed(ctx)) {
        log_printf(LL_DEBUG, "phptrace extension has been installed, use extension\n");
        if (try_ext(ctx) == 0) {
            die(ctx, 0);
            return;
        }
    }

    /*dump status without extension if above failed*/
    log_printf(LL_DEBUG, "phptrace extension was not been installed, use ptrace\n");
    if (try_ptrace(ctx) == 0) {
        die(ctx, 0);
        return;
    }

    die(ctx, -1);
}
