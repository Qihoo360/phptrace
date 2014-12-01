/*
 *  trace cmdtools 
 */

#include "../util/phptrace_mmap.h"

//#include "p_object.h"
#include "../util/phptrace_protocol.h"
#include "../util/phptrace_string.h"

#include <stdarg.h>

static char *progname;

static phptrace_str_t *map_filename_next;
static phptrace_str_t *map_filename_prev;
static int phptrace_mmap_again;
static int phptrace_tracer_pid = 0;
static int phptrace_php_pid = 0;
static int phptrace_mmap_cnt = 0; 
int map_filename_free_flag = 0; /* @TODO because first map filename is malloc, we need to free it */
int seq = -1;

uint64_t phptrace_start_time;

int phptrace_record_cnt = 0;

phptrace_file_t *file;
//phptrace_stack_t *stack;
phptrace_segment_t seg;
phptrace_ctrl_t *ctrl;

/* not used */
static unsigned int nprocs = 0;
static FILE *shared_log;

//static volatile int interrupted;


#define MAX_TEMP_LENGTH 20
#define MMAP_LENGTH  (1024 * 1024)

//#define MIN_RECORDS_NUMBER 100
//#define MAX_RECORDS_NUMBER 100

#define CTRL_WAIT_INTERVAL 100   			/* ms */
#define MAX_CTRL_WAIT_INTERVAL (CTRL_WAIT_INTERVAL * 16)  /* ms */
#define DATA_WAIT_INTERVAL 10   			/* ms */
#define MAX_DATA_WAIT_INTERVAL (DATA_WAIT_INTERVAL * 20)  /* ms */ 

#define grow_interval(t, mx) (((t)<<1) > (mx) ? (mx) : ((t) << 1))

#define BASE_FUNCTION_LEVEL 0
#define MAX_FUNCTION_LEVEL 100

enum {
	PHPTRACE_OK = 0,
	PHPTRACE_ERROR,
	PHPTRACE_AGAIN,
	PHPTRACE_IGNORE
};

enum {
	STATE_START = 0,
	STATE_OPEN,
	STATE_HEADER,
	STATE_RECORD,
//	STATE_RECORD_DEEP,
	STATE_TAILER,
	STATE_ERROR
};


static void cleanup()
{
	phptrace_unmap(&seg);
	//phptrace_stack_free(stack);
	phptrace_ctrl_free(ctrl, phptrace_php_pid);
	phptrace_file_free(file);
	if (map_filename_free_flag)  /* ugly. free first malloc string */
	{
		map_filename_free_flag = 0;
		if (map_filename_prev) free(map_filename_prev);
		if (map_filename_next) free(map_filename_next);
	}

#ifdef DEBUG
	printf ("[debug]~~~cleanup~~~~~\n");
#endif 
}

/* trace error utils */
static void verror_msg(int err_no, const char *fmt, va_list p)
{
	char *msg;
	fflush(NULL);
	/* We want to print entire message with single fprintf to ensure
	 * message integrity if stderr is shared with other programs.
	 * Thus we use vasprintf + single fprintf.
	 */
	msg = NULL;
	if (vasprintf(&msg, fmt, p) >= 0) {
		if (err_no)
			fprintf(stderr, "%s: %s: %s\n", progname, msg, strerror(err_no));
		else
			fprintf(stderr, "%s: %s\n", progname, msg);
		free(msg);
	} else {
		/* malloc in vasprintf failed, try it without malloc */
		fprintf(stderr, "%s: ", progname);
		vfprintf(stderr, fmt, p);
		if (err_no)
			fprintf(stderr, ": %s\n", strerror(err_no));
		else
			putc('\n', stderr);
	}
	/* We don't switch stderr to buffered, thus fprintf(stderr)
	 * always flushes its output and this is not necessary: */
	/* fflush(stderr); */
}

uint64_t phptrace_time_usec()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}
void error_msg(const char *fmt, ...)
{
	va_list p;
	va_start(p, fmt);
	verror_msg(0, fmt, p);
	va_end(p);
}
static void die(int exit_code)
{
	//if (phptrace_tracer_pid == getpid()) {
		cleanup();
	//}
	exit(exit_code);
}
void error_msg_and_die(const char *fmt, ...)
{
	va_list p;
	va_start(p, fmt);
	verror_msg(0, fmt, p);
	die(PHPTRACE_ERROR);
}

static void process_opt_p(char *optarg)
{
	uint8_t num = -1;
	uint8_t a = 1;

	phptrace_php_pid = string2uint(optarg);
	if (phptrace_php_pid <= 0 || phptrace_php_pid > MAX_PROCESS_NUMBER || phptrace_php_pid > ctrl->ctrl_seg.size) {
		error_msg_and_die("Invalid process id: '%s'", optarg);
	}
	if (phptrace_php_pid == phptrace_tracer_pid) {
		error_msg_and_die("Sorry not a php process.");
	}
	phptrace_ctrl_get(ctrl, &num, phptrace_php_pid);
#ifdef DEBUG
	printf ("[debug] read ctrl data: %d, not zero will exit!\n", num);
#endif
	if (num > 0) {
		error_msg_and_die("Sorry process %d has been traced!", phptrace_php_pid);
	}

	phptrace_ctrl_set(ctrl, (uint8_t)1, phptrace_php_pid);
#ifdef DEBUG
	phptrace_ctrl_get(ctrl, &num, phptrace_php_pid);
	printf ("[debug][ok] argument -p pid=%d  ctrl after set num=(%d)\n", phptrace_php_pid, num);
#endif
}

static void process_opt_c(char *optarg)
{
	int pid;
	if (strncmp(optarg, "all", 3) == 0)
		phptrace_ctrl_clean_all(ctrl);
	else
	{
		pid = string2uint(optarg);
		if (pid <= 0) 
			error_msg_and_die("Invalid process id: '%s'", optarg);
		phptrace_ctrl_clean_one(ctrl, pid);
	}
}
		
static int update_mmap_filename(phptrace_file_t *f)
{
	char buf[MAX_TEMP_LENGTH];

	if (phptrace_mmap_cnt == 0)
	{
		sprintf (buf, "%s%d", MMAP_LOG_FILENAME, phptrace_php_pid);
		map_filename_next = phptrace_string(buf);
		map_filename_prev = NULL;
		map_filename_free_flag = 1;
	}
	else
	{
		phptrace_mmap_cnt++;
		if (phptrace_str_equal(f->tailer.filename, map_filename_next))
		{
			phptrace_mmap_again = 0;
			return PHPTRACE_IGNORE;
		}
		else
		{
			if (map_filename_free_flag && map_filename_prev)  /* ugly. free first malloc string */
			{
				map_filename_free_flag = 0;
				free(map_filename_prev);
			}
			map_filename_prev = map_filename_next;
			//phptrace_str_free(map_filename_prev);
			map_filename_next = f->tailer.filename;
		}
	}

#ifdef DEBUG
	//@TEST
	//map_filename_next = phptrace_string("data.in3");

	printf ("[debug]map_filename_prev=(%s) map_filename_next=(%s)\n", map_filename_prev == NULL ? "NULL" : map_filename_prev->data, map_filename_next->data);
#endif
	return PHPTRACE_AGAIN;
}

int open_mmap(phptrace_file_t *f)
{
	int rc;

	rc = update_mmap_filename(f);
	
	if (rc == PHPTRACE_IGNORE)
	{
		return PHPTRACE_OK;
	}

	//@DEBUG
	//map_filename_next = phptrace_string("data.in");
	//map_filename_next = phptrace_string("data_r");
	//map_filename_next = phptrace_string("data.in2");

	seg = phptrace_mmap_read(map_filename_next->data);
	if (seg.shmaddr == MAP_FAILED)  //(void *)-1
	{
#ifdef DEBUG
		printf("[debug]phptrace_mmap '%s' failed!\n", map_filename_next->data);
#endif
		return PHPTRACE_AGAIN;
	} 
#ifdef DEBUG
	else 
	{
		printf("[debug]phptrace_mmap '%s' successfully!\n", map_filename_next->data);
		//phptrace_unmap(&seg);
	}
#endif
	return PHPTRACE_OK;
}


void interrupt(int sig)
{
	if (sig == SIGINT)
	{
		die(PHPTRACE_OK);
	}
}

void usage()
{
	printf ("usage: phptrace [-p pid] [-c pid|all] [-h]\n\
	-p pid -- trace php process with process ID pid.\n\
	-c pid|all -- clean the debug switches of pid, or all the switches.\n\
	-h -- show this help.\n");
	exit(PHPTRACE_OK);
}

static void init(int argc, char *argv[])
{
	int c;

	progname = argv[0] ? argv[0] : "phptrace";
	phptrace_tracer_pid = getpid();
	shared_log = stderr;

	if (argc < 2)
	{
		usage();
		exit(1);
	}
	/* init ctrl switches */
	ctrl = (phptrace_ctrl_t *)malloc(sizeof(phptrace_ctrl_t));
	if (!phptrace_ctrl_init(ctrl))
		error_msg_and_die("[error] can't open ctrl mmap file!\n");

	while ((c = getopt(argc, argv,
					"p:"	 // trace pid
					"c:"     // clean switches of pid | all
					"h"		 // help
					)) != EOF)
	{
		switch (c)
		{
			case 'p':
				process_opt_p(optarg);
				break;
			case 'c':
				process_opt_c(optarg);
				break;
			default: 
				usage();
				break;
		}
	} 

	//stack = (phptrace_stack_t *)malloc(sizeof(phptrace_stack_t));
	//phptrace_stack_init(stack, file);  // stack init must be afer file init

	if (signal(SIGINT, interrupt) == SIG_ERR)
		error_msg_and_die("[error] can't catch SIGINT!\n");
}

void print_indent_char(char ch, int size)
{
	int i;
	for (i = 0; i < size; i++)
		putchar(ch);
}
void print_indent_str(char* str, int32_t size)
{
	int32_t i;
	for (i = 0; i < size; i++)
		printf ("%s", str);
}

void update_deep_info(phptrace_file_record_t *r)
{
	//phptrace_mem_read_64b(&(r->time_cost), r->time_cost_ptr);
	phptrace_mem_update_record(r, r->ret_values_ptr);
}

void print_time(uint64_t t, int kind)
{
	switch (kind)
	{
		default:
			printf ("%0.6lf ", t / 1e6);
			break;
	}
}
void func_enter_print(phptrace_file_record_t *r)
{
#ifdef DEBUG
	printf ("[debug][enter level=%d]\n", r->level);
#endif 
	print_time(r->start_time, 0);
	print_indent_str("    ", r->level);
 	phptrace_str_print(r->func_name); 
	putchar ('(');
	phptrace_str_print(r->params); 
	printf (")\n");
	fflush(NULL);
}

void func_leave_print(phptrace_file_record_t *r)
{
	print_time(r->start_time+r->time_cost, 0);
	print_indent_str("    ", r->level);
	phptrace_str_print(r->func_name);
	printf (" =>");
	printf ("	");
	phptrace_str_print(r->ret_values); 
	printf ("	");
	print_time(r->time_cost, 0);
	putchar('\n');
	fflush(NULL);
}

void stack_dfs(phptrace_file_record_t *r)
{
	if (!r) return;
	if (r->prev)
		stack_dfs(r->prev);
	func_enter_print(r);
}
void phptrace_file_stack_print(phptrace_file_t *f)
{
#ifdef DEBUG
	printf ("[debug]------dump old stack-----\n");
#endif
	if (f)
	{
		stack_dfs(f->records_top);
	}
}
void trace(phptrace_file_t *f)
{
	char ch;
	int rc;
	//int32_t idx_new, idx_top;
	phptrace_file_record_t *ptr_rcd_top, *ptr_rcd_new;
	int state = STATE_START;
	uint64_t flag;
	uint64_t record_time;
	void *mem_ptr = NULL;
	void *mem_ptr_prev = NULL;	
	phptrace_file_record_t* p;
	int16_t level_new, level_top, level_prev = -1;
	int ctrl_wait_interval = CTRL_WAIT_INTERVAL;
	int data_wait_interval = DATA_WAIT_INTERVAL;

	while (1)
	{
		if (phptrace_php_pid <= 0)
		{
			state = STATE_ERROR;
			break;
		}

		/* open mmap first time(START) or another time*/
		while (state == STATE_START || state == STATE_TAILER)	
		{
			phptrace_mmap_again = 1;
			rc = open_mmap(f);
			if (rc == PHPTRACE_OK) 
			{
				state = STATE_OPEN;
				break;
			}
#ifdef DEBUG
			printf ("[debug]  open ctrl sleep %d ms.\n", ctrl_wait_interval);
#endif
			phptrace_msleep(ctrl_wait_interval);
			ctrl_wait_interval = grow_interval(ctrl_wait_interval, MAX_CTRL_WAIT_INTERVAL);
			//phptrace_msleep(CTRL_WAIT_INTERVAL);
		} 
		ctrl_wait_interval = CTRL_WAIT_INTERVAL;
		
		/* ? here or before sleep */
		phptrace_ctrl_heart_beat_ping(ctrl, phptrace_php_pid);

		/* read header */
		phptrace_mem_read_64b(&flag, seg.shmaddr);
#ifdef DEBUG
		printf ("[debug]flag=(%lld)", flag);
		if (flag == WAIT_FLAG)	
		{
			printf (" will wait %d ms.\n", data_wait_interval);
		}
		putchar ('\n');
#endif
		if (flag != WAIT_FLAG)
		{
			data_wait_interval = DATA_WAIT_INTERVAL;
		}

		mem_ptr = seg.shmaddr;	// mem_ptr will not change shmaddr;
		switch (flag)
		{
			case MAGIC_NUMBER_HEADER:
				if (state != STATE_OPEN)
					error_msg_and_die("[error] file damaged!\n");
					
				mem_ptr = phptrace_mem_read_header(&(f->header), mem_ptr);			
				state = STATE_HEADER;
#ifdef DEBUG
				printf ("[debug]");
				pr_header(&(f->header));
#endif
				break;
			case MAGIC_NUMBER_TAILER:
				if (state != STATE_HEADER && state != STATE_RECORD)
					error_msg_and_die("[error] file damaged!\n");

				state = STATE_TAILER;
				mem_ptr = phptrace_mem_read_tailer(&(f->tailer), mem_ptr);			
#ifdef DEBUG
				printf ("[debug]");
				pr_tailer(&(f->tailer));
#endif
				break;
			case WAIT_FLAG:
				//if (state != STATE_HEADER && state != STATE_RECORD)  BugFix: maybe wait in STATE_OPEN too.
				//	error_msg_and_die("[error] file damaged!\n");

				phptrace_msleep(data_wait_interval);
				data_wait_interval = grow_interval(data_wait_interval, MAX_DATA_WAIT_INTERVAL);
				continue;
			default:
				if (state == STATE_HEADER)
					state = STATE_RECORD;
				if (state != STATE_RECORD)
					error_msg_and_die("--Sorry, tool internal error(1)!\n"); /* file not ok */

				if (flag != seq + 1)
				{
					error_msg_and_die("--Sorry, tool internal error(2)!\n"); /* seq not ok */
				}

				phptrace_mem_read_record_level(&level_new, mem_ptr);
			//	if (level_new > MIN_RECORDS_NUMBER)
			//	{
			//		error_msg_and_die("[error] too many records!\n");
			//	}
			//	if (level_new > MAX_FUNCTION_LEVEL)
			//	{
			//		error_msg_and_die("[error] function call too much levels!\n");
			//	}

				//assert(level_new >= 0);	
				if (level_new <= level_prev)
				{
					//idx_top = phptrace_stack_top(stack);
					ptr_rcd_top = phptrace_file_record_top(f);
					while (ptr_rcd_top && ptr_rcd_top->level >= level_new)
					{
						//idx_top = phptrace_stack_pop(stack);
						ptr_rcd_top = phptrace_file_record_pop(f);
						update_deep_info(ptr_rcd_top);
						record_time = ptr_rcd_top->start_time;	
						if (record_time > phptrace_start_time)
							func_leave_print(ptr_rcd_top);

						phptrace_file_record_free(ptr_rcd_top);
						ptr_rcd_top = phptrace_file_record_top(f);
					}
				}
				ptr_rcd_new = phptrace_file_record_new();
				mem_ptr = phptrace_mem_read_record(ptr_rcd_new, mem_ptr);

				record_time = ptr_rcd_new->start_time;	
				if (record_time > phptrace_start_time)
				{
					if (phptrace_record_cnt == 0 && level_new > BASE_FUNCTION_LEVEL)
					{
						phptrace_file_stack_print(f);
					}
					func_enter_print(ptr_rcd_new);
					phptrace_record_cnt++;
				}
#ifdef DEBUG
				else 
				{
					printf ("[debug] old record [seq=%d] ~~ [record_time:", flag);
					print_time(record_time, 0);
					printf ("] < [tool_start_time:");
					print_time(phptrace_start_time, 0);
					putchar ('\n');
				}
#endif

				phptrace_file_record_push(f, ptr_rcd_new);

				level_prev = level_new;
				//@TODO rotate

				seq++;
				break;
		}
		mem_ptr_prev = seg.shmaddr;	
		seg.shmaddr = mem_ptr;

		//@TEST
		//scanf ("%c", &ch);
	}
}

int main(int argc, char *argv[])
{
	phptrace_start_time = phptrace_time_usec();
	file = (phptrace_file_t *)malloc(sizeof(phptrace_file_t));
	phptrace_file_init(file);
	init(argc, argv);
	trace(file);

	phptrace_file_free(file);

	return 0;
}
