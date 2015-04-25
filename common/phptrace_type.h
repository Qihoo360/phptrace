#ifndef PHPTRACE_TYPE_H
#define PHPTRACE_TYPE_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "sds/sds.h"

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
        uint64_t    wall_time;  /* wall time cost */
        uint64_t    cpu_time;   /* cpu time cost */
        int64_t     mem;        /* delta memory usage */
        int64_t     mempeak;    /* delta memory peak */
    } profile;
} phptrace_frame;

size_t phptrace_type_len_frame(phptrace_frame *frame);
size_t phptrace_type_pack_frame(phptrace_frame *frame, char *buf);
phptrace_frame *phptrace_type_unpack_frame(phptrace_frame *frame, char *buf);

#endif
