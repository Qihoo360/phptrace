#include <stdio.h>
#include "fpm_ptrace.h"

static pid_t traced_pid;

int fpm_trace_signal(pid_t pid)
{
	if (0 > ptrace(PTRACE_ATTACH, pid, 0, 0)) {
		//zlog(ZLOG_SYSERROR, "failed to ptrace(ATTACH) child %d", pid);
		printf("failed to ptrace(ATTACH) child %d", pid);
		return -1;
	}
	return 0;
}

int fpm_trace_ready(pid_t pid)
{
	traced_pid = pid;
	return 0;
}

int fpm_trace_close(pid_t pid)
{
	if (0 > ptrace(PTRACE_DETACH, pid, (void *) 1, 0)) {
		//zlog(ZLOG_SYSERROR, "failed to ptrace(DETACH) child %d", pid);
		printf("failed to ptrace(DETACH) child %d", pid);
		return -1;
	}
	traced_pid = 0;
	return 0;
}

int fpm_trace_get_long(long addr, long *data)
{
#ifdef PT_IO
	struct ptrace_io_desc ptio = {
		.piod_op = PIOD_READ_D,
		.piod_offs = (void *) addr,
		.piod_addr = (void *) data,
		.piod_len = sizeof(long)
	};

	if (0 > ptrace(PT_IO, traced_pid, (void *) &ptio, 0)) {
		//zlog(ZLOG_SYSERROR, "failed to ptrace(PT_IO) pid %d", traced_pid);
		printf("failed to ptrace(PT_IO) pid %d\n", traced_pid);
		return -1;
	}
#else
	*data = ptrace(PTRACE_PEEKDATA, traced_pid, (void *) addr, 0);
	if (errno) {
		printf("failed to ptrace(PEEKDATA) pid %d\n", traced_pid);
		return -1;
	}
#endif
	return 0;
}

int fpm_pctl_kill(pid_t pid, int how)
{
	int s = 0;

	switch (how) {
		case FPM_PCTL_TERM :
			s = SIGTERM;
			break;
		case FPM_PCTL_STOP :
			s = SIGSTOP;
			break;
		case FPM_PCTL_CONT :
			s = SIGCONT;
			break;
		case FPM_PCTL_QUIT :
			s = SIGQUIT;
			break;
		default :
			break;
	}
	return kill(pid, s);
}

