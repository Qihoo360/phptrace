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

#ifndef TRACE_H
#define TRACE_H

#include "log.h"
#include "trace_mmap.h"
#include "trace_ctrl.h"
#include "trace_time.h"

#include "trace_comm.h"
#include "trace_type.h"

#include "sds.h"

#include "sys_trace.h"

#include "uthash.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h> /* needs PRIu64, PRId64 */
#include <stdint.h>

typedef struct record_count_s {
    sds function_name;

    long cost_time;                 /* inclusive cost time of function */
    long cpu_time;                  /* inclusive cpu time of function */
    int64_t memory_usage;
    int64_t memory_peak_usage;
    int32_t calls;

    UT_hash_handle hh;
} record_count_t;

typedef struct count_dimension_s {
    int (*cmp) (record_count_t *, record_count_t *);
    const char* title;
    const char* sortby;
} count_dimension_t;

/* stack address info */
typedef struct address_info_s {
    long sapi_globals_addr;
    long sapi_globals_path_offset;
    long executor_globals_addr;
    long executor_globals_ed_offset;
    long execute_data_addr;
    long execute_data_f_offset;
    long function_addr;
    long function_fn_offset;
    long execute_data_prev_offset;
    long execute_data_oparray_offset;
    long oparray_addr;
    long oparray_fn_offset;
    long execute_data_opline_offset;
    long opline_addr;
    long opline_ln_offset;
} address_info_t;

typedef struct pt_context_s pt_context_t;

typedef sds (*pt_record_transform_t)(pt_context_t *ctx, pt_comm_message_t *msg, pt_frame_t *f);

struct pt_context_s {
    int32_t php_pid;                                    /* pid of the -p option, default -1 */
    long start_time;                                    /* start time of cmdtool */

    FILE *log;                                          /* log output stream */
    sds mmap_filename;
    int32_t rotate_cnt;                                 /* count of rotate file */

    sds in_filename;                                    /* input filename */
    FILE *out_fp;                                       /* output stream */
    sds out_filename;                                   /* output filename */

    int32_t trace_flag;                                 /* flag of trace data, default 0 */
    int32_t opt_flag;                                   /* flag of cmdtool option, -s -p -c --cleanup */
    int32_t output_flag;                                /* flag of cmdtool output option, may be -w or -o */

    int32_t max_print_len;                              /* max length to print32_t string, default is MAX_PRINT_LENGTH */

    int32_t top_n;                                      /* top number to count, default is DEFAULT_TOP_N */
    record_count_t *record_count;                       /* record count structure */
    int32_t record_num;
    int32_t sortby_idx;
    int32_t exclusive_flag;                             /* flag of count exclusive time, default 0 */

    int32_t max_level;
    int64_t *sub_cost_time;                             /* count the time of sub calls' cost time when exclusive mode */
    int64_t *sub_cpu_time;                              /* count the time of sub calls' cpu time when exclusive mode */
    int64_t max_function;

    pt_comm_socket_t sock;

    pt_ctrl_t ctrl;

    pt_record_transform_t record_transform;     /* transform,  a function point32_ter */

    /* stack only */
    int32_t php_version;
    int32_t status_deep;
    int32_t retry;
    address_info_t addr_info;
};

#endif
