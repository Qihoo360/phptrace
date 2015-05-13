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

#ifndef PHPTRACE_CTRL_H
#define PHPTRACE_CTRL_H

#include <stdint.h>

#if 1 /* TODO mmap */
#include <sys/mman.h>

/* Some operating systems (like FreeBSD) have a MAP_NOSYNC flag that
 * tells whatever update daemons might be running to not flush dirty
 * vm pages to disk unless absolutely necessary.  My guess is that
 * most systems that don't have this probably default to only synching
 * to disk when absolutely necessary. */
#ifndef MAP_NOSYNC
#define MAP_NOSYNC 0
#endif

/* support for systems where MAP_ANONYMOUS is defined but not MAP_ANON, ie:
 * HP-UX bug #14615 */
#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
# define MAP_ANON MAP_ANONYMOUS
#endif
#endif

/* We use PT_PID_MAX + 1 as the size of file ctrl file. 4 million is the hard
 * limit of linux kernel so far, and it is 99999 on Mac OS X which is coverd by
 * this value. So 4*1024*1024 can serve both linux and unix(include darwin). */
#define PT_PID_MAX          4194304
#define PT_CTRL_SIZE        PT_PID_MAX + 1
#define PT_CTRL_FILENAME    "phptrace.ctrl"
#define PT_CTRL_ACTIVE      0x01

typedef struct {
    size_t size;
    void *addr;
} pt_ctrl_t;

int pt_ctrl_open(pt_ctrl_t *ctrl, const char *file);
int pt_ctrl_create(pt_ctrl_t *ctrl, const char *file);
int pt_ctrl_close(pt_ctrl_t *ctrl);
void pt_ctrl_reset(pt_ctrl_t *ctrl);
void pt_ctrl_clean_all(pt_ctrl_t *ctrl);

#define pt_ctrl_pid(ctrl, pid)              (*(uint8_t *) (((ctrl)->addr) + (pid)))
#define pt_ctrl_pid_is_active(ctrl, pid)    (pt_ctrl_pid(ctrl, pid) &   PT_CTRL_ACTIVE)
#define pt_ctrl_pid_set_active(ctrl, pid)   (pt_ctrl_pid(ctrl, pid) |=  PT_CTRL_ACTIVE)
#define pt_ctrl_pid_set_inactive(ctrl, pid) (pt_ctrl_pid(ctrl, pid) &= ~PT_CTRL_ACTIVE)

#endif
