/*
 * @FILE: ctrl structs
 * @DESC:   
 */

#ifndef PHPTRACE_CTRL_H
#define PHPTRACE_CTRL_H

#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "phptrace_protocol.h"
#include "phptrace_mmap.h"

typedef struct phptrace_ctrl_s {
    phptrace_segment_t ctrl_seg;
} phptrace_ctrl_t;

#define PT_CTRL_ACTIVE 0x01

int phptrace_ctrl_needcreate(const char *filename, int pid_max);
int phptrace_ctrl_create(phptrace_ctrl_t *c, const char *filename, int pid_max);
int phptrace_ctrl_open(phptrace_ctrl_t *c, const char *filename);
void phptrace_ctrl_close(phptrace_ctrl_t *c);

int phptrace_ctrl_init(phptrace_ctrl_t *c);
void phptrace_ctrl_clean_all(phptrace_ctrl_t *c);
void phptrace_ctrl_clean_one(phptrace_ctrl_t *c, int pid);
void phptrace_ctrl_destroy(phptrace_ctrl_t *c);
#define phptrace_ctrl_set(c, n, pid)     \
    do { *((int8_t *)((c)->ctrl_seg.shmaddr)+(pid)) = (n); } while(0);
#define phptrace_ctrl_get(c, n, pid)    \
    do { *(n) = *( (int8_t *)( (c)->ctrl_seg.shmaddr+(pid) ) ); } while  (0);

int8_t phptrace_ctrl_heartbeat_ping(phptrace_ctrl_t *c, int pid);
int8_t phptrace_ctrl_heartbeat_pong(phptrace_ctrl_t *c, int pid);

#endif
