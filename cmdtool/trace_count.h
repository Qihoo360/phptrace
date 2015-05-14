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

#ifndef TRACE_COUNT_H
#define TRACE_COUNT_H

#include "trace.h"
#include "trace_util.h"

/* count utils */
int wt_cmp(record_count_t *p, record_count_t *q);
int avgwt_cmp(record_count_t *p, record_count_t *q);
int ct_cmp(record_count_t *p, record_count_t *q);
int calls_cmp(record_count_t *p, record_count_t *q);
int name_cmp(record_count_t *p, record_count_t *q);
int mem_cmp(record_count_t *p, record_count_t *q);
int avgmem_cmp(record_count_t *p, record_count_t *q);

void count_record(pt_context_t *ctx, pt_frame_t *f);
void count_summary(pt_context_t *ctx);
int set_sortby(pt_context_t *ctx, char *sortby);

#endif
