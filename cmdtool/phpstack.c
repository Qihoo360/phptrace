#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# include <stdint.h>
#endif

//#include <fcntl.h>
//#include <sys/wait.h>
//#include <sys/ptrace.h>
#include "fpm_ptrace.h"

#define SIZEOF_LONG		4

#define valid_ptr(p) ((p) && 0 == ((p) & (sizeof(long) - 1)))

#if SIZEOF_LONG == 4
#define PTR_FMT "08"
#elif SIZEOF_LONG == 8
#define PTR_FMT "016"
#endif

typedef struct _trace_info_t {
    long sapi_globals_addr;
    long sapi_globals_path_offset;
    long executor_globals_addr;
    long executor_globals_ed_offset;
    long execute_data_addr;
    long execute_data_f_offset;
    long function_addr;
    long function_fn_offset;
    long execute_data_prev_offset;
    long execute_data_oparray_offset;
    long oparray_addr;
    long oparray_fn_offset;
    long execute_data_opline_offset;
    long opline_addr;
    long opline_ln_offset;
} trace_info_t;

static trace_info_t traced_info;

long hexstring2long(const char*s, size_t len)
{
	int i;
	long l, n;
	char c;

	//printf("len: %d\n", len);
        if (len <= 2 || s[0] != '0' || s[1] != 'x') {
		return -1;
        }
	l = 0;
        for (i = 2; i < len; i++) {
		c = s[i];
		if (c >= 'a' && c <= 'f') {
			n = (long)c - (long)'a' + 10;
		}
		else if (c >= '0' && c <= '9') {
			n = (long)c - (long)'0';
		}
		l = l * 16 + n;
		//printf("char %ld addr: %ld\n", n, l);
	}
	return l;
}

int fpm_trace_get_strz(char *buf, size_t sz, long addr)
{
	int i;
	long l;
	char *lc = (char *) &l;

	if (0 > fpm_trace_get_long(addr, &l)) {
		return -1;
	}

	i = l % SIZEOF_LONG;
	l -= i;
	for (addr = l; ; addr += SIZEOF_LONG) {
		if (0 > fpm_trace_get_long(addr, &l)) {
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

int parse_args(pid_t* pid, trace_info_t* info, int argc, char** argv)
{
	long addr;
	int len;

	if (!info) {
		printf("invalid info\n");
		return -1;
	}

	if (argc < 14) {
		printf("args wrong\n");
		return -1;
	}

	*pid = atoi(argv[1]);

	if ((addr = hexstring2long(argv[2], strlen(argv[2]))) == -1) {
		printf("invalid hex string %s\n", argv[2]);
		return -1;
	}
	info->sapi_globals_addr = addr;

	if ((addr = hexstring2long(argv[3], strlen(argv[3]))) == -1) {
		printf("invalid hex string %s\n", argv[3]);
		return -1;
	}
	info->sapi_globals_path_offset = addr - info->sapi_globals_addr;

        if ((addr = hexstring2long(argv[4], strlen(argv[4]))) == -1) {
		printf("invalid hex string %s\n", argv[4]);
                return -1;
        }
        info->executor_globals_addr = addr;

        if ((addr = hexstring2long(argv[5], strlen(argv[5]))) == -1) {
		printf("invalid hex string %s\n", argv[5]);
                return -1;
        }
        info->executor_globals_ed_offset = addr - info->executor_globals_addr;

        if ((addr = hexstring2long(argv[6], strlen(argv[6]))) == -1) {
		printf("invalid hex string %s\n", argv[6]);
                return -1;
        }
	info->execute_data_addr = addr;

        if ((addr = hexstring2long(argv[7], strlen(argv[7]))) == -1) {
		printf("invalid hex string %s\n", argv[7]);
                return -1;
        }
	info->execute_data_f_offset = addr - info->execute_data_addr;

        if ((addr = hexstring2long(argv[8], strlen(argv[8]))) == -1) {
		printf("invalid hex string %s\n", argv[8]);
                return -1;
        }
	info->function_addr = addr;

        if ((addr = hexstring2long(argv[9], strlen(argv[9]))) == -1) {
		printf("invalid hex string %s\n", argv[9]);
                return -1;
        }
	info->function_fn_offset = addr - info->function_addr;

        if ((addr = hexstring2long(argv[10], strlen(argv[10]))) == -1) {
		printf("invalid hex string %s\n", argv[10]);
                return -1;
        }
	info->execute_data_prev_offset = addr - info->execute_data_addr;

        if ((addr = hexstring2long(argv[11], strlen(argv[11]))) == -1) {
		printf("invalid hex string %s\n", argv[11]);
                return -1;
        }
	info->execute_data_oparray_offset = addr - info->execute_data_addr;

        if ((addr = hexstring2long(argv[12], strlen(argv[12]))) == -1) {
		printf("invalid hex string %s\n", argv[12]);
                return -1;
        }
	info->oparray_addr = addr;

        if ((addr = hexstring2long(argv[13], strlen(argv[13]))) == -1) {
		printf("invalid hex string %s\n", argv[13]);
                return -1;
        }
	info->oparray_fn_offset = addr - info->oparray_addr;

        if ((addr = hexstring2long(argv[14], strlen(argv[14]))) == -1) {
		printf("invalid hex string %s\n", argv[14]);
                return -1;
        }
	info->execute_data_opline_offset = addr - info->execute_data_addr;

        if ((addr = hexstring2long(argv[15], strlen(argv[15]))) == -1) {
		printf("invalid hex string %s\n", argv[15]);
                return -1;
        }
	info->opline_addr = addr;

        if ((addr = hexstring2long(argv[16], strlen(argv[16]))) == -1) {
		printf("invalid hex string %s\n", argv[16]);
                return -1;
        }
	info->opline_ln_offset = addr - info->opline_addr;
#if 0
	printf("pid %d\n", *pid, (long)addr);
	printf("sapi_globals_addr %lld\n", info->sapi_globals_addr);
	printf("sapi_globals_path_offset %lld\n", info->sapi_globals_path_offset);
	printf("executor_globals_addr %lld\n", info->executor_globals_addr);
	printf("executor_globals_ed_offset %lld\n", info->executor_globals_ed_offset);
	printf("execute_data_addr %lld\n", info->execute_data_addr);
	printf("execute_data_f_offset %lld\n", info->execute_data_f_offset);
	printf("function_addr %lld\n", info->function_addr);
	printf("function_fn_offset %lld\n", info->function_fn_offset);
	printf("execute_data_prev_offset %lld\n", info->execute_data_prev_offset);
	printf("execute_data_oparray_offset %lld\n", info->execute_data_oparray_offset);
	printf("oparray_addr %lld\n", info->oparray_addr);
	printf("oparray_fn_offset %lld\n", info->oparray_fn_offset);
	printf("execute_data_opline_offset %lld\n", info->execute_data_opline_offset);
	printf("opline_addr %lld\n", info->opline_addr);
	printf("opline_ln_offset %lld\n", info->opline_ln_offset);
#endif
	return 0;
}

int trace_dump(pid_t pid)
{
	static const int buf_size = 1024;
	char buf[buf_size];
	long l, execute_data, function;
	int callers_limit = 10;

	printf("phptrace 0.1 demo, published by infra webcore team\n");
	printf("process id = %lld\n", pid);

	//if (0 > fpm_trace_get_strz(buf, buf_size, (long) &SG(request_info).path_translated)) {
	if (0 > fpm_trace_get_strz(buf, buf_size, (long) (traced_info.sapi_globals_addr + traced_info.sapi_globals_path_offset))) {
		//printf("fpm_trace_get_strz failed\n");
		return -1;
	}
	printf("script_filename = %s\n", buf);

	//if (0 > fpm_trace_get_long((long) &EG(current_execute_data), &l)) {
	if (0 > fpm_trace_get_long(traced_info.executor_globals_addr + traced_info.executor_globals_ed_offset, &l)) {
		printf("fpm_trace_get_long failed\n");
		return -1;
	}
	//printf("l: %lld, execute_data_addr: %lld\n", l, traced_info.execute_data_addr);
	traced_info.execute_data_addr = l;
	execute_data = traced_info.execute_data_addr;

	while (execute_data) {
		printf("[0x%" PTR_FMT "lx] ", execute_data);

		if (0 > fpm_trace_get_long(execute_data + traced_info.execute_data_f_offset, &l)) {
			printf("fpm_trace_get_long failed\n");
			return -1;
		}
		traced_info.function_addr = l;

		if (valid_ptr(traced_info.function_addr)) {
			if (0 > fpm_trace_get_strz(buf, buf_size, (long) (traced_info.function_addr + traced_info.function_fn_offset))) {
				printf("fpm_trace_get_strz failed\n");
				return -1;
			}
			printf(" %s", buf);
		}

		if (0 > fpm_trace_get_long(execute_data + traced_info.execute_data_oparray_offset, &l)) {
			printf("fpm_trace_get_long failed\n");
			return -1;
		}
		traced_info.oparray_addr = l;

		if (valid_ptr(traced_info.oparray_addr)) {
			if (0 > fpm_trace_get_strz(buf, buf_size, (long) (traced_info.oparray_addr + traced_info.oparray_fn_offset))) {
				printf("fpm_trace_get_strz failed\n");
				return -1;
			}
			printf(" %s", buf);
		}
		
		if (0 > fpm_trace_get_long(execute_data + traced_info.execute_data_opline_offset, &l)) {
			printf("fpm_trace_get_long failed\n");
			return -1;
		}
		traced_info.opline_addr = l;
		if (valid_ptr(traced_info.opline_addr)) {
			uint *lu = (uint *) &l;
			uint lineno;
			if (0 > fpm_trace_get_long(traced_info.opline_addr + traced_info.opline_ln_offset, &l)) {
				printf("fpm_trace_get_strz failed\n");
				return -1;
			}
			lineno = *lu;
			printf(":%d", lineno);
		}

		printf("\n");


		if (0 > fpm_trace_get_long(execute_data + traced_info.execute_data_prev_offset, &l)) {
			printf("fpm_trace_get_long failed\n");
			return -1;
		}
		execute_data = l;

		if (--callers_limit == 0)
			break;
	}

	return 0;
}


//static int fpm_php_trace_dump(struct fpm_child_s *child, FILE *slowlog TSRMLS_DC)
//{
//	int callers_limit = 20;
//	pid_t pid = child->pid;
//	struct timeval tv;
//	static const int buf_size = 1024;
//	char buf[buf_size];
//	long execute_data;
//	long l;
//
//	gettimeofday(&tv, 0);
//
//	zlog_print_time(&tv, buf, buf_size);
//
//	fprintf(slowlog, "\n%s [pool %s] pid %d\n", buf, child->wp->config->name, (int) pid);
//
//	if (0 > fpm_trace_get_strz(buf, buf_size, (long) &SG(request_info).path_translated)) {
//		return -1;
//	}
//
//	fprintf(slowlog, "script_filename = %s\n", buf);
//
//	if (0 > fpm_trace_get_long((long) &EG(current_execute_data), &l)) {
//		return -1;
//	}
//
//	execute_data = l;
//
//	while (execute_data) {
//		long function;
//		uint lineno = 0;
//
//		fprintf(slowlog, "[0x%" PTR_FMT "lx] ", execute_data);
//
//		if (0 > fpm_trace_get_long(execute_data + offsetof(zend_execute_data, function_state.function), &l)) {
//			return -1;
//		}
//
//		function = l;
//
//		if (valid_ptr(function)) {
//			if (0 > fpm_trace_get_strz(buf, buf_size, function + offsetof(zend_function, common.function_name))) {
//				return -1;
//			}
//
//			fprintf(slowlog, "%s()", buf);
//		} else {
//			fprintf(slowlog, "???");
//		}
//
//		if (0 > fpm_trace_get_long(execute_data + offsetof(zend_execute_data, op_array), &l)) {
//			return -1;
//		}
//
//		*buf = '\0';
//
//		if (valid_ptr(l)) {
//			long op_array = l;
//
//			if (0 > fpm_trace_get_strz(buf, buf_size, op_array + offsetof(zend_op_array, filename))) {
//				return -1;
//			}
//		}
//
//		if (0 > fpm_trace_get_long(execute_data + offsetof(zend_execute_data, opline), &l)) {
//			return -1;
//		}
//
//		if (valid_ptr(l)) {
//			long opline = l;
//			uint *lu = (uint *) &l;
//
//			if (0 > fpm_trace_get_long(opline + offsetof(struct _zend_op, lineno), &l)) {
//				return -1;
//			}
//
//			lineno = *lu;
//		}
//
//		fprintf(slowlog, " %s:%u\n", *buf ? buf : "unknown", lineno);
//
//		if (0 > fpm_trace_get_long(execute_data + offsetof(zend_execute_data, prev_execute_data), &l)) {
//			return -1;
//		}
//
//		execute_data = l;
//
//		if (0 == --callers_limit) {
//			break;
//		}
//	}
//	return 0;
//}

int main(int argc, char** argv)
{
	int ret;
	pid_t pid;
	int retry;

	if (parse_args(&pid, &traced_info, argc, argv) == -1) {
		return -1;
	}

	if (fpm_trace_signal(pid)) {
		printf("fpm_trace_signal failed\n");
		return -1;
	}

	ret = 0;
	retry = 3;
	do {
		if (fpm_trace_ready(pid)) {
			printf("fpm_trace_ready failed\n");
			ret = -1;
			break;
		}

		while (--retry) {
			if ((ret = trace_dump(pid)) >= 0) {
				break;
			}
			sleep(1);
		}
		if (ret < 0) {
			printf("failed trace_dump\n");
		}
	} while (0);

	fpm_trace_close(pid);
        fpm_pctl_kill(pid, FPM_PCTL_CONT);
	return ret;
}


