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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "log.h"
#include "sys_trace.h"

int sys_trace_attach(pid_t pid)
{
    if (0 > ptrace(PTRACE_ATTACH, pid, 0, 0)) {
        log_printf(LL_ERROR, "failed to ptrace(ATTACH) child %d (%s)", pid, (errno ? strerror(errno) : "null"));
        return -1;
    }
    return 0;
}

int sys_trace_detach(pid_t pid)
{
    if (0 > ptrace(PTRACE_DETACH, pid, (void *) 1, 0)) {
        log_printf(LL_ERROR, "failed to ptrace(DETACH) child %d (%s)", pid, (errno ? strerror(errno) : "null"));
        return -1;
    }
    return 0;
}

int sys_trace_get_long(pid_t pid, long addr, long *data)
{
#ifdef PT_IO
    struct ptrace_io_desc ptio = {
        .piod_op = PIOD_READ_D,
        .piod_offs = (void *) addr,
        .piod_addr = (void *) data,
        .piod_len = sizeof(long)
    };

    if (0 > ptrace(PT_IO, pid, (void *) &ptio, 0)) {
        log_printf(LL_ERROR, "failed to ptrace(PT_IO) pid %d (%s)", pid, (errno ? strerror(errno) : "null"));
        return -1;
    }
#else
    *data = ptrace(PTRACE_PEEKDATA, pid, (void *) addr, 0);
    if (errno) {
        log_printf(LL_ERROR, "failed to ptrace(PEEKDATA) pid %d (%s)", pid, strerror(errno));
        return -1;
    }
#endif
    return 0;
}

int sys_trace_get_strz(pid_t pid, char *buf, size_t sz, long addr)
{
    int i;
    long l;
    char *lc = (char *) &l; 

    if (0 > sys_trace_get_long(pid, addr, &l)) {
        return -1; 
    }

    i = l % SIZEOF_LONG;
    l -= i;
    for (addr = l; ; addr += SIZEOF_LONG) {
        if (0 > sys_trace_get_long(pid, addr, &l)) {
            return -1;
        }
        for ( ; i < SIZEOF_LONG; i++) {
            --sz;
            if (sz && lc[i]) {
                *buf++ = lc[i];
                continue;
            }
            *buf = '\0';
            return 0;
        }
        i = 0;
    }
}

int sys_trace_kill(pid_t pid, int sig)
{
    switch (sig) {
        case SIGTERM:
        case SIGSTOP:
        case SIGCONT:
        case SIGQUIT:
            return kill(pid, sig);
        default:
            break;
    }
    return 0;
}
