#ifndef PHPTRACE_UTIL_H
#define PHPTRACE_UTIL_H

#include "log.h"
#include "phptrace_mmap.h"
#include "phptrace_protocol.h"
#include "phptrace_ctrl.h"
#include "phptrace_time.h"
#include "sds.h"

#include "sys_trace.h"

#include "uthash.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <string.h>
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# include <stdint.h>
#endif

#define PHPTRACE_NAME "phptrace"
#define PHPTRACE_VERSION "0.2.3 stable"
#define PHPTRACE_DEVELOPER "360 infra webcore team"

#define MAX_TEMP_LENGTH 100
#define MAX_PRINT_LENGTH 600

#define RECORD_STRING_LENGTH  (4 * 1024)

#define DEFAULT_TOP_N       20
#define DEFAULT_MAX_LEVEL   2048

#define OPT_FLAG_STATUS         0x0001                              /* flag of status */
#define OPT_FLAG_CLEANUP        0x0002                              /* flag of cleanup */
#define OPT_FLAG_COUNT          0x0004                              /* flag of count */

#define OUTPUT_FLAG_WRITE       0x0001                              /* flag of write */
#define OUTPUT_FLAG_JSON        0x0002                              /* flag of json */

#define OPEN_DATA_WAIT_INTERVAL 100                                 /* ms */
#define MAX_OPEN_DATA_WAIT_INTERVAL (OPEN_DATA_WAIT_INTERVAL * 16)  /* ms */
#define DATA_WAIT_INTERVAL 10                                       /* ms */
#define MAX_DATA_WAIT_INTERVAL (DATA_WAIT_INTERVAL * 20)            /* ms */

#define grow_interval(t, mx) (((t)<<1) > (mx) ? (mx) : ((t) << 1))  

#define phptrace_msleep(ms) (void) usleep((ms) * 1000)

#define phptrace_mem_read_64b(num, mem)     \
     do { (num) = *((int64_t *)(mem)); } while(0);
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* stack */
#define valid_ptr(p) ((p) && 0 == ((p) & (sizeof(long) - 1)))

#define PHP52 2
#define PHP53 3
#define PHP54 4
#define PHP55 5

#define MAX_STACK_DEEP 16
#define MAX_RETRY 3

enum {
    ERR = 1,
    ERR_INVALID_PARAM,
    ERR_STACK,
    ERR_CTRL,
    ERR_TRACE,
    ERR_COUNT,
};

enum {
    STATE_OPEN = 0,
    STATE_HEADER,
    STATE_RECORD,
    STATE_TAILER,
    STATE_END
};

typedef struct record_count_s {
    sds function_name;

    uint64_t cost_time;                 /* inclusive cost time of function */
    uint64_t cpu_time;                  /* inclusive cpu time of function */
    int64_t memory_usage;
    int64_t memory_peak_usage;
    int32_t calls;

    UT_hash_handle hh;
}record_count_t;

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

typedef sds (*phptrace_record_transform_t)(phptrace_context_t *ctx, phptrace_file_record_t *r);

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

    phptrace_file_t file;
    phptrace_segment_t seg;
    phptrace_ctrl_t ctrl;

    phptrace_record_transform_t record_transform;     /* transform,  a function point32_ter */

    /* stack only */
    int32_t php_version;
    int32_t stack_deep;
    int32_t retry;
    address_info_t addr_info;
};

long hexstring2long(const char*s, size_t len);
unsigned int string2uint(const char *str);

void verror_msg(phptrace_context_t *ctx, int err_code, const char *fmt, va_list p);
void error_msg(phptrace_context_t *ctx, int err_code, const char *fmt, ...);
void die(phptrace_context_t *ctx, int exit_code);

void usage();
void phptrace_context_init(phptrace_context_t *ctx);

void process_opt_e(phptrace_context_t *ctx);

/* print utils */
sds sdscatrepr_noquto(sds s, const char *p, size_t len);
sds print_indent_str(sds s, char* str, int32_t size);
sds print_time(sds s, uint64_t t);

sds standard_transform(phptrace_context_t *ctx, phptrace_file_record_t *r);
sds dump_transform(phptrace_context_t *ctx, phptrace_file_record_t *r);
sds json_transform(phptrace_context_t *ctx, phptrace_file_record_t *r);

void phptrace_record_free(phptrace_file_record_t *r);
int update_mmap_filename(phptrace_context_t *ctx);
void trace_start(phptrace_context_t *ctx);
void trace(phptrace_context_t *ctx);
void trace_cleanup(phptrace_context_t *ctx);

/* count utils */
int wt_cmp(record_count_t *p, record_count_t *q);
int avgwt_cmp(record_count_t *p, record_count_t *q);
int ct_cmp(record_count_t *p, record_count_t *q);
int calls_cmp(record_count_t *p, record_count_t *q);
int name_cmp(record_count_t *p, record_count_t *q);
int mem_cmp(record_count_t *p, record_count_t *q);
int avgmem_cmp(record_count_t *p, record_count_t *q);

void count_record(phptrace_context_t *ctx, phptrace_file_record_t *r);
void count_summary(phptrace_context_t *ctx);
int set_sortby(phptrace_context_t *ctx, char *sortby);

/* stack related */
int stack_dump_once(phptrace_context_t* ctx);
int stack_dump(phptrace_context_t* ctx);
void process_opt_s(phptrace_context_t *ctx);
#endif
