#ifndef _SYS_TRACE_H_
#define _SYS_TRACE_H_

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <fcntl.h>

#include <signal.h>
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
