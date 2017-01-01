/**
 * Copyright 2017 Yuchen Wang <phobosw@gmail.com>
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

static inline long ptrace_fetch_long(pid_t pid, void *addr)
{
    /* some debug code here, just this... */
    return ptrace(PTRACE_PEEKDATA, pid, (void *) addr, NULL);
}

/* quick ptrace fetch */
#define fetch_long(addr)    ptrace_fetch_long(pid, (void *) addr)
#define fetch_int(addr)     (int) (fetch_long(addr) & 0xFFFFFFFF)
#define fetch_ptr(addr)     (void *) fetch_long(addr)

#define fetch_str(addr, buf, size)  ptrace_fetch_str(pid, (void *) addr, buf, size)
#define fetch_zstr(addr, buf)       fetch_str(addr + 24, buf, fetch_long(addr + 16))

/* TODO store presets in local file */
static pt_ptrace_preset_t presets[] = {
    {70012, 1,/**/0, 40, 8, 48, 140, 144, 440,/**/480, 0, 56, 24, 32,/**/8, 16, 120, 8, 20, 24},
    {50619, 0,/**/0, 40, 8, 48, 140, 144, 440,/**/1120, 0, 48, 8, 32,/**/8, 16, 152, 8, 32, 40},
    {50533, 0,/**/0, 64, 8, 72, 156, 160, 448,/**/1120, 0, 48, 8, 32,/**/8, 16, 152, 8, 32, 40},
};

pt_ptrace_preset_t *pt_ptrace_preset(int version, void *addr_sapi_module,
        void *addr_sapi_globals, void *addr_executor_globals)
{
    int i;

    for (i = 0; i < sizeof(presets); i++) {
        if (version >= presets[i].version) {
            presets[i].sapi_module = addr_sapi_module;
            presets[i].sapi_globals = addr_sapi_globals;
            presets[i].executor_globals = addr_executor_globals;

            return &presets[i];
        }
    }

    return NULL;
}

long pt_ptrace_attach(pid_t pid)
{
    return ptrace(PTRACE_ATTACH, pid, NULL, NULL);
}

long pt_ptrace_detach(pid_t pid)
{
    return ptrace(PTRACE_DETACH, pid, NULL, NULL);
}

void *pt_ptrace_fetch_current_ex(pt_ptrace_preset_t *preset, pid_t pid)
{
    /* NULL means PHP is in-active */
    return fetch_ptr(preset->executor_globals + preset->EG_current_execute_data);
}

int pt_ptrace_build_status(pt_status_t *status, pt_ptrace_preset_t *preset,
        pid_t pid, void *addr_root_ex, int version_id)
{
    int i;
    void *addr_current_ex;

    memset(status, 0, sizeof(pt_status_t));

    /* version */
    status->php_version = sdscatprintf(sdsempty(), "%d.%d.%d",
            version_id / 10000, version_id % 10000 / 100, version_id % 100);

    /* request */
    pt_ptrace_build_request(&status->request, preset, pid);

    /* calculate stack depth */
    for (i = 0, addr_current_ex = addr_root_ex; addr_current_ex; i++) {
        addr_current_ex = fetch_ptr(addr_current_ex + preset->EX_prev_execute_data); /* prev */
    }
    status->frame_count = i;

    if (status->frame_count) {
        status->frames = calloc(status->frame_count, sizeof(pt_frame_t));
        for (i = 0, addr_current_ex = addr_root_ex; i < status->frame_count && addr_current_ex; i++) {
            pt_ptrace_build_frame(status->frames + i, preset, pid, addr_current_ex);
            addr_current_ex = fetch_ptr(addr_current_ex + preset->EX_prev_execute_data); /* prev */
        }
    } else {
        status->frames = NULL;
    }

    return 0;
}

int pt_ptrace_build_request(pt_request_t *request, pt_ptrace_preset_t *preset,
        pid_t pid)
{
    int i;
    long val;
    void *addr;
    static char buf[4096];

    /* init */
    memset(request, 0, sizeof(pt_request_t));

    request->type = PT_FRAME_STACK;

    /* sapi_module.name */
    addr = fetch_ptr(preset->sapi_module + preset->SM_name);
    if (addr != NULL) {
        fetch_str(addr, buf, sizeof(buf));
        request->sapi = sdsnew(buf);
    }

    /* sapi_globals request_info->path_translated */
    addr = fetch_ptr(preset->sapi_globals + preset->SG_request_info_path_translated);
    if (addr != NULL) {
        fetch_str(addr, buf, sizeof(buf));
        request->script = sdsnew(buf);
    }

    /* sapi_globals request_info->request_method */
    addr = fetch_ptr(preset->sapi_globals + preset->SG_request_info_request_method);
    if (addr != NULL) {
        fetch_str(addr, buf, sizeof(buf));
        request->method = sdsnew(buf);
    }

    /* sapi_globals request_info->request_uri */
    addr = fetch_ptr(preset->sapi_globals + preset->SG_request_info_request_uri);
    if (addr != NULL) {
        fetch_str(addr, buf, sizeof(buf));
        request->uri = sdsnew(buf);
    }

    /* argc, argv */
    request->argc = fetch_int(preset->sapi_globals + preset->SG_request_info_argc);
    if (request->argc) {
        request->argv = calloc(request->argc, sizeof(sds));
        addr = fetch_ptr(preset->sapi_globals + preset->SG_request_info_argv);
        for (i = 0; i < request->argc; i++) {
            fetch_str(addr + sizeof(char *) * i, buf, sizeof(buf));
            request->argv[i] = sdsnew(buf);
        }
    }

    /* sapi_globals global_request_time */
    val = fetch_long(preset->sapi_globals + preset->SG_global_request_time);
    request->time = (long) (*(double *) &val) * 1000000l;

    return 0;
}

int pt_ptrace_build_frame(pt_frame_t *frame, pt_ptrace_preset_t *preset,
        pid_t pid, void *addr_current_ex)
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

    void *addr_prev_ex = fetch_ptr(addr_current_ex + preset->EX_prev_execute_data);

    void *addr_ex_func = fetch_ptr(addr_current_ex + preset->EX_func);
    void *addr_ex_func_name = fetch_ptr(addr_ex_func + preset->EX_FUNC_name);
    void *addr_ex_func_scope = fetch_ptr(addr_ex_func + preset->EX_FUNC_scope);
    void *addr_ex_This = fetch_ptr(addr_current_ex + preset->EX_This);

    void *addr_caller_ex = addr_prev_ex ? addr_prev_ex : addr_current_ex;
    void *addr_caller_ex_opline = fetch_ptr(addr_caller_ex + preset->EX_opline);
    void *addr_caller_ex_func = fetch_ptr(addr_caller_ex + preset->EX_func);
    void *addr_caller_ex_func_oparray_filename = fetch_ptr(addr_caller_ex_func +
            preset->EX_FUNC_oparray_filename);

    /* names */
    if (addr_ex_func_name) {
        /* function name */
        if (preset->has_zend_string) {
            fetch_zstr(addr_ex_func_name, buf);
        } else {
            fetch_str(addr_ex_func_name, buf, sizeof(buf));
        }
        frame->function = sdsnew(buf);

        /*  functype, class name */
        if (addr_ex_func_scope) {
            void *addr_ex_func_scope_name = fetch_ptr(addr_ex_func_scope + preset->EX_FUNC_SCOPE_name);

            if (preset->has_zend_string) {
                fetch_zstr(addr_ex_func_scope_name, buf);
            } else {
                fetch_str(addr_ex_func_scope_name, buf, sizeof(buf));
            }
            frame->class = sdsnew(buf);

            frame->functype |= addr_ex_This ? PT_FUNC_MEMBER : PT_FUNC_STATIC;
        } else {
            frame->functype |= PT_FUNC_NORMAL;
        }
    } else {
        long ev = 0;

        if (addr_caller_ex && addr_caller_ex_opline) {
            ev = fetch_long(addr_caller_ex_opline + preset->EX_OPLINE_extended_value);
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
        frame->lineno = fetch_int(addr_caller_ex_opline + preset->EX_OPLINE_lineno);
    } else {
        frame->lineno = 0;
    }

    /* filename */
    unsigned char caller_func_type = fetch_long(addr_caller_ex_func + 0) & 0xFF;
    if (addr_caller_ex_func_oparray_filename && ZEND_USER_CODE(caller_func_type)) {
        if (preset->has_zend_string) {
            fetch_zstr(addr_caller_ex_func_oparray_filename, buf);
        } else {
            fetch_str(addr_caller_ex_func_oparray_filename, buf, sizeof(buf));
        }
        frame->filename = sdsnew(buf);
    } else {
        frame->filename = NULL;
    }

    return 0;
}
