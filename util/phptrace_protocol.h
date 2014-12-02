/*
 * @FILE:	definations of common structs
 * @DESC:   
 * 	file format
 *	 +------------------------------------------+
 *	 |    header								|
 *	 |------------------------------------------|
 *	 |	Record 1								|
 *	 |------------------------------------------|
 *	 |	Record 2								|
 *	 |------------------------------------------|
 *	 |	...										|
 *	 |------------------------------------------|
 *	 |	Record n								|
 *	 |------------------------------------------|
 *	 |	tailer									|
 *	 +------------------------------------------+
 *
 * @Author: 
 * @Change:
 */
#ifndef PHPTRACE_PROTOCOL_H
#define PHPTRACE_PROTOCOL_H

#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/syscall.h>

#include "phptrace_string.h"
#include "phptrace_mmap.h"

//#define DEBUG

typedef struct phptrace_file_header_s
{
	uint64_t magic_number;
	uint8_t version;
	uint8_t flag;
}phptrace_file_header_t;

typedef struct phptrace_file_tailer_s
{
	uint64_t magic_number;
    phptrace_str_t *filename;
}phptrace_file_tailer_t;

/* record struct */
//#define MIN_RECORDS_NUMBER 100
//#define MAX_RECORDS_NUMBER 32010
#define RET_VALUE_SIZE 32

#define MMAP_CTRL_FILENAME "/tmp/phptrace.ctrl"
#define	MMAP_LOG_FILENAME "/tmp/phptrace.log."

typedef struct phptrace_file_record_s
{
	uint64_t seq;
	uint16_t level;
	uint64_t start_time;

	phptrace_str_t *func_name;    /* ptr or ref */
	phptrace_str_t *params;
	phptrace_str_t *ret_values;

	uint64_t time_cost;
	
	/* other stuff */
	struct phptrace_file_record_s *prev;
	void *ret_values_ptr;
}phptrace_file_record_t;

/* File struct */
typedef struct phptrace_file_s
{
	phptrace_file_header_t header;
	phptrace_file_tailer_t tailer;
	
	uint32_t num_records;
	//phptrace_file_record_t *records;

	phptrace_file_record_t *records_base;
	phptrace_file_record_t *records_top;

	/* other stuffs */
	uint32_t size;   /* not in used */
}phptrace_file_t;

/* shm node and array */
//	typedef struct phptrace_ctrl_node_s
//	{
//	}phptrace_ctrl_node_t;

typedef struct phptrace_ctrl_s
{
	uint32_t size;
	uint32_t used;
	phptrace_segment_t ctrl_seg;
}phptrace_ctrl_t;

#define MAX_PROCESS_NUMBER 65535

#define MAGIC_NUMBER_HEADER 0x6563617274706870
#define DATA_FORMAT_VERSION 1	

#define MAGIC_NUMBER_TAILER 0x657461746f720000
#define WAIT_FLAG ((uint64_t)-1LL)

enum {
	CTRL_STATE_CLOSE = 0,
	CTRL_STATE_PING, 		/* ping and pong are used to heart beat */
	CTRL_STATE_PONG 
};

#define set_file_header(h, m, v, f)	\
	{	(h).magic_number = (m);	\
		(h).version = (v);		\
		(h).flag = (f);}		
#define init_file_header(h)		set_file_header((h), (MAGIC_NUMBER_HEADER), \
		(DATA_FORMAT_VERSION), (0))

#define set_file_tailer(t, m, fn) \
	{	(t).magic_number = (m);		\
		(t).filename = phptrace_string((fn));}	
//#define init_file_tailer(t)		set_file_tailer((t), (MAGIC_NUMBER_TAILER), 

/* records stack op */
//void phptrace_file_record_clean_str(phptrace_file_record_t *r);
phptrace_file_record_t *phptrace_file_record_new();
void phptrace_file_record_free(phptrace_file_record_t *r);
void phptrace_file_record_push(phptrace_file_t *f, phptrace_file_record_t *r);
phptrace_file_record_t* phptrace_file_record_top(phptrace_file_t *f);
phptrace_file_record_t* phptrace_file_record_pop(phptrace_file_t *f);


int phptrace_ctrl_init(phptrace_ctrl_t *c);
void phptrace_ctrl_clean_all(phptrace_ctrl_t *c);
void phptrace_ctrl_clean_one(phptrace_ctrl_t *c, int pid);
void phptrace_ctrl_free(phptrace_ctrl_t *c, int pid);
#define phptrace_ctrl_set(c, n, pid) 	\
	phptrace_mem_write_8b((n), (c)->ctrl_seg.shmaddr+(pid))
#define phptrace_ctrl_get(c, n, pid)	\
	phptrace_mem_read_8b((n), (c)->ctrl_seg.shmaddr+(pid))
//int8_t phptrace_ctrl_heart_beat(phptrace_ctrl_t *c, int pid);
int8_t phptrace_ctrl_heart_beat_ping(phptrace_ctrl_t *c, int pid);
	
void phptrace_file_init(phptrace_file_t *f);
void phptrace_file_free(phptrace_file_t *f);

/*
#define phptrace_file_clean_records_state(f, id) (f)->records_state[(id)] = 0
#define phptrace_file_set_records_state(f, id) (f)->records_state[(id)] = 1
int32_t phptrace_file_next_record_idx(phptrace_file_t *f); 
void phptrace_file_shrink_records_queue(phptrace_file_t *f);
int phptrace_file_push_record(phptrace_file_t *f); 
int phptrace_file_pop_record(phptrace_file_t *f); 
*/

void *phptrace_mem_read_header(phptrace_file_header_t *header, void *mem);
void *phptrace_mem_write_header(phptrace_file_header_t *header, void *mem);

void *phptrace_mem_read_record(phptrace_file_record_t *record, void *mem);
void *phptrace_mem_read_record_level(int16_t *level, void *mem);
void *phptrace_mem_write_record(phptrace_file_record_t *record, void *mem);
int phptrace_mem_update_record(phptrace_file_record_t *record, void *mem);

void *phptrace_mem_fix_record(phptrace_file_record_t *record, void *mem);

void *phptrace_mem_write_waitflag(void *mem);
int phptrace_mem_is_waitflag(void *mem);
void *phptrace_mem_read_waitflag(uint64_t *waitflag, void *mem);

void *phptrace_mem_read_tailer(phptrace_file_tailer_t *record, void *mem);
void *phptrace_mem_write_tailer(phptrace_file_tailer_t *record, void *mem);

void *phptrace_mem_read_64b(uint64_t *number, void *mem);
void *phptrace_mem_read_8b(uint8_t *number, void *mem);
void *phptrace_mem_write_8b(uint8_t number, void *mem);

/* utils */
int string2uint(const char *str);

#define phptrace_msleep(ms) (void) usleep((ms) * 1000)


#ifdef DEBUG
void pr_header(phptrace_file_header_t *h);
void pr_tailer(phptrace_file_tailer_t *t);
void pr_record(phptrace_file_record_t *r);
//void pr_file(phptrace_file_t *f);
//void dump2file(phptrace_file_t *f, char *filename);

void pr_file(phptrace_file_t *f, phptrace_file_record_t *r);
void dump2file(phptrace_file_t *f, phptrace_file_record_t *r, char *filename);
#endif 

#endif
