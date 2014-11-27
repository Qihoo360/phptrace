#ifndef _FPM_PTRACE_H_
#define _FPM_PTRACE_H_

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <fcntl.h>

enum {
        FPM_PCTL_TERM,
        FPM_PCTL_STOP,
        FPM_PCTL_CONT,
        FPM_PCTL_QUIT
};

int fpm_trace_signal(pid_t pid);
int fpm_trace_ready(pid_t pid);
int fpm_trace_close(pid_t pid);
int fpm_trace_get_long(long addr, long *data);
int fpm_pctl_kill(pid_t pid, int how);

#endif // _FPM_PTRACE_H_
