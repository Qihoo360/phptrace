#ifndef PHPTRACE_H
#define PHPTRACE_H

#include "log.h"
#include "phptrace_mmap.h"
#include "phptrace_protocol.h"
#include "phptrace_ctrl.h"
#include "phptrace_time.h"

#include "phptrace_comm.h"
#include "phptrace_type.h"

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
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# include <stdint.h>
#endif

typedef struct record_count_s {
    sds function_name;

    uint64_t cost_time;                 /* inclusive cost time of function */
    uint64_t cpu_time;                  /* inclusive cpu time of function */
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

typedef struct phptrace_context_s phptrace_context_t;

typedef sds (*phptrace_record_transform_t)(phptrace_context_t *ctx, phptrace_comm_message *msg, phptrace_frame *f);

struct phptrace_context_s {
    int32_t php_pid;                                    /* pid of the -p option, default -1 */
    uint64_t start_time;                                /* start time of cmdtool */

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

    /*
    phptrace_file_t file;
    phptrace_segment_t seg;
    */
    phptrace_comm_socket sock;

    phptrace_ctrl_t ctrl;

    phptrace_record_transform_t record_transform;     /* transform,  a function point32_ter */

    /* stack only */
    int32_t php_version;
    int32_t stack_deep;
    int32_t retry;
    address_info_t addr_info;
};

#endif
