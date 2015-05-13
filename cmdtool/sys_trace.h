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

#ifndef _SYS_TRACE_H_
#define _SYS_TRACE_H_

#include <signal.h>
#include <sys/ptrace.h>

#if !defined(PTRACE_ATTACH) && defined(PT_ATTACH)
#define PTRACE_ATTACH PT_ATTACH
#endif

#if !defined(PTRACE_DETACH) && defined(PT_DETACH)
#define PTRACE_DETACH PT_DETACH
#endif

#if !defined(PTRACE_PEEKDATA) && defined(PT_READ_D)
#define PTRACE_PEEKDATA PT_READ_D
#endif

#define SIZEOF_LONG		4

#if SIZEOF_LONG == 4
#define PTR_FMT "08"
#elif SIZEOF_LONG == 8
#define PTR_FMT "016"
#endif

int sys_trace_attach(pid_t pid);
int sys_trace_detach(pid_t pid);
int sys_trace_get_long(pid_t pid, long addr, long *data);
int sys_trace_get_strz(pid_t pid, char *buf, size_t sz, long addr);
int sys_trace_kill(pid_t pid, int how);

#endif // _SYS_TRACE_H_
