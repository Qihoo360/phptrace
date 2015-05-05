#ifndef PHPTRACE_TYPE_H
#define PHPTRACE_TYPE_H

#include <stdint.h>
#include "sds/sds.h"

/* phptrace_frame */
#define PT_FRAME_ENTRY          1 /* function entry */
#define PT_FRAME_EXIT           2 /* function exit */
#define PT_FRAME_STACK          3 /* backtrace stack */

#define PT_FUNC_INTERNAL        0x80 /* function is ZEND_INTERNAL_FUNCTION */

#define PT_FUNC_TYPES           0x7f /* mask for type of function call */
#define PT_FUNC_INCLUDES        0x10 /* mask for includes type */

#define PT_FUNC_UNKNOWN         0x00
#define PT_FUNC_NORMAL          0x01
#define PT_FUNC_MEMBER          0x02
#define PT_FUNC_STATIC          0x03
#define PT_FUNC_EVAL            0x04
#define PT_FUNC_INCLUDE         0x10
#define PT_FUNC_INCLUDE_ONCE    0x11
#define PT_FUNC_REQUIRE         0x12
#define PT_FUNC_REQUIRE_ONCE    0x13

typedef struct {
    uint8_t type;               /* frame type, entry or exit */
    uint8_t functype;           /* function flags of PT_FUNC_xxx */

    uint32_t lineno;            /* entry lineno */
    sds filename;               /* entry filename */
    sds class;                  /* class name */
    sds function;               /* function name */
    uint32_t level;             /* nesting level */

    uint32_t arg_count;         /* arguments number */
    sds *args;                  /* arguments represent string */
    sds retval;                 /* return value represent string */

    struct {
        uint64_t wall_time;     /* wall time */
        uint64_t cpu_time;      /* cpu time */
        int64_t mem;            /* memory usage */
        int64_t mempeak;        /* memory peak */
    } entry;

    struct {
        uint64_t wall_time;     /* wall time */
        uint64_t cpu_time;      /* cpu time */
        int64_t mem;            /* memory usage */
        int64_t mempeak;        /* memory peak */
    } exit;
} phptrace_frame;

size_t phptrace_type_len_frame(phptrace_frame *frame);
size_t phptrace_type_pack_frame(phptrace_frame *frame, char *buf);
phptrace_frame *phptrace_type_unpack_frame(phptrace_frame *frame, char *buf);

/* phptrace_status
 * NO NEED to use sds because all string is allocated. */
typedef struct {
    char *php_version;      /* php version eg: 5.5.24 */
    char *sapi_name;        /* sapi name eg: fpm-fcgi */

    int64_t mem;            /* memory usage */
    int64_t mempeak;        /* memory peak */
    int64_t mem_real;       /* real memory usage */
    int64_t mempeak_real;   /* real memory peak */

    double request_time;    /* request part, available in fpm, cli-server */
    char *request_method;   /* [optional] */
    char *request_uri;      /* [optional] */
    char *request_query;    /* [optional] */
    char *request_script;   /* [optional] */

    int argc;               /* arguments part, available in cli */
    char **argv;

    int proto_num;

    uint32_t frame_count;   /* backtrace depth */
    phptrace_frame *frames; /* backtrace frames */
} phptrace_status;

#endif
