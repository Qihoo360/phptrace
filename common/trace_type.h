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

#ifndef TRACE_TYPE_H
#define TRACE_TYPE_H

#include <stdint.h>
#include "sds/sds.h"

/* pt_frame */
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
        int64_t wall_time;      /* wall time */
        int64_t cpu_time;       /* cpu time */
        int64_t mem;            /* memory usage */
        int64_t mempeak;        /* memory peak */
    } entry;

    struct {
        int64_t wall_time;      /* wall time */
        int64_t cpu_time;       /* cpu time */
        int64_t mem;            /* memory usage */
        int64_t mempeak;        /* memory peak */
    } exit;
} pt_frame_t;

size_t pt_type_len_frame(pt_frame_t *frame);
size_t pt_type_pack_frame(pt_frame_t *frame, char *buf);
size_t pt_type_unpack_frame(pt_frame_t *frame, char *buf);

/* pt_status
 * use sds to make pack, unpack reliable and uniform outside PHP */
typedef struct {
    sds php_version;            /* php version eg: 5.5.24 */
    sds sapi_name;              /* sapi name eg: fpm-fcgi */

    int64_t mem;                /* memory usage */
    int64_t mempeak;            /* memory peak */
    int64_t mem_real;           /* real memory usage */
    int64_t mempeak_real;       /* real memory peak */

    double request_time;        /* request part, available in fpm, cli-server */
    sds request_method;         /* [optional] */
    sds request_uri;            /* [optional] */
    sds request_query;          /* [optional] */
    sds request_script;         /* [optional] */

    int argc;                   /* arguments part, available in cli */
    sds *argv;

    int proto_num;

    uint32_t frame_count;       /* backtrace depth */
    pt_frame_t *frames;     /* backtrace frames */
} pt_status_t;

size_t pt_type_len_status(pt_status_t *status);
size_t pt_type_pack_status(pt_status_t *status, char *buf);
size_t pt_type_unpack_status(pt_status_t *status, char *buf);

#endif
