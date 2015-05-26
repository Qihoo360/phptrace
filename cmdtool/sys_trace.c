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
#include "trace_util.h" /* for hexstring2long() */

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

int sys_fetch_php_address(pid_t pid, long *addr_sg, long *addr_eg)
{
    FILE *fp;
    char buf[256];
    long addr;

    /* prepare command, requirement: procfs, gdb, awk */
    sprintf(buf, "gdb --batch -nx /proc/%d/exe %d "
                 "-ex 'print &(sapi_globals)' "
                 "-ex 'print &(executor_globals)' "
                 "2>/dev/null | awk '$1 ~ /^\\$[0-9]/ {print $NF}'", pid, pid);

    /* open process */
    log_printf(LL_DEBUG, "popen cmd: %s", buf);
    if ((fp = popen(buf, "r")) == NULL) {
        log_printf(LL_DEBUG, "popen failed");
        return -1;
    }

    /* sapi_globals */
    addr = 0;
    if (fgets(buf, sizeof(buf), fp) == NULL || (addr = hexstring2long(buf, strlen(buf) - 1)) == -1) { /* strip '\n' */
        if (addr != -1) {
            log_printf(LL_DEBUG, "popen fgets failed");
        } else {
            log_printf(LL_DEBUG, "convert hex string to long failed \"%s\"", buf);
        }
        pclose(fp);
        return -1;
    }
    log_printf(LL_DEBUG, "popen fgets: %s", buf);
    *addr_sg = addr;

    /* executor_globals */
    if (fgets(buf, sizeof(buf), fp) == NULL || (addr = hexstring2long(buf, strlen(buf) - 1)) == -1) {
        /* TODO it's totally duplicated code here, refactor it if we want
         * process more address */
        if (addr != -1) {
            log_printf(LL_DEBUG, "popen fgets failed");
        } else {
            log_printf(LL_DEBUG, "convert hex string to long failed \"%s\"", buf);
        }
        pclose(fp);
        return -1;
    }
    log_printf(LL_DEBUG, "popen fgets: %s", buf);
    *addr_eg = addr;

    pclose(fp);
    return 0;
}

int sys_fetch_php_versionid(pid_t pid)
{
    FILE *fp;
    char buf[256];
    int version_id;

    /* prepare command, requirement: procfs, awk */
    sprintf(buf, "/proc/%d/exe -v | awk 'NR == 1 {print $2}'", pid);

    /* open process */
    log_printf(LL_DEBUG, "popen cmd: %s", buf);
    if ((fp = popen(buf, "r")) == NULL) {
        log_printf(LL_DEBUG, "popen failed");
        return -1;
    }

    /* read output */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        log_printf(LL_DEBUG, "popen fgets failed");
        pclose(fp);
        return -1;
    }
    log_printf(LL_DEBUG, "popen fgets: %s", buf);
    pclose(fp);

    /* convert version string "5.6.3" to id 50603 */
    if (strlen(buf) < 5 || buf[0] != '5' || buf[1] != '.' || buf[3] != '.') {
        return -1;
    }
    version_id = 50000;
    version_id += (buf[2] - '0') * 100;
    version_id += (buf[4] - '0') * 10;
    version_id += (buf[5] - '0') * 1;

    return version_id;
}
