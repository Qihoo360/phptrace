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
#include <errno.h>

#include "phptrace_comm.h"
#include "phptrace_mmap.h"

/* We use PID_MAX+1 as the size of file phptrace.ctrl 4 million is the hard
 * limit of linux kernel so far, It is 99999 on Mac OS X which is coverd by
 * this value.  So 4*1024*1024 can serve both linux and unix(include darwin) */
#define PID_MAX 4194304 /* 4*1024*1024 */

#define PHPTRACE_CTRL_FILENAME          "phptrace.ctrl"

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
