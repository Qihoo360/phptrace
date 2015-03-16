/*
 * @FILE:    definations of common structs
 * @DESC:   
 *     file format
 *     +------------------------------------------+
 *     |    header                                |
 *     |------------------------------------------|
 *     |    Record 1                              |
 *     |------------------------------------------|
 *     |    Record 2                              |
 *     |------------------------------------------|
 *     |    ...                                   |
 *     |------------------------------------------|
 *     |    Record n                              |
 *     |------------------------------------------|
 *     |    tailer                                |
 *     +------------------------------------------+
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

#include "sds/sds.h"

#define PHPTRACE_LOG_DIR "/tmp"
#define PHPTRACE_TRACE_FILENAME "phptrace.trace"
#define PHPTRACE_CTRL_FILENAME "phptrace.ctrl"
#define PHPTRACE_STATUS_FILENAME "phptrace.status"

/*We use PID_MAX+1 as the size of file phptrace.ctrl
 *4 million is the hard limit of linux kernel so far,
 *It is 99999 on Mac OS X which is coverd by this value.
 *So 4*1024*1024 can serve both linux and unix(include darwin)
 * */
#define PID_MAX 4194304 /* 4*1024*1024 */

#define MAGIC_NUMBER_HEADER 0x6563617274706870
#define DATA_FORMAT_VERSION 1    

#define MAGIC_NUMBER_TAILER 0x657461746f720000
#define WAIT_FLAG ((uint64_t)-1LL)

#define RECORD_FLAG_ENTRY    (1<<0)
#define RECORD_FLAG_EXIT    (1<<1) 

typedef struct phptrace_file_header_s {
    uint64_t magic_number;
    uint8_t version;
    uint8_t flag;
} phptrace_file_header_t;

typedef struct phptrace_file_tailer_s {
    uint64_t magic_number;
    sds filename;
} phptrace_file_tailer_t;

/* record struct */
#define RECORD_ENTRY(r, m) ((r)->info.entry.m)
#define RECORD_EXIT(r, m) ((r)->info.exit.m)
typedef struct phptrace_file_record_s {
    uint64_t seq;                       /* sequence number of record */
    uint8_t  flag;                      /* flag of record type: entry or exit */
    uint16_t level;                     /* absolute function level */ 
    sds function_name;                  /* function name   ptr or ref */
    uint64_t start_time;                /* function start time     */    

    union {
        struct {
            sds params;                 /* function params    */
            sds filename;               /* file name    */
            uint32_t lineno;            /* line number */    
        } entry;
        struct {
            sds return_value;           /* function return values */
            uint64_t cost_time;         /* inclusive cost time of function */
            uint64_t cpu_time;          /* inclusive cpu time of function */
            int64_t  memory_usage;
            int64_t  memory_peak_usage;
        } exit;
    } info;
} phptrace_file_record_t;

/* File struct */
typedef struct phptrace_file_s {
    phptrace_file_header_t header;
    phptrace_file_tailer_t tailer;
    uint32_t num_records;
} phptrace_file_t;

void *phptrace_mem_read_header(phptrace_file_header_t *header, void *mem);
void *phptrace_mem_write_header(phptrace_file_header_t *header, void *mem);

void *phptrace_mem_read_record(phptrace_file_record_t *record, void *mem, uint64_t seq);
void *phptrace_mem_write_record(phptrace_file_record_t *record, void *mem);

size_t phptrace_record_rawsize(phptrace_file_record_t *record);

void *phptrace_mem_write_waitflag(void *mem);
void *phptrace_mem_read_waitflag(uint64_t *waitflag, void *mem);

void *phptrace_mem_read_tailer(phptrace_file_tailer_t *record, void *mem);
void *phptrace_mem_write_tailer(phptrace_file_tailer_t *record, void *mem);

#endif
