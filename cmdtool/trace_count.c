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

#include "trace_count.h"

const count_dimension_t count_dimension[] = {
    { wt_cmp, "wt", "wt" },
    { avgwt_cmp, "avg wt", "avgwt" },
    { ct_cmp, "ct", "ct" },
    { calls_cmp, "calls", "calls" },
    { mem_cmp, "+mem", "mem" },
    { avgmem_cmp, "+avg mem", "avgmem" },
    { NULL, "nothing" }
};

void count_record(pt_context_t *ctx, pt_frame_t *f)
{
    uint64_t td;
    record_count_t *find_rc;
    record_count_t *tmp;

    /* note:  HASH_FIND_STR(header, findstr, out)  is implemented by HASH_FIND
     * 1. HASH_FIND will set out to null first!!
     * 2. HASH_FIND will use strlen(findstr) as an argument!!
     * */
    HASH_FIND_STR(ctx->record_count, f->function, find_rc);
    if (!find_rc) {                                         /* update */
        tmp = (record_count_t *)calloc(1, sizeof(record_count_t));
        tmp->function_name = sdsdup(f->function);
        log_printf (LL_DEBUG, " hash miss, add new ");
        ctx->record_num++;

        HASH_ADD_KEYPTR(hh, ctx->record_count, tmp->function_name, sdslen(tmp->function_name), tmp);
    } else {
        tmp = find_rc;
    }

    if (ctx->exclusive_flag) {
        if (ctx->max_level >= f->level) {
            td = f->exit.wall_time - f->entry.wall_time;
            ctx->sub_cost_time[f->level] += td;
            tmp->cost_time += td - ctx->sub_cost_time[f->level + 1];
            ctx->sub_cost_time[f->level + 1] = 0LL;

            td = f->exit.cpu_time - f->entry.cpu_time;
            ctx->sub_cpu_time[f->level] += td;
            tmp->cpu_time += td - ctx->sub_cpu_time[f->level + 1];
            ctx->sub_cpu_time[f->level + 1] = 0LL;
        } else {
            log_printf (LL_DEBUG, "[count] record level is larger than max_level(%d)\n", ctx->max_level);
        }
    } else {
        tmp->cost_time += f->exit.wall_time - f->entry.wall_time;
        tmp->cpu_time += f->exit.cpu_time - f->entry.cpu_time;
    }
    tmp->memory_usage += f->exit.mem - f->entry.mem;
    tmp->memory_peak_usage = MAX(tmp->memory_peak_usage, f->exit.mempeak - f->entry.mempeak);
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

int set_sortby(pt_context_t *ctx, char *sortby)
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
int64_t get_sortby_value(pt_context_t *ctx, record_count_t *rc)
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

void count_summary(pt_context_t *ctx)
{
    const char *dashes = "----------------";
    uint32_t size;
    uint32_t cnt;
    uint32_t calls_all = 0;
    int64_t cost_time_all = 0;
    int64_t cpu_time_all = 0;
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

