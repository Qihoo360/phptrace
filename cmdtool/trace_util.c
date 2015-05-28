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

#include "trace_util.h"
#include "trace_count.h"

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

void trace_cleanup(pt_context_t *ctx)
{
    log_printf (LL_DEBUG, " [trace_cleanup]");

    if (ctx->trace_flag) {
        pt_ctrl_set_inactive(&(ctx->ctrl), ctx->php_pid);
        log_printf (LL_DEBUG, " clean ctrl pid=%d~~~~~\n", ctx->php_pid);
    }

    if (ctx->sock.active) {
        if (ctx->in_filename) {
            pt_comm_sclose(&ctx->sock, 0);
        } else {
            pt_comm_sclose(&ctx->sock, 1);
        }
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

void verror_msg(pt_context_t *ctx, int err_code, const char *fmt, va_list p)
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

void error_msg(pt_context_t *ctx, int err_code, const char *fmt, ...)
{
    va_list p;
    va_start(p, fmt);
    verror_msg(ctx, err_code, fmt, p);
    va_end(p);
}

void die(pt_context_t *ctx, int exit_code)
{
    trace_cleanup(ctx);
    exit(exit_code);
}

void pt_context_init(pt_context_t *ctx)
{
    memset(ctx, 0, sizeof(pt_context_t));

    ctx->php_pid = -1;
    ctx->start_time = pt_time_usec();
    ctx->log = stdout;
    ctx->mmap_filename = NULL;

    ctx->in_filename = NULL;
    ctx->out_fp = stdout;
    ctx->out_filename = NULL;

    ctx->max_level = DEFAULT_MAX_LEVEL;

    ctx->max_print_len = MAX_PRINT_LENGTH;

    ctx->sock.active = 0; /* init comm socket */

    ctx->record_transform = standard_transform;
    log_level_set(LL_ERROR + 1);
}

void trace_start(pt_context_t *ctx)
{
    uint8_t flag = 1;
    struct stat st;

    if (pt_ctrl_open(&(ctx->ctrl), PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME) < 0) {
        error_msg(ctx, ERR_CTRL, "cannot open control mmap file %s (%s)", 
                PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME, (errno ? strerror(errno) : "null"));
        die(ctx, -1);
    }
    log_printf (LL_DEBUG, " [trace_start] ctrl_init open (%s) successful", PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME);

    if (pt_ctrl_is_active(&(ctx->ctrl), ctx->php_pid)) {
        error_msg(ctx, ERR_CTRL, "[trace_start] process %d has been traced", ctx->php_pid);
        die(ctx, -1);
    }
    log_printf (LL_DEBUG, "[trace_start] read ctrl data ok: ctrl[pid=%d]=%d!", ctx->php_pid, pt_ctrl_pid(&(ctx->ctrl), ctx->php_pid));

    if (stat(ctx->mmap_filename, &st) == 0) {
        if (unlink(ctx->mmap_filename) < 0) {
            error_msg(ctx, ERR_TRACE, "can not delete the old traced file %s(%s)", ctx->mmap_filename, strerror(errno));
            die(ctx, -1);
        }
    }

    /* create comm socket before set active */
    if (pt_comm_screate(&ctx->sock, ctx->mmap_filename, 1, PT_COMM_E2T_SIZE, PT_COMM_T2E_SIZE) < 0) {
        die(ctx, -1);
    }

    /* write DO_TRACE op then set active */
    pt_comm_swrite(&ctx->sock, PT_MSG_DO_TRACE, NULL, 0);
    pt_ctrl_set_active(&(ctx->ctrl), ctx->php_pid);

    ctx->trace_flag = flag;
    log_printf (LL_DEBUG, "[trace_start] set ctrl data ok: ctrl[pid=%d]=%u is opened!", ctx->php_pid, flag);
}

void process_opt_e(pt_context_t *ctx)
{
    if (pt_ctrl_open(&(ctx->ctrl), PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME) < 0) {
        error_msg(ctx, ERR_CTRL, "cannot open control mmap file %s (%s)",
                PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME, (errno ? strerror(errno) : "null"));
        die(ctx, -1);
    }
    log_printf (LL_DEBUG, " [ok] ctrl_init open (%s)", PHPTRACE_LOG_DIR "/" PT_CTRL_FILENAME);
    if (ctx->php_pid >= 0) {
        pt_ctrl_set_inactive(&(ctx->ctrl), ctx->php_pid);
        printf ("clean process %d.\n", ctx->php_pid);
    } else {
        pt_ctrl_clean_all(&(ctx->ctrl));
        printf ("clean all process.\n");
    }
    pt_ctrl_close(&(ctx->ctrl));
}

void usage()
{
    printf ("usage: phptrace [-chlsnvvvvw]  [-p pid] [--cleanup]\n\
   or: phptrace [-chlnvvvvw]  [-r infile]\n\
    -h                   -- show this help\n\
    --cleanup            -- set inactive for php process of pid, or all processes if no pid parameter\n\
    -p pid               -- access the php process pid\n\
    -s                   -- print status of the php process by the pid\n\
    -c[top_n]            -- count the wall time, cpu time, memory usage for each function calls of the php process.\n\
                            list top_n of the functions (default is 20).\n\
    -S sortby            -- sort the output of the count results. Legal values are wt, ct, \n\
                            avgct, calls, mem and avgmem (default wt)\n\
    --exclusive          -- count the exclusive wall time and cpu time instead of inclusive time\n\
    --max-level level    -- specify the max function level when count or trace. There is no limit by default\n\
                            except counting the exclusive time.\n\
    -n function-count    -- specify the total function number to trace or count, there is no limit by default\n\
    -l size              -- specify the max string length to print\n\
    -v                   -- print verbose information\n\
    -w outfile           -- write the trace data to file in trace format\n\
    -r infile            -- read the trace file of trace format, instead of the process\n\
    -o outfile           -- write the trace data to file in specified format\n\
    --format format      -- specify the format when -o option is set. Legal values is json for now\n");
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
            case '"':
                s = sdscatprintf(s,"\\%c",*p);
                break;
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

sds pt_repr_function(sds buf, pt_frame_t *f)
{
    if ((f->functype & PT_FUNC_TYPES) == PT_FUNC_NORMAL ||
            f->functype & PT_FUNC_TYPES & PT_FUNC_INCLUDES) {
        buf = sdscatprintf (buf, "%s", f->function);
    } else if ((f->functype & PT_FUNC_TYPES) == PT_FUNC_MEMBER) {
        buf = sdscatprintf (buf, "%s->%s", f->class, f->function);
    } else if ((f->functype & PT_FUNC_TYPES) == PT_FUNC_STATIC) {
        buf = sdscatprintf (buf, "%s::%s", f->class, f->function);
    } else if ((f->functype & PT_FUNC_TYPES) == PT_FUNC_EVAL) {
        buf = sdscatprintf (buf, "%s", f->function);
    } else {
        buf = sdscatprintf (buf, "unknown");
    }
    return buf;
}

sds standard_transform(pt_context_t *ctx, pt_comm_message_t *msg, pt_frame_t *f)
{
    int i;
    sds buf = sdsempty();

    if (f->type == PT_FRAME_ENTRY) {
        buf = sdscatprintf (buf, "%lld.%06d", (long long)(f->entry.wall_time / 1000000), (int)(f->entry.wall_time % 1000000));   
        buf = print_indent_str (buf, "  ", f->level + 1);
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
    } else if (f->type == PT_FRAME_EXIT) {
        buf = sdscatprintf (buf, "%lld.%06d", (long long)(f->exit.wall_time / 1000000), (int)(f->exit.wall_time % 1000000));   
        buf = print_indent_str (buf, "  ", f->level + 1);
        buf = pt_repr_function(buf, f);
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

sds dump_transform(pt_context_t *ctx, pt_comm_message_t *msg, pt_frame_t *f)
{
    sds buf;
    size_t raw_size;

    raw_size = pt_type_len_frame(f);
    buf = sdsnewlen(NULL, raw_size + sizeof(pt_comm_message_t));
    pt_comm_write_message(msg->seq, msg->type, raw_size, f, buf);
    log_printf(LL_DEBUG, "[dump] record(sequence=%d) raw_size=%u", msg->seq, raw_size);
    return buf;
}

sds json_transform(pt_context_t *ctx, pt_comm_message_t *msg, pt_frame_t *f)
{
    int i;
    sds buf = sdsempty();

    buf = sdscatprintf (buf, "{\"seq\":%u, \"type\":%u, \"level\":%u, \"func\":\"",
            msg->seq, f->type, f->level);
    buf = pt_repr_function(buf, f);

    if (f->type == PT_FRAME_ENTRY) {
        buf = sdscatprintf (buf, "\", \"st\":%"PRIu64", ", f->entry.wall_time);

        buf = sdscatprintf (buf, "\"params\":\"");
        if (f->arg_count) {
            for (i = 0; i < f->arg_count; i++) {
                buf = sdscatrepr_noquto (buf, f->args[i], sdslen(f->args[i]));
                if (i < f->arg_count - 1) {
                    buf = sdscatprintf (buf, ", ");
                }
            }
        }
        buf = sdscatprintf (buf, "\", \"file\":");
        buf = sdscatrepr (buf, f->filename, sdslen(f->filename));
        buf = sdscatprintf (buf, ", \"lineno\":%u }\n", f->lineno);
    } else {
        buf = sdscatprintf (buf, ", \"st\":%"PRIu64", ", f->exit.wall_time);
        buf = sdscatprintf (buf, "\"return\":");
        if (f->retval) {
            buf = sdscatrepr (buf, f->retval, sdslen(f->retval));
        }
        buf = sdscatprintf (buf, ", \"wt\":%" PRIu64 ", \"ct\":%" PRIu64 ", \"mem\":%" PRId64 ", \"pmem\":%" PRId64 " }\n",
                f->exit.wall_time - f->entry.wall_time,
                f->exit.cpu_time - f->entry.cpu_time,
                f->exit.mem - f->entry.mem,
                f->exit.mempeak - f->entry.mempeak);
    }
    return buf;
}

#define FREE_SDS(var)           \
    if (var) {                  \
        sdsfree((var));         \
        (var) = NULL;           \
    }
void frame_free_sds(pt_frame_t *f)
{
    int i;
    FREE_SDS(f->function)
    FREE_SDS(f->filename)
    FREE_SDS(f->class)
    FREE_SDS(f->retval)
    if (f->arg_count > 0) {
        for (i = 0; i < f->arg_count; i++) {
            sdsfree(f->args[i]);
        }
        free(f->args);
        f->args = NULL;
        f->arg_count = 0;
    }
}

void trace(pt_context_t *ctx)
{
    uint64_t seq = 0;
    uint64_t magic_number;
    size_t tmp;

    sds buf;
    uint32_t type;
    int data_wait_interval = DATA_WAIT_INTERVAL;

    /* new protocol API */
    pt_comm_message_t *msg;
    pt_frame_t frame;
    pt_comm_socket_t *p_sock = &(ctx->sock);

    /* open comm socket here if in -r mode */
    if (ctx->in_filename) {
        if (pt_comm_sopen(&ctx->sock, ctx->mmap_filename, 1) < 0) {
            error_msg(ctx, ERR_TRACE, "Can not open %s to read, %s!", ctx->mmap_filename, strerror(errno));
            die(ctx, -1);
        }
    }

    /* exclusive time mode */
    if (ctx->exclusive_flag) {
        ctx->sub_cost_time = calloc(ctx->max_level + 2, sizeof(int64_t));
        ctx->sub_cpu_time = calloc(ctx->max_level + 2, sizeof(int64_t));
    }

    ctx->rotate_cnt++;

    while (1) {
        if (interrupted) {
            break;
        }

        if (!ctx->in_filename) {
            pt_comm_swrite(p_sock, PT_MSG_DO_PING, NULL, 0);           /* send to do ping */
        }

        type = pt_comm_sread_type(p_sock);
        log_printf (LL_DEBUG, "msg type=(%u)", type);

        if (type == PT_MSG_EMPTY) {                         /* wait flag */
            if (ctx->in_filename) {
                break;
            }

            log_printf (LL_DEBUG, "  wait_type will wait %d ms.\n", data_wait_interval);
            pt_msleep(data_wait_interval);
            data_wait_interval = grow_interval(data_wait_interval, MAX_DATA_WAIT_INTERVAL);
            continue; 
        }

        data_wait_interval = DATA_WAIT_INTERVAL;            /* reset the data wait interval */

        if (pt_comm_sread(p_sock, &msg, 1) == PT_MSG_INVALID) {
            if (!ctx->in_filename) {
                error_msg(ctx, ERR_TRACE, "read record failed, maybe write too fast");
            }
            break;
        }

        switch (msg->type) {
            case PT_MSG_RET:
                pt_type_unpack_frame(&frame, msg->data);

                if (ctx->opt_flag & OPT_FLAG_COUNT) {
                    if (frame.type == PT_FRAME_EXIT) {
                        count_record(ctx, &(frame));
                    }
                } else {
                    if ((ctx->output_flag & OUTPUT_FLAG_WRITE) && seq == 0) {           /* dump meta to out_fp */
                        magic_number = PT_MAGIC_NUMBER;
                        fwrite(&magic_number, sizeof(uint64_t), 1, ctx->out_fp);
                        tmp = SIZE_MAX;
                        fwrite(&tmp, sizeof(size_t), 1, ctx->out_fp);
                        tmp = 0;
                        fwrite(&tmp, sizeof(size_t), 1, ctx->out_fp);
                        fflush(NULL);
                    }

                    buf = ctx->record_transform(ctx, msg, &(frame));
                    fwrite(buf, sizeof(char), sdslen(buf), ctx->out_fp);
                    fflush(NULL);
                    sdsfree(buf);
                }

                frame_free_sds(&frame);
                seq++;
                break;
        }
    }

    if (ctx->opt_flag & OPT_FLAG_COUNT) {
        count_summary(ctx);
    }
    die(ctx, 0);
}

