#ifndef PHPTRACE_COUNT_H
#define PHPTRACE_COUNT_H

#include "phptrace.h"
#include "phptrace_util.h"

/* count utils */
int wt_cmp(record_count_t *p, record_count_t *q);
int avgwt_cmp(record_count_t *p, record_count_t *q);
int ct_cmp(record_count_t *p, record_count_t *q);
int calls_cmp(record_count_t *p, record_count_t *q);
int name_cmp(record_count_t *p, record_count_t *q);
int mem_cmp(record_count_t *p, record_count_t *q);
int avgmem_cmp(record_count_t *p, record_count_t *q);

void count_record(phptrace_context_t *ctx, phptrace_frame *f);
void count_summary(phptrace_context_t *ctx);
int set_sortby(phptrace_context_t *ctx, char *sortby);

#endif
