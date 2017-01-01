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

#ifndef TRACE_PTRACE_H
#define TRACE_PTRACE_H

#include "trace_type.h"

typedef struct {
    int version;
    int has_zend_string;

    /* The naming convention for offset of a element in struct:
     * for SG_request_info_argc
     * SG is a abbr for sapi_globals, is the struct, MUST be capital
     * request_info_argc is element's name */
    long SM_name;
    long SG_request_info_path_translated;
    long SG_request_info_request_method;
    long SG_request_info_request_uri;
    long SG_request_info_argc;
    long SG_request_info_argv;
    long SG_global_request_time;

    long EG_current_execute_data;
    long EX_opline;
    long EX_prev_execute_data;
    long EX_func;
    long EX_This;

    long EX_FUNC_name;
    long EX_FUNC_scope;
    long EX_FUNC_oparray_filename;
    long EX_FUNC_SCOPE_name;
    long EX_OPLINE_extended_value;
    long EX_OPLINE_lineno;

    void *sapi_module;
    void *sapi_globals;
    void *executor_globals;
} pt_ptrace_preset_t;

pt_ptrace_preset_t *pt_ptrace_preset(int version, void *addr_sapi_module,
        void *addr_sapi_globals, void *addr_executor_globals);

long pt_ptrace_attach(pid_t pid);

long pt_ptrace_detach(pid_t pid);

void *pt_ptrace_fetch_current_ex(pt_ptrace_preset_t *preset, pid_t pid);

int pt_ptrace_build_status(pt_status_t *status, pt_ptrace_preset_t *preset,
        pid_t pid, void *addr_root_ex, int version_id);

int pt_ptrace_build_request(pt_request_t *request, pt_ptrace_preset_t *preset,
        pid_t pid);

int pt_ptrace_build_frame(pt_frame_t *frame, pt_ptrace_preset_t *preset,
        pid_t pid, void *addr_current_ex);

#endif
