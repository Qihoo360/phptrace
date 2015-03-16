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

static const count_dimension_t count_dimension[] = {
    { wt_cmp, "wt", "wt" },
    { avgwt_cmp, "avg wt", "avgwt" },
    { ct_cmp, "ct", "ct" },
    { calls_cmp, "calls", "calls" },
    { mem_cmp, "+mem", "mem" },
    { avgmem_cmp, "+avg mem", "avgmem" },
    { NULL, "nothing" }
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

    ctx->max_level = DEFAULT_MAX_LEVEL;

    ctx->max_print_len = MAX_PRINT_LENGTH;
    ctx->seg.shmaddr = MAP_FAILED;

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

int update_mmap_filename(phptrace_context_t *ctx)
{
    if (!ctx->file.tailer.filename || !sdscmp(ctx->file.tailer.filename, ctx->mmap_filename)) {
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
    printf ("usage: phptrace [-chlsnvvvvw]  [-p pid] [--cleanup]\n\
   or: phptrace [-chlnvvvvw]  [-r infile]\n\
    -h                   -- show this help\n\
    --cleanup            -- cleanup the trace switches of pid, or all the switches if no pid parameter\n\
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
    -w outfile           -- write the trace data to file in phptrace format\n\
    -r infile            -- read the trace file of phptrace format, instead of the process\n\
    -o outfile           -- write the trace data to file in specified format\n\
    --format format      -- specify the format when -o option is set. Legal values is json for now\n\
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
    return sdscatprintf (s, "%lld.%06d", (long long)(t / 1000000), (int)(t % 1000000LL));
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

sds standard_transform(phptrace_context_t *ctx, phptrace_file_record_t *r)
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
    buf = sdscat(buf, "\n");
    log_printf (LL_DEBUG, " record_sds[%s]", buf);
    return buf;
}

sds dump_transform(phptrace_context_t *ctx, phptrace_file_record_t *r)
{
    sds buf;
    size_t raw_size;

    raw_size = phptrace_record_rawsize(r);
    buf = sdsnewlen(NULL, raw_size);
    phptrace_mem_write_record(r, buf);
    log_printf(LL_DEBUG, "[dump] record(sequence=%lld) raw_size=%u", r->seq, raw_size);
    return buf;
}

sds json_transform(phptrace_context_t *ctx, phptrace_file_record_t *r)
{
    sds buf = sdsempty();

    buf = sdscatprintf (buf, "{\"seq\":%"PRId64", \"flag\":%u, \"level\":%u, \"func\":",
            r->seq, r->flag, r->level);
    buf = sdscatrepr(buf, r->function_name, sdslen(r->function_name));
    buf = sdscatprintf (buf, ", \"st\":%"PRIu64", ", r->start_time);
    if (r->flag == RECORD_FLAG_ENTRY) {
        buf = sdscatprintf (buf, "\"params\":");
        buf = sdscatrepr (buf, RECORD_ENTRY(r, params), sdslen(RECORD_ENTRY(r, params)));
        buf = sdscatprintf (buf, ", \"file\":");
        buf = sdscatrepr (buf, RECORD_ENTRY(r, filename), sdslen(RECORD_ENTRY(r, filename))),
        buf = sdscatprintf (buf, ", \"lineno\":%u }\n", RECORD_ENTRY(r, lineno));
    } else {
        buf = sdscatprintf (buf, "\"return\":");
        buf = sdscatrepr (buf, RECORD_EXIT(r, return_value), sdslen(RECORD_EXIT(r, return_value)));

        buf = sdscatprintf (buf, ", \"wt\":%" PRIu64 ", \"ct\":%" PRIu64 ", \"mem\":%" PRId64 ", \"pmem\":%" PRId64 " }\n",
                RECORD_EXIT(r, cost_time),
                RECORD_EXIT(r, cpu_time),
                RECORD_EXIT(r, memory_usage),
                RECORD_EXIT(r, memory_peak_usage));
    }
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

void count_record(phptrace_context_t *ctx, phptrace_file_record_t *r)
{
    record_count_t *find_rc;
    record_count_t *tmp;

    /* note:  HASH_FIND_STR(header, findstr, out)  is implemented by HASH_FIND
     * 1. HASH_FIND will set out to null first!!
     * 2. HASH_FIND will use strlen(findstr) as an argument!!
     * */
    HASH_FIND_STR(ctx->record_count, r->function_name, find_rc);
    if (!find_rc) {                                         /* update */
        tmp = (record_count_t *)calloc(1, sizeof(record_count_t));
        tmp->function_name = sdsdup(r->function_name);
        log_printf (LL_DEBUG, " hash miss, add new ");
        ctx->record_num++;

        HASH_ADD_KEYPTR(hh, ctx->record_count, tmp->function_name, sdslen(tmp->function_name), tmp);
    } else {
        tmp = find_rc;
    }

    if (ctx->exclusive_flag) {
        if (ctx->max_level >= r->level) {
            ctx->sub_cost_time[r->level] += RECORD_EXIT(r, cost_time);
            tmp->cost_time += RECORD_EXIT(r, cost_time) - ctx->sub_cost_time[r->level + 1];
            ctx->sub_cost_time[r->level + 1] = 0LL;

            ctx->sub_cpu_time[r->level] += RECORD_EXIT(r, cpu_time);
            tmp->cpu_time += RECORD_EXIT(r, cpu_time) - ctx->sub_cpu_time[r->level + 1];
            ctx->sub_cpu_time[r->level + 1] = 0LL;
        } else {
            log_printf (LL_DEBUG, "[count] record level is larger than max_level(%d)\n", ctx->max_level);
        }
    } else {
        tmp->cost_time += RECORD_EXIT(r, cost_time);
        tmp->cpu_time += RECORD_EXIT(r, cpu_time);
    }
    tmp->memory_usage += RECORD_EXIT(r, memory_usage);
    tmp->memory_peak_usage = MAX(tmp->memory_peak_usage, RECORD_EXIT(r, memory_peak_usage));
    tmp->calls++;
    log_printf (LL_DEBUG, "         function_name(%s) calls=%d\n cost_time=(%llu)\n", tmp->function_name, tmp->calls, tmp->cost_time);
}

int wt_cmp(record_count_t *p, record_count_t *q)
{
    return (q->cost_time - p->cost_time);
}
int avgwt_cmp(record_count_t *p, record_count_t *q)
{
    return (q->cost_time / q->calls - p->cost_time / p->calls);
}
int ct_cmp(record_count_t *p, record_count_t *q)
{
    return (q->cpu_time - p->cpu_time);
}
int calls_cmp(record_count_t *p, record_count_t *q)
{
    return (q->calls - p->calls);
}
int name_cmp(record_count_t *p, record_count_t *q)
{
    return strcmp(p->function_name, q->function_name);
}
int mem_cmp(record_count_t *p, record_count_t *q)
{
    return (q->memory_usage - p->memory_usage);
}
int avgmem_cmp(record_count_t *p, record_count_t *q)
{
    return (q->memory_usage / q->calls - p->memory_usage / p->calls);
}

int set_sortby(phptrace_context_t *ctx, char *sortby)
{
    int i;

    for (i = 0; count_dimension[i].cmp; i++) {
        if (strcmp(sortby, count_dimension[i].sortby) == 0) {
            ctx->sortby_idx = i;
            return 1;
        }
    }
    return 0;
}
int64_t get_sortby_value(phptrace_context_t *ctx, record_count_t *rc)
{
    switch (ctx->sortby_idx) {
        case 0: return rc->cost_time;
        case 1: return (rc->cost_time / rc->calls);
        case 2: return rc->cpu_time;
        case 3: return rc->calls;
        case 4: return rc->memory_usage;
        case 5: return (rc->memory_usage / rc->calls);
        default: return 0;
    }
}

void count_summary(phptrace_context_t *ctx)
{
    const char *dashes = "----------------";
    uint32_t size;
    uint32_t cnt;
    uint32_t calls_all = 0;
    uint64_t cost_time_all = 0;
    uint64_t cpu_time_all = 0;
    int64_t memory_usage_all = 0;
    int64_t sortby_all = 0;
    int64_t tmp_v;
    double percent;

    record_count_t *rc;
    record_count_t *tmp;

    size = HASH_COUNT(ctx->record_count);

    if (ctx->sortby_idx >= 0) {
        HASH_SORT(ctx->record_count, count_dimension[ctx->sortby_idx].cmp);
        log_printf (LL_DEBUG, " count sortby %s\n", count_dimension[ctx->sortby_idx].sortby);
    }

    for (rc = ctx->record_count; rc != NULL; rc = rc->hh.next) {
        calls_all += rc->calls;
        cost_time_all += rc->cost_time;
        cpu_time_all += rc->cpu_time;
        memory_usage_all += rc->memory_usage;
        tmp_v = get_sortby_value(ctx, rc);
        if (tmp_v > 0) {                          /* only count positive number */
            sortby_all += tmp_v;
        }
    }

    fprintf (ctx->out_fp, "Keys to Summary:\n    wt (wall time), avg (average), ct (cpu time)\n%s%s%s\n",
            dashes, dashes, dashes);
    fprintf (ctx->out_fp, "Note: time is %s.\n%s%s%s\n", (ctx->exclusive_flag ? "exclusive" : "inclusive"),
            dashes, dashes, dashes);

    fprintf(ctx->out_fp, "%8.8s %10.10s %10.10s %10.10s %10.10s %10.10s %10.10s %s\n",
            count_dimension[ctx->sortby_idx].title,
            "wt", "avg wt", "ct",
            "mem", "avg mem",
            "calls", "function name");
    fprintf(ctx->out_fp, "%8.8s %10.10s %10.10s %10.10s %10.10s %10.10s %10.10s %s\n",
            "%",
            "(seconds)", "(us/call)", "(us)",
            "(Bytes)", "(B/call)",
            "", "");

    fprintf(ctx->out_fp, "%8.8s %10.10s %10.10s %10.10s %10.10s %10.10s %10.10s %s\n",
            dashes, dashes, dashes, dashes, dashes, dashes, dashes, dashes);
    log_printf (LL_DEBUG, " after count, hash table size=%u ctx->record_num=%u\n", size, ctx->record_num);
    for (rc = ctx->record_count, cnt = 0; rc != NULL && cnt < ctx->top_n; rc = rc->hh.next, cnt++) {
        tmp_v = get_sortby_value(ctx, rc);
        if (sortby_all > 0 && tmp_v >= 0) {
            percent = 100.0 * tmp_v / sortby_all;
            fprintf (ctx->out_fp, "%8.2f ", percent);
        } else {
            fprintf (ctx->out_fp, "%8.8s ", "");
        }
        fprintf(ctx->out_fp, "%10.6f %10" PRIu64 " %10"PRId64" %10"PRId64" %10"PRId64" %10u %s\n",
                rc->cost_time / 1000000.0,
                rc->cost_time / rc->calls,
                rc->cpu_time,
                rc->memory_usage,
                rc->memory_usage / rc->calls,
                rc->calls,
                rc->function_name);
    }

    fprintf(ctx->out_fp, "%8.8s %10.10s %10.10s %10.10s %10.10s %10.10s %10.10s %s\n",
            dashes, dashes, dashes, dashes, dashes, dashes, dashes, dashes);
    fprintf(ctx->out_fp, "%8.8s %10.6f %10.10s %10" PRIu64 " %10.10s %10.10s %10u %s\n",
            "100.00", cost_time_all / 1000000.0,  "", cpu_time_all, "", "", calls_all, "total");

    HASH_ITER(hh, ctx->record_count, rc, tmp) {
        sdsfree(rc->function_name);
        HASH_DEL(ctx->record_count, rc);
        free(rc);
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

    /* exclusive time mode */
    if (ctx->exclusive_flag) {
        ctx->sub_cost_time = calloc(ctx->max_level + 2, sizeof(int64_t));
        ctx->sub_cpu_time = calloc(ctx->max_level + 2, sizeof(int64_t));
    }

    while (1) {
        if (interrupted) {
            break;
        }

        if (state == STATE_END) {
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

                if (ctx->rotate_cnt == 1 && (ctx->output_flag & OUTPUT_FLAG_WRITE)) {                          /* dump header to out_fp */
                    buf = sdsnewlen(NULL, raw_size);
                    phptrace_mem_write_header(&(ctx->file.header), buf);
                    fwrite(buf, sizeof(char), raw_size, ctx->out_fp);

                    log_printf(LL_DEBUG, "[dump] header raw_size=%u", raw_size);
                    sdsfree(buf);
                }
                break;
            case STATE_RECORD:
                if (flag != seq) {
                    error_msg(ctx, ERR_TRACE, "sequence number is incorrect, trace file may be damaged");
                    if (ctx->output_flag & OUTPUT_FLAG_WRITE) {
                        state = STATE_END;
                        continue;
                    }
                    die(ctx, -1);
                }

                if (ctx->max_function > 0 && ctx->max_function * 2 < flag) {                   /* max records limit */
                    state = STATE_END;
                    continue;
                }

                mem_ptr = phptrace_mem_read_record(&(rcd), mem_ptr, flag);
                if (!mem_ptr) {
                    error_msg(ctx, ERR_TRACE, "read record failed, maybe write too fast");
                    if (ctx->output_flag & OUTPUT_FLAG_WRITE) {
                        state = STATE_END;
                        continue;
                    }
                    die(ctx, -1);
                }

                if (ctx->opt_flag & OPT_FLAG_COUNT) {
                    if (rcd.flag == RECORD_FLAG_EXIT) {
                        count_record(ctx, &(rcd));
                    }
                } else {
                    buf = ctx->record_transform(ctx, &(rcd));
                    fwrite(buf, sizeof(char), sdslen(buf), ctx->out_fp);
                    fflush(NULL);
                    sdsfree(buf);
                }

                phptrace_record_free(&(rcd));
                seq++;
                break;
            case STATE_TAILER:
                if (flag != MAGIC_NUMBER_TAILER) {
                    error_msg(ctx, ERR_TRACE, "tailer's magic number is not correct, trace file may be damaged");
                    if (ctx->output_flag & OUTPUT_FLAG_WRITE) {
                        state = STATE_END;
                        continue;
                    }
                    die(ctx, -1);
                }

                if (ctx->file.tailer.filename) {                    /* free tailer.filename */
                    sdsfree(ctx->file.tailer.filename);
                    ctx->file.tailer.filename = NULL;
                }
                tmp_ptr = phptrace_mem_read_tailer(&(ctx->file.tailer), mem_ptr);
                raw_size = tmp_ptr - mem_ptr;
                mem_ptr = tmp_ptr;

                if (ctx->in_filename) {                             /* -r option, read from file */
                    state = STATE_END;
                } else {
                    state = STATE_OPEN;
                }
                log_printf (LL_DEBUG, " [ok load tailer]");
                break;
        }
    }

    if (ctx->output_flag & OUTPUT_FLAG_WRITE) {                      /* dump empty tailer to out_fp */
        len = 0;
        magic_number = MAGIC_NUMBER_TAILER;
        fwrite(&magic_number, sizeof(uint64_t), 1, ctx->out_fp);
        fwrite(&len, sizeof(uint32_t), 1, ctx->out_fp);

        fflush(NULL);
        log_printf(LL_DEBUG, "[dump] empty tailer");
    } else if (ctx->opt_flag & OPT_FLAG_COUNT) {
        count_summary(ctx);
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

