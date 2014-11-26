/*
 * generate binary data
 */

#include "../util/phptrace_protocol.h"
#include "../util/phptrace_string.h"

#include <stdarg.h>

static const char *progname;

phptrace_file_t *file;

/* not used */
static unsigned int nprocs = 0;
static FILE *shared_log;

#define MMAP_LENGTH  (1024 * 1024)

int seq = 0;
void gen_record(phptrace_file_record_t *p)
{
	p->seq = seq;
	p->level = seq++;
	// wait data
	//if (seq == 3)
	//	p->seq = (-1LL);
	
	if (seq == 4)
		p->level = 1;
	if (seq == 5)
		p->level = 0;

	p->start_time = 0x1234123412341234;

	p->func_name = phptrace_string("func1");
	p->params = phptrace_string("para1, para2");
	p->ret_values = phptrace_string("ret_0");

	p->time_cost = 0x5678567856785678;
}

int num_records = 5;
void init_file(phptrace_file_t *f, phptrace_file_record_t *p)
{
	int i, j;
	init_file_header(f->header);

	f->num_records = num_records;

	for (i = 0; i < num_records; i++)
	{
		gen_record(p + i);
	}

	//init_file_tailer(f->tailer);
	set_file_tailer(f->tailer, MAGIC_NUMBER_TAILER, "data_r");
}


int main(int argc, char *argv[])
{
	char *str = (char *)malloc(MAX_PROCESS_NUMBER);
	FILE *fp;
	if (argc == 2 && strncmp(argv[1], "ctrl", 4) == 0)
	{
		printf ("gen ctrl data");
		fp = fopen("phptrace.ctrl", "wb");
		memset(str, 0, sizeof(str));
		if (!fp)
		{
			printf ("failed to open ctrl file");
			return 0;
		}
		
		fwrite(str, sizeof(uint8_t), MAX_PROCESS_NUMBER, (fp));	
		fclose(fp);
		return 0;
	}
	
				
	/* simulate write a file */	
	file = (phptrace_file_t *)malloc(sizeof(phptrace_file_t));
	phptrace_file_record_t *p = (phptrace_file_record_t *)malloc(sizeof(phptrace_file_record_t) * num_records);
	if (!p)
	{
		printf ("[error] malloc failed!\n");
		return;
	}
	init_file(file, p);
	pr_file(file, p);
	
	char *filename = argc < 2 ? "data.in" : argv[1];
	dump2file(file, p, filename);
	return 0;

	/*
	phptrace_str_t *s;
	s = phptrace_string("asdfasdfasdfasdf");
	printf ("len=%d (%s)\n", s->len, s->data);

	s = phptrace_string("abcde");
	s = phptrace_string("asdfasdfasdfasdf");
	printf ("len=%d (%s)\n", s->len, s->data);
	*/

	//init_file_header(file.header);
	//pr_header(&(file.header));
	
	return 0;
}
