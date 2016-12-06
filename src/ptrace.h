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

#ifndef TRACE_PTRACE_H
#define TRACE_PTRACE_H

#include "trace_type.h"

long pt_ptrace_attach(pid_t pid);

long pt_ptrace_detach(pid_t pid);

void *pt_ptrace_fetch_current_ex(void *addr_executor_globals, pid_t pid);

int pt_ptrace_build_status(pt_status_t *status, void *addr_root_ex, 
        void *addr_sapi_module, void *addr_sapi_globals, pid_t pid);

int pt_ptrace_build_request(pt_request_t *request, void *addr_sapi_module, 
        void *addr_sapi_globals, pid_t pid);

int pt_ptrace_build_frame(pt_frame_t *frame, void *addr_current_ex, pid_t pid);

#endif
