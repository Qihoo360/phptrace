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

#ifndef _GUN_SOURCE
#define _GNU_SOURCE /* only for strndup() */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "trace_type.h"
#include "trace_time.h"
#include "trace_color.h"

/* TODO move outside this file */
static int output_is_tty = -1;

static int has_color(void)
{
    if (output_is_tty == -1) {
        output_is_tty = isatty(1); /* STDOUT fd */
    }

    return output_is_tty == 1 ? 1 : 0;
}

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

    size += sizeof(int64_t);                                  /* inc_time */
    size += sizeof(int64_t);                                  /* exc_time */

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

    PACK(buf, int64_t, frame->inc_time);
    PACK(buf, int64_t, frame->exc_time);

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

    UNPACK(buf, int64_t, frame->inc_time);
    UNPACK(buf, int64_t, frame->exc_time);

    return buf - ori;
}

void pt_type_destroy_frame(pt_frame_t *frame)
{
    int i;

    sdsfree(frame->filename);
    sdsfree(frame->class);
    sdsfree(frame->function);
    if (frame->args && frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            sdsfree(frame->args[i]);
        }
        free(frame->args);
    }
    sdsfree(frame->retval);
}

void pt_type_display_frame(pt_frame_t *frame, int indent, const char *format, ...)
{
    int i, has_bracket = 1;
    va_list ap;

    /* indent */
    if (indent) {
        printf("%*s", (frame->level - 1) * 4, "");
    }

    /* format */
    if (format) {
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }

    /* frame */
    if (has_color()) {
        printf(PTC_GREEN);
    }
    if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_NORMAL ||
            frame->functype & PT_FUNC_TYPES & PT_FUNC_INCLUDES) {
        printf("%s", frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_MEMBER) {
        printf("%s->%s", frame->class, frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_STATIC) {
        printf("%s::%s", frame->class, frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_EVAL) {
        printf("%s", frame->function);
        has_bracket = 0;
    } else {
        printf("unknown");
        has_bracket = 0;
    }
    if (has_color()) {
        printf(PTC_RESET);
    }

    /* arguments */
    if (has_bracket) {
        printf("(");
    }
    if (frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            if (has_color()) {
                printf(PTC_MAGENTA "%s" PTC_RESET, frame->args[i]);
            } else {
                printf("%s", frame->args[i]);
            }
            if (i < frame->arg_count - 1) {
                printf(", ");
            }
        }
    }
    if (has_bracket) {
        printf(")");
    }

    /* return value */
    if (frame->type == PT_FRAME_EXIT && frame->retval) {
        if (has_color()) {
            printf(" = " PTC_MAGENTA "%s" PTC_RESET, frame->retval);
        } else {
            printf(" = %s", frame->retval);
        }
    }

    /* TODO output relative filepath */
    if (has_color()) {
        printf(" called at [" PTC_BBLACK "%s:%d" PTC_RESET "]", frame->filename, frame->lineno);
    } else {
        printf(" called at [%s:%d]", frame->filename, frame->lineno);
    }

    if (frame->type == PT_FRAME_EXIT) {
        printf(" ~ %.3fs %.3fs\n", frame->inc_time / 1000000.0, frame->exc_time / 1000000.0);
    } else {
        printf("\n");
    }
}

/* pt_request */
size_t pt_type_len_request(pt_request_t *request)
{
    /* TODO support sds, str both */
    int i;
    size_t size = 0;

    size += sizeof(uint8_t);                            /* type */
    size += LEN_STR(request->sapi);                     /* sapi_name */
    size += LEN_STR(request->script);                   /* script */
    size += sizeof(uint64_t);                           /* time */

    size += LEN_STR(request->method);                   /* method */
    size += LEN_STR(request->uri);                      /* uri */

    size += sizeof(uint32_t);                           /* argc */
    for (i = 0; i < request->argc; i++) {
        size += LEN_STR(request->argv[i]);              /* argv */
    }

    return size;
}

size_t pt_type_pack_request(pt_request_t *request, char *buf)
{
    /* TODO support sds, str both */
    int i;
    size_t len;
    char *ori = buf;

    PACK(buf, uint8_t,  request->type);
    PACK_STR(buf,       request->sapi);
    PACK_STR(buf,       request->script);
    PACK(buf, uint64_t, request->time);

    PACK_STR(buf,       request->method);
    PACK_STR(buf,       request->uri);

    PACK(buf, uint32_t, request->argc);
    for (i = 0; i < request->argc; i++) {
        PACK_STR(buf,   request->argv[i]);
    }

    return buf - ori;
}

size_t pt_type_unpack_request(pt_request_t *request, char *buf)
{
    int i;
    size_t len;
    char *ori = buf;

    UNPACK(buf, uint8_t,  request->type);
    UNPACK_SDS(buf,       request->sapi);
    UNPACK_SDS(buf,       request->script);
    UNPACK(buf, uint64_t, request->time);

    UNPACK_SDS(buf,       request->method);
    UNPACK_SDS(buf,       request->uri);

    UNPACK(buf, uint32_t, request->argc);
    if (request->argc > 0) {
        request->argv = calloc(request->argc, sizeof(sds));
    } else {
        request->argv = NULL;
    }
    for (i = 0; i < request->argc; i++) {
        UNPACK_SDS(buf,   request->argv[i]);
    }

    return buf - ori;
}

void pt_type_destroy_request(pt_request_t *request)
{
    int i;

    sdsfree(request->sapi);
    sdsfree(request->script);

    sdsfree(request->method);
    sdsfree(request->uri);

    if (request->argc && request->argv) {
        for (i = 0; i < request->argc; i++) {
            sdsfree(request->argv[i]);
        }
        free(request->argv);
    }
}

void pt_type_display_request(pt_request_t *request, const char *format, ...)
{
    int i;
    va_list ap;

    /* format */
    if (format) {
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }

    if (has_color()) {
        printf(PTC_BBLACK "%s " PTC_RESET, request->sapi);
    } else {
        printf("%s ", request->sapi);
    }

    if (request->method) { /* http request */
        if (has_color()) {
            printf(PTC_YELLOW "%s " PTC_GREEN "%s" PTC_RESET, request->method, request->uri);
        } else {
            printf("%s %s", request->method, request->uri);
        }
    } else { /* probable cli mode */
        if (has_color()) {
            printf(PTC_YELLOW "php " PTC_RESET);
        } else {
            printf("php ");
        }
        for (i = 0; i < request->argc; i++) {
            if (i == 0 && has_color()) {
                printf(PTC_GREEN "%s " PTC_RESET, request->argv[i]);
            } else {
                printf("%s ", request->argv[i]);
            }
        }
    }
    printf("\n");
}

/* pt_status */
size_t pt_type_len_status(pt_status_t *status)
{
    int i;
    size_t size = 0;

    size += LEN_STR(status->php_version);                     /* php_version */

    size += sizeof(int64_t);                                  /* mem */
    size += sizeof(int64_t);                                  /* mempeak */
    size += sizeof(int64_t);                                  /* real mem */
    size += sizeof(int64_t);                                  /* real mempeak */

    size += pt_type_len_request(&status->request);            /* request */

    size += sizeof(uint32_t);                                 /* frame count */
    for (i = 0; i < status->frame_count; i++) {
        size += pt_type_len_frame(status->frames + i);
    }

    return size;
}

size_t pt_type_pack_status(pt_status_t *status, char *buf)
{
    int i;
    size_t len;
    char *ori = buf;

    PACK_STR(buf,       status->php_version);

    PACK(buf, int64_t,  status->mem);
    PACK(buf, int64_t,  status->mempeak);
    PACK(buf, int64_t,  status->mem_real);
    PACK(buf, int64_t,  status->mempeak_real);

    len = pt_type_pack_request(&status->request, buf);
    buf += len;

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

    UNPACK(buf, int64_t,  status->mem);
    UNPACK(buf, int64_t,  status->mempeak);
    UNPACK(buf, int64_t,  status->mem_real);
    UNPACK(buf, int64_t,  status->mempeak_real);

    len = pt_type_unpack_request(&status->request, buf);
    buf += len;

    UNPACK(buf, uint32_t, status->frame_count);
    if (status->frame_count > 0) {
        status->frames = calloc(status->frame_count, sizeof(pt_frame_t));
    } else {
        status->frames = NULL;
    }
    for (i = 0; i < status->frame_count; i++) {
        len = pt_type_unpack_frame(status->frames + i, buf);
        buf += len;
    }

    return buf - ori;
}

void pt_type_destroy_status(pt_status_t *status, int free_request)
{
    int i;

    sdsfree(status->php_version);

    if (free_request) {
        pt_type_destroy_request(&status->request);
    }

    if (status->frames && status->frame_count) {
        for (i = 0; i < status->frame_count; i++) {
            pt_type_destroy_frame(status->frames + i);
        }
        free(status->frames);
    }
}

void pt_type_display_status(pt_status_t *status)
{
    int i;
    long elapse;

    /* calculate elapse time */
    elapse = status->request.time ? pt_time_usec() - status->request.time : 0;

    printf("------------------------------- Status --------------------------------\n");
    printf("PHP Version:       %s\n", status->php_version);
    printf("SAPI:              %s\n", status->request.sapi);
    printf("script:            %s\n", status->request.script);
    if (elapse) {
    printf("elapse:            %.3fs\n", elapse / 1000000.0);
    }

    if (status->mem || status->mempeak || status->mem_real || status->mempeak_real) {
    printf("memory:            %.2fm\n", status->mem / 1048576.0);
    printf("memory peak:       %.2fm\n", status->mempeak / 1048576.0);
    printf("real-memory:       %.2fm\n", status->mem_real / 1048576.0);
    printf("real-memory peak   %.2fm\n", status->mempeak_real / 1048576.0);
    }

    if (status->request.method || status->request.uri) {
    printf("------------------------------- Request -------------------------------\n");
    if (status->request.method) {
    printf("request method:    %s\n", status->request.method);
    }
    if (status->request.uri) {
    printf("request uri:       %s\n", status->request.uri);
    }
    }

    if (status->request.argc) {
        printf("------------------------------ Arguments ------------------------------\n");
        for (i = 0; i < status->request.argc; i++) {
            printf("$%-10d        %s\n", i, status->request.argv[i]);
        }
    }

    if (status->frame_count) {
        printf("------------------------------ Backtrace ------------------------------\n");
        for (i = 0; i < status->frame_count; i++) {
            pt_type_display_frame(status->frames + i, 0, "#%-3d", i);
        }
    }
}

