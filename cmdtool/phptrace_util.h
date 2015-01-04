#ifndef PHPTRACE_UTIL_H
#define PHPTRACE_UTIL_H

#include "log.h"
#include "phptrace_mmap.h"
#include "phptrace_protocol.h"
#include "phptrace_ctrl.h"
#include "phptrace_time.h"
#include "sds.h"

#include "sys_trace.h"

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
#define PHPTRACE_VERSION "0.2.0 release candidate"
#define PHPTRACE_DEVELOPER "360 infra webcore team"

#define MAX_TEMP_LENGTH 100
#define MAX_PRINT_LENGTH 600

#define RECORD_STRING_LENGTH  (4 * 1024)

#define OPEN_DATA_WAIT_INTERVAL 100                                 /* ms */
#define MAX_OPEN_DATA_WAIT_INTERVAL (OPEN_DATA_WAIT_INTERVAL * 16)  /* ms */
#define DATA_WAIT_INTERVAL 10                                       /* ms */
#define MAX_DATA_WAIT_INTERVAL (DATA_WAIT_INTERVAL * 20)            /* ms */

#define grow_interval(t, mx) (((t)<<1) > (mx) ? (mx) : ((t) << 1))  

#define phptrace_msleep(ms) (void) usleep((ms) * 1000)

#define phptrace_mem_read_64b(num, mem)     \
     do { (num) = *((int64_t *)(mem)); } while(0);
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* stack */
#define valid_ptr(p) ((p) && 0 == ((p) & (sizeof(long) - 1)))

#define PHP52 2
#define PHP53 3
#define PHP54 4
#define PHP55 5

#define MAX_STACK_DEEP 16
#define MAX_RETRY 3

enum {
    STATE_START = 0,
    STATE_OPEN,
    STATE_HEADER,
    STATE_RECORD,
    STATE_TAILER,
    STATE_ERROR
};

static volatile int interrupted;        /* flag of interrupt (ctrl + c) */

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

typedef struct phptrace_context_s {
    int php_pid;                        /* pid of the -p option */
    uint64_t start_time;                /* start time of cmdtool */

    FILE *log;                          /* output stream */
    sds mmap_filename;

    int trace_flag;                     /* flag of trace data, default 0 */
    int opt_c_flag;                     /* flag of cmdtool option -c, default 0 */
    int opt_s_flag;                     /* flag of cmdtool option -s(stack), default 0 */
    int opt_p_flag;                     /* flag of cmdtool option -p(pid), default 0 */

    int max_print_len;                  /* max length to print string, default is MAX_PRINT_LENGTH */

    phptrace_file_t file;
    phptrace_segment_t seg;
    phptrace_ctrl_t ctrl;

    /* stack only */
    int php_version;
    int stack_deep;
    int retry;
    address_info_t addr_info;
} phptrace_context_t;

long hexstring2long(const char*s, size_t len);
unsigned int string2uint(const char *str);

void verror_msg(phptrace_context_t *ctx, int err_code, const char *fmt, va_list p);
void error_msg(phptrace_context_t *ctx, int err_code, const char *fmt, ...);
void die(phptrace_context_t *ctx, int exit_code);

void phptrace_context_init(phptrace_context_t *ctx);
void trace_start_ctrl(phptrace_context_t *ctx);
void process_opt_c(phptrace_context_t *ctx);

int update_mmap_filename(phptrace_context_t *ctx);
void interrupt(int sig);
void usage();

/* print utils */
sds sdscatrepr_noquto(sds s, const char *p, size_t len);
sds print_indent_str(sds s, char* str, int32_t size);
sds print_time(sds s, uint64_t t);
void print_record(phptrace_context_t *ctx, phptrace_file_record_t *r);
void phptrace_record_free(phptrace_file_record_t *r);
void trace(phptrace_context_t *ctx);
void trace_cleanup(phptrace_context_t *ctx);

/* stack related */
int stack_dump_once(phptrace_context_t* ctx);
int stack_dump(phptrace_context_t* ctx);
void process_opt_s(phptrace_context_t *ctx);

void init(phptrace_context_t *ctx, int argc, char *argv[]);
void process(phptrace_context_t *ctx);

#endif
