#ifndef PHPTRACE_TYPE_H
#define PHPTRACE_TYPE_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "sds/sds.h"

#define PT_FUNC_UNKNOWN         0x00
#define PT_FUNC_NORMAL          0x01
#define PT_FUNC_MEMBER          0x02
#define PT_FUNC_STATIC          0x03

#define PT_FUNC_INCLUDES        0x10
#define PT_FUNC_INCLUDE         0x10
#define PT_FUNC_INCLUDE_ONCE    0x11
#define PT_FUNC_REQUIRE         0x12
#define PT_FUNC_REQUIRE_ONCE    0x13
#define PT_FUNC_EVAL            0x14

typedef struct {
    uint8_t internal;           /* is ZEND_INTERNAL_FUNCTION */
    uint8_t type;               /* flags of PT_FUNC_xxx */
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

#endif
