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

#include "phptrace_ctrl.h"

/**
 * Public Interface
 * --------------------
 */
int phptrace_ctrl_needcreate(const char *filename, int pid_max)
{
    struct stat st;

    if (stat(filename, &st) == -1) {
        if (errno == ENOENT) {
            return 1;
        } else {
            return -1;
        }
    } else if (st.st_size < pid_max + 1) {
        return 1;
    }

    return 0;
}

int phptrace_ctrl_create(phptrace_ctrl_t *c, const char *filename, int pid_max)
{
    c->ctrl_seg = phptrace_mmap_create(filename, pid_max + 1);
    if (c->ctrl_seg.shmaddr == MAP_FAILED) {
        return -1;
    }

    memset(c->ctrl_seg.shmaddr, 0, c->ctrl_seg.size);
    return 0;
}

int phptrace_ctrl_open(phptrace_ctrl_t *c, const char *filename)
{
    c->ctrl_seg = phptrace_mmap_write(filename);
    return (c->ctrl_seg.shmaddr == MAP_FAILED) ? -1 : 0;
}

void phptrace_ctrl_close(phptrace_ctrl_t *c)
{
    phptrace_unmap(&c->ctrl_seg);
}

/**
 * Old
 * --------------------
 */
int phptrace_ctrl_init(phptrace_ctrl_t *c)
{
    c->ctrl_seg = phptrace_mmap_write(PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME);
    return (c->ctrl_seg.shmaddr != MAP_FAILED); 
}

void phptrace_ctrl_clean_all(phptrace_ctrl_t *c)
{
    if (c && c->ctrl_seg.shmaddr && c->ctrl_seg.shmaddr != MAP_FAILED) {
        memset(c->ctrl_seg.shmaddr, 0, c->ctrl_seg.size);
    }
}

void phptrace_ctrl_clean_one(phptrace_ctrl_t *c, int pid)
{
    if (c && c->ctrl_seg.shmaddr && c->ctrl_seg.shmaddr != MAP_FAILED) {
        phptrace_ctrl_set(c, 0, pid); 
    }
}

void phptrace_ctrl_destroy(phptrace_ctrl_t *c)
{
    if (c && c->ctrl_seg.shmaddr && c->ctrl_seg.shmaddr != MAP_FAILED) {
        phptrace_unmap(&(c->ctrl_seg));
    }
}

int8_t phptrace_ctrl_heartbeat_ping(phptrace_ctrl_t *c, int pid)
{ 
    int8_t s = -1;
    phptrace_ctrl_get(c, &s, pid);
    s = s | (1 << 7);
    phptrace_ctrl_set(c, (int8_t)s, pid);
    return s;
}

/* TODO c */
int8_t phptrace_ctrl_heartbeat_pong(phptrace_ctrl_t *c, int pid)
{
    int8_t s = -1;
    phptrace_ctrl_get(c, &s, pid);
    s &= ~(1 << 7) & 0x00FF;
    phptrace_ctrl_set(c, (int8_t)s, pid);
    return s;
}
