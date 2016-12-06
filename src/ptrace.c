/**
 * Copyright 2016 Yuchen Wang <phobosw@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>

#include "ptrace.h"

#ifndef ZEND_EVAL
#define ZEND_EVAL               (1<<0)
#define ZEND_INCLUDE            (1<<1)
#define ZEND_INCLUDE_ONCE       (1<<2)
#define ZEND_REQUIRE            (1<<3)
#define ZEND_REQUIRE_ONCE       (1<<4)
#endif

#ifndef ZEND_USER_CODE
/* A quick check (type == ZEND_USER_FUNCTION || type == ZEND_EVAL_CODE) */
#define ZEND_USER_CODE(type) ((type & 1) == 0)
#endif

static int ptrace_fetch_str(pid_t pid, void *addr, char *buf, size_t size)
{
    char *p;
    int i, j;
    long val;

    for (i = 0; i < size; addr += sizeof(long)) {
        val = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);
        if (val == -1) {
            return -1;
        }

        p = (char *) &val;
        for (j = 0; j < sizeof(long) && i < size; j++) {
            buf[i++] = *p++;
        }
    }
    buf[i] = '\0';

    return i;
}

/* quick ptrace fetch */
#define fetch_long(addr)    ptrace(PTRACE_PEEKDATA, pid, (void *) addr, NULL)
#define fetch_int(addr)     (int) (fetch_long(addr) & 0xFFFFFFFF)
#define fetch_ptr(addr)     (void *) fetch_long(addr)

#define fetch_str(addr, buf, size)  ptrace_fetch_str(pid, (void *) addr, buf, size)
#define fetch_zstr(addr, buf)       fetch_str(addr + 24, buf, fetch_long(addr + 16))

long pt_ptrace_attach(pid_t pid)
{
    return ptrace(PTRACE_ATTACH, pid, NULL, NULL);
}

long pt_ptrace_detach(pid_t pid)
{
    return ptrace(PTRACE_DETACH, pid, NULL, NULL);
}

void *pt_ptrace_fetch_current_ex(void *addr_executor_globals, pid_t pid)
{
    return fetch_ptr(addr_executor_globals + 480); /* NULL means PHP is in-active */
}

int pt_ptrace_build_status(pt_status_t *status, void *addr_root_ex, 
        void *addr_sapi_module, void *addr_sapi_globals, pid_t pid)
{
    int i;
    void *addr_current_ex;

    memset(status, 0, sizeof(pt_status_t));

    /* request */
    pt_ptrace_build_request(&status->request, addr_sapi_module, addr_sapi_globals, pid);

    /* calculate stack depth */
    for (i = 0, addr_current_ex = addr_root_ex; addr_current_ex; i++) {
        addr_current_ex = fetch_ptr(addr_current_ex + 56); /* prev */
    }
    status->frame_count = i;

    if (status->frame_count) {
        status->frames = calloc(status->frame_count, sizeof(pt_frame_t));
        for (i = 0, addr_current_ex = addr_root_ex; i < status->frame_count && addr_current_ex; i++) {
            pt_ptrace_build_frame(status->frames + i, addr_current_ex, pid);
            addr_current_ex = fetch_ptr(addr_current_ex + 56); /* prev */
        }
    } else {
        status->frames = NULL;
    }

    return 0;
}

int pt_ptrace_build_request(pt_request_t *request, void *addr_sapi_module, 
        void *addr_sapi_globals, pid_t pid)
{
    int i;
    void *addr;
    static char buf[4096];

    /* init */
    memset(request, 0, sizeof(pt_request_t));

    request->type = PT_FRAME_STACK;

    /* sapi_module.name */
    addr = fetch_ptr(addr_sapi_module + 0);
    if (addr != NULL) {
        fetch_str(addr, buf, sizeof(buf));
        request->sapi = sdsnew(buf);
    }

    /* sapi_globals request_info->path_translated */
    addr = fetch_ptr(addr_sapi_globals + 40);
    if (addr != NULL) {
        fetch_str(addr, buf, sizeof(buf));
        request->script = sdsnew(buf);
    }

    /* sapi_globals request_info->request_method */
    addr = fetch_ptr(addr_sapi_globals + 8);
    if (addr != NULL) {
        fetch_str(addr, buf, sizeof(buf));
        request->method = sdsnew(buf);
    }

    /* sapi_globals request_info->request_uri */
    addr = fetch_ptr(addr_sapi_globals + 48);
    if (addr != NULL) {
        fetch_str(addr, buf, sizeof(buf));
        request->uri = sdsnew(buf);
    }

    /* argc, argv */
    request->argc = fetch_int(addr_sapi_globals + 140);
    if (request->argc) {
        request->argv = calloc(request->argc, sizeof(sds));
        addr = fetch_ptr(addr_sapi_globals + 144);
        for (i = 0; i < request->argc; i++) {
            fetch_str(addr + sizeof(char *) * i, buf, sizeof(buf));
            request->argv[i] = sdsnew(buf);
        }
    }

    return 0;
}

int pt_ptrace_build_frame(pt_frame_t *frame, void *addr_current_ex, pid_t pid)
{
    static char buf[4096];

    memset(frame, 0, sizeof(pt_frame_t));

    /* types, level */
    frame->type = PT_FRAME_STACK;
    /* TODO frame->functype = internal ? PT_FUNC_INTERNAL : 0x00; */
    frame->level = 1;

    /* args init */
    frame->arg_count = 0;
    frame->args = NULL;

    void *addr_prev_ex = fetch_ptr(addr_current_ex + 56);

    void *addr_ex_func = fetch_ptr(addr_current_ex + 24);
    void *addr_ex_func_name = fetch_ptr(addr_ex_func + 8);
    void *addr_ex_func_scope = fetch_ptr(addr_ex_func + 16);
    void *addr_ex_This = fetch_ptr(addr_current_ex + 32);

    void *addr_caller_ex = addr_prev_ex ? addr_prev_ex : addr_current_ex;
    void *addr_caller_ex_opline = fetch_ptr(addr_caller_ex + 0);
    void *addr_caller_ex_func = fetch_ptr(addr_caller_ex + 24);
    void *addr_caller_ex_func_oparray_filename = fetch_ptr(addr_caller_ex_func + 120);

    /* names */
    if (addr_ex_func_name) {
        /* function name */
        fetch_zstr(addr_ex_func_name, buf);
        frame->function = sdsnew(buf);

        /*  functype, class name */
        if (addr_ex_func_scope) {
            void *addr_ex_func_scope_name = fetch_ptr(addr_ex_func_scope + 8);

            fetch_zstr(addr_ex_func_scope_name, buf);
            frame->class = sdsnew(buf);

            frame->functype |= addr_ex_This ? PT_FUNC_MEMBER : PT_FUNC_STATIC;
        } else {
            frame->functype |= PT_FUNC_NORMAL;
        }
    } else {
        long ev = 0;

        if (addr_caller_ex && addr_caller_ex_opline) {
            ev = fetch_long(addr_caller_ex_opline + 20);
        }

        /* special user function */
        switch (ev) {
            case ZEND_INCLUDE_ONCE:
                frame->functype |= PT_FUNC_INCLUDE_ONCE;
                frame->function = "include_once";
                break;
            case ZEND_REQUIRE_ONCE:
                frame->functype |= PT_FUNC_REQUIRE_ONCE;
                frame->function = "require_once";
                break;
            case ZEND_INCLUDE:
                frame->functype |= PT_FUNC_INCLUDE;
                frame->function = "include";
                break;
            case ZEND_REQUIRE:
                frame->functype |= PT_FUNC_REQUIRE;
                frame->function = "require";
                break;
            case ZEND_EVAL:
                frame->functype |= PT_FUNC_EVAL;
                frame->function = "{eval}";
                break;
            default:
                /* should be function main */
                frame->functype |= PT_FUNC_NORMAL;
                frame->function = "{main}";
                break;
        }
        frame->function = sdsnew(frame->function);
    }

    /* lineno */
    if (addr_caller_ex && addr_caller_ex_opline) {
        frame->lineno = fetch_int(addr_caller_ex_opline + 24);
    } else {
        frame->lineno = 0;
    }

    /* filename */
    unsigned char caller_func_type = fetch_long(addr_caller_ex_func + 0) & 0xFF;
    if (addr_caller_ex_func_oparray_filename && ZEND_USER_CODE(caller_func_type)) {
        fetch_zstr(addr_caller_ex_func_oparray_filename, buf);
        frame->filename = sdsnew(buf);
    } else {
        frame->filename = NULL;
    }

    return 0;
}
