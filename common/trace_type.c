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

#ifndef _GUN_SOURCE
#define _GNU_SOURCE /* only for strndup() */
#endif

#include <stdlib.h>
#include <string.h>
#include "trace_type.h"

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

/* pt_frame */
size_t pt_type_len_frame(pt_frame_t *frame)
{
    int i;
    size_t size = 0;

    size += sizeof(uint8_t);                                  /* type */
    size += sizeof(uint8_t);                                  /* functype */
    size += sizeof(uint32_t);                                 /* lineno */

    size += LEN_SDS(frame->filename);                         /* filename */
    size += LEN_SDS(frame->class);                            /* class */
    size += LEN_SDS(frame->function);                         /* function */

    size += sizeof(uint32_t);                                 /* level */

    size += sizeof(uint32_t);                                 /* arg_count */
    for (i = 0; i < frame->arg_count; i++) {
        size += LEN_SDS(frame->args[i]);                      /* args */
    }
    size += LEN_SDS(frame->retval);                           /* retval */

    size += sizeof(int64_t);                                  /* wall_time */
    size += sizeof(int64_t);                                  /* cpu_time */
    size += sizeof(int64_t);                                  /* mem */
    size += sizeof(int64_t);                                  /* mempeak */

    size += sizeof(int64_t);                                  /* wall_time */
    size += sizeof(int64_t);                                  /* cpu_time */
    size += sizeof(int64_t);                                  /* mem */
    size += sizeof(int64_t);                                  /* mempeak */

    return size;
}

size_t pt_type_pack_frame(pt_frame_t *frame, char *buf)
{
    int i;
    char *ori = buf;

    PACK(buf, uint8_t, frame->type);
    PACK(buf, uint8_t, frame->functype);
    PACK(buf, uint32_t, frame->lineno);

    PACK_SDS(buf, frame->filename);
    PACK_SDS(buf, frame->class);
    PACK_SDS(buf, frame->function);

    PACK(buf, uint32_t, frame->level);

    PACK(buf, uint32_t, frame->arg_count);
    for (i = 0; i < frame->arg_count; i++) {
        PACK_SDS(buf, frame->args[i]);
    }
    PACK_SDS(buf, frame->retval);

    PACK(buf, int64_t, frame->entry.wall_time);
    PACK(buf, int64_t, frame->entry.cpu_time);
    PACK(buf, int64_t,  frame->entry.mem);
    PACK(buf, int64_t,  frame->entry.mempeak);

    PACK(buf, int64_t, frame->exit.wall_time);
    PACK(buf, int64_t, frame->exit.cpu_time);
    PACK(buf, int64_t,  frame->exit.mem);
    PACK(buf, int64_t,  frame->exit.mempeak);

    return buf - ori;
}

size_t pt_type_unpack_frame(pt_frame_t *frame, char *buf)
{
    int i;
    size_t len;
    char *ori = buf;

    UNPACK(buf, uint8_t, frame->type);
    UNPACK(buf, uint8_t, frame->functype);
    UNPACK(buf, uint32_t, frame->lineno);

    UNPACK_SDS(buf, frame->filename);
    UNPACK_SDS(buf, frame->class);
    UNPACK_SDS(buf, frame->function);

    UNPACK(buf, uint32_t, frame->level);

    UNPACK(buf, uint32_t, frame->arg_count);
    if (frame->arg_count > 0) {
        frame->args = calloc(frame->arg_count, sizeof(sds));
    } else {
        frame->args = NULL;
    }
    for (i = 0; i < frame->arg_count; i++) {
        UNPACK_SDS(buf, frame->args[i]);
    }
    UNPACK_SDS(buf, frame->retval);

    UNPACK(buf, int64_t, frame->entry.wall_time);
    UNPACK(buf, int64_t, frame->entry.cpu_time);
    UNPACK(buf, int64_t,  frame->entry.mem);
    UNPACK(buf, int64_t,  frame->entry.mempeak);

    UNPACK(buf, int64_t, frame->exit.wall_time);
    UNPACK(buf, int64_t, frame->exit.cpu_time);
    UNPACK(buf, int64_t,  frame->exit.mem);
    UNPACK(buf, int64_t,  frame->exit.mempeak);

    return buf - ori;
}

/* pt_status */
size_t pt_type_len_status(pt_status_t *status)
{
    /* FIXME support sds, str both */
    int i;
    size_t size = 0;

    size += LEN_STR(status->php_version);                     /* php_version */
    size += LEN_STR(status->sapi_name);                       /* sapi_name */

    size += sizeof(int64_t);                                  /* mem */
    size += sizeof(int64_t);                                  /* mempeak */
    size += sizeof(int64_t);                                  /* real mem */
    size += sizeof(int64_t);                                  /* real mempeak */

    size += sizeof(double);                                   /* request time */
    size += LEN_STR(status->request_method);                  /* request method */
    size += LEN_STR(status->request_uri);                     /* request uri */
    size += LEN_STR(status->request_query);                   /* request query */
    size += LEN_STR(status->request_script);                  /* request script */

    size += sizeof(uint32_t);                                 /* argc */
    for (i = 0; i < status->argc; i++) {
        size += LEN_STR(status->argv[i]);                     /* argv */
    }

    size += sizeof(uint32_t);                                 /* proto_num */

    size += sizeof(uint32_t);                                 /* frame count */
    for (i = 0; i < status->frame_count; i++) {
        size += pt_type_len_frame(status->frames + i);
    }

    return size;
}

size_t pt_type_pack_status(pt_status_t *status, char *buf)
{
    /* FIXME support sds, str both */
    int i;
    size_t len;
    char *ori = buf;

    PACK_STR(buf,       status->php_version);
    PACK_STR(buf,       status->sapi_name);

    PACK(buf, int64_t,  status->mem);
    PACK(buf, int64_t,  status->mempeak);
    PACK(buf, int64_t,  status->mem_real);
    PACK(buf, int64_t,  status->mempeak_real);

    PACK(buf, double,   status->request_time);
    PACK_STR(buf,       status->request_method);
    PACK_STR(buf,       status->request_uri);
    PACK_STR(buf,       status->request_query);
    PACK_STR(buf,       status->request_script);

    PACK(buf, uint32_t, status->argc);
    for (i = 0; i < status->argc; i++) {
        PACK_STR(buf,   status->argv[i]);
    }

    PACK(buf, uint32_t, status->proto_num);

    PACK(buf, uint32_t, status->frame_count);
    for (i = 0; i < status->frame_count; i++) {
        len = pt_type_pack_frame(status->frames + i, buf);
        buf += len;
    }

    return buf - ori;
}

size_t pt_type_unpack_status(pt_status_t *status, char *buf)
{
    int i;
    size_t len;
    char *ori = buf;

    UNPACK_SDS(buf,       status->php_version);
    UNPACK_SDS(buf,       status->sapi_name);

    UNPACK(buf, int64_t,  status->mem);
    UNPACK(buf, int64_t,  status->mempeak);
    UNPACK(buf, int64_t,  status->mem_real);
    UNPACK(buf, int64_t,  status->mempeak_real);

    UNPACK(buf, double,   status->request_time);
    UNPACK_SDS(buf,       status->request_method);
    UNPACK_SDS(buf,       status->request_uri);
    UNPACK_SDS(buf,       status->request_query);
    UNPACK_SDS(buf,       status->request_script);

    UNPACK(buf, uint32_t, status->argc);
    if (status->argc > 0) {
        status->argv = calloc(status->argc, sizeof(char *));
    } else {
        status->argv = NULL;
    }
    for (i = 0; i < status->argc; i++) {
        UNPACK_SDS(buf,   status->argv[i]);
    }

    UNPACK(buf, uint32_t, status->proto_num);

    UNPACK(buf, uint32_t, status->frame_count);
    if (status->frame_count > 0) {
        status->frames = calloc(status->frame_count, sizeof(pt_status_t));
    } else {
        status->frames = NULL;
    }
    for (i = 0; i < status->frame_count; i++) {
        len = pt_type_unpack_frame(status->frames + i, buf);
        buf += len;
    }

    return buf - ori;
}
