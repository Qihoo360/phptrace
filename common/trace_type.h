/**
 * Copyright 2017 Qihoo 360
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

#ifndef TRACE_TYPE_H
#define TRACE_TYPE_H

#include <stdint.h>
#include "sds/sds.h"

/* pt_frame */
#define PT_FRAME_ENTRY          1 /* entry */
#define PT_FRAME_EXIT           2 /* exit */
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

/* Buffer Pack */
#define PACK(buf, type, ele) \
    *(type *) buf = ele; buf += sizeof(type)

#define UNPACK(buf, type, ele) \
    ele = *(type *) buf; buf += sizeof(type)

#define LEN_STR_EX(s, lenfunc) \
    (sizeof(uint32_t) + (s == NULL ? 0 : lenfunc(s)))
#define LEN_STR(s) LEN_STR_EX(s, strlen)
#define LEN_SDS(s) LEN_STR_EX(s, sdslen)

#define PACK_STR_EX(buf, ele, lenfunc) \
if (ele == NULL) { \
    PACK(buf, uint32_t, 0); \
} else { \
    PACK(buf, uint32_t, lenfunc(ele)); \
    memcpy(buf, ele, lenfunc(ele)); \
    buf += lenfunc(ele); \
}
#define PACK_STR(buf, ele) PACK_STR_EX(buf, ele, strlen)
#define PACK_SDS(buf, ele) PACK_STR_EX(buf, ele, sdslen)

#define UNPACK_STR_EX(buf, ele, dupfunc) \
UNPACK(buf, uint32_t, len); \
if (len) { \
    ele = dupfunc(buf, len); \
    buf += len; \
} else { \
    ele = NULL; \
}
#define UNPACK_STR(buf, ele) UNPACK_STR_EX(buf, ele, strndup)
#define UNPACK_SDS(buf, ele) UNPACK_STR_EX(buf, ele, sdsnewlen)


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

    int64_t inc_time;           /* inclusive wall time */
    int64_t exc_time;           /* exclusive wall time */
} pt_frame_t;

size_t pt_type_len_frame(pt_frame_t *frame);
size_t pt_type_pack_frame(pt_frame_t *frame, char *buf);
size_t pt_type_unpack_frame(pt_frame_t *frame, char *buf);
void pt_type_destroy_frame(pt_frame_t *frame);
void pt_type_display_frame(pt_frame_t *frame, int indent, const char *format, ...);

typedef struct {
    uint8_t type;               /* request type, begin or end */
    sds sapi;                   /* sapi name eg: fpm-fcgi */
    sds script;                 /* request script */
    int64_t time;               /* request time */

    sds method;                 /* request method */
    sds uri;                    /* request uri */

    int argc;                   /* arguments part, available in cli */
    sds *argv;
} pt_request_t;

/* pt_request
 * use sds to make pack, unpack reliable and uniform outside PHP */
size_t pt_type_len_request(pt_request_t *request);
size_t pt_type_pack_request(pt_request_t *request, char *buf);
size_t pt_type_unpack_request(pt_request_t *request, char *buf);
void pt_type_destroy_request(pt_request_t *request);
void pt_type_display_request(pt_request_t *request, const char *format, ...);

/* pt_status */
typedef struct {
    sds php_version;            /* php version eg: 5.5.24 */

    int64_t mem;                /* memory usage */
    int64_t mempeak;            /* memory peak */
    int64_t mem_real;           /* real memory usage */
    int64_t mempeak_real;       /* real memory peak */

    pt_request_t request;       /* request */

    uint32_t frame_count;       /* backtrace depth */
    pt_frame_t *frames;         /* backtrace frames */
} pt_status_t;

size_t pt_type_len_status(pt_status_t *status);
size_t pt_type_pack_status(pt_status_t *status, char *buf);
size_t pt_type_unpack_status(pt_status_t *status, char *buf);
void pt_type_destroy_status(pt_status_t *status, int free_request);
void pt_type_display_status(pt_status_t *status);

#endif
