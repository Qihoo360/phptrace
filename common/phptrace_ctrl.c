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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "phptrace_ctrl.h"

int pt_ctrl_open(pt_ctrl_t *ctrl, const char *file)
{
    int fd;
    struct stat st;

    pt_ctrl_reset(ctrl);

    /* file open */
    fd = open(file, O_RDWR, DEFFILEMODE);
    if (fd == -1) {
        return -1;
    }

    /* size */
    if (fstat(fd, &st) == -1) {
        return -1;
    }
    ctrl->size = st.st_size;
    if (ctrl->size < PT_CTRL_SIZE) {
        return -1; /* return here if size is too small */
    }

    /* mmap */
    ctrl->addr = mmap(NULL, ctrl->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NOSYNC, fd, 0);
    if (ctrl->addr == MAP_FAILED) {
        pt_ctrl_reset(ctrl);
        return -1;
    }

    return 0;
}

int pt_ctrl_create(pt_ctrl_t *ctrl, const char *file)
{
    int fd;

    pt_ctrl_reset(ctrl);

    /* file open */
    fd = open(file, O_RDWR | O_CREAT, DEFFILEMODE);
    if (fd == -1) {
        return -1;
    }

    /* size */
    if (ftruncate(fd, PT_CTRL_SIZE) == -1) {
        close(fd);
        unlink(file);
        return -1;
    }
    ctrl->size = PT_CTRL_SIZE;

    /* mmap */
    ctrl->addr = mmap(NULL, ctrl->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NOSYNC, fd, 0);
    if (ctrl->addr == MAP_FAILED) {
        pt_ctrl_reset(ctrl);
        return -1;
    }

    /* zero fill */
    pt_ctrl_clean_all(ctrl);

    return 0;
}

int pt_ctrl_close(pt_ctrl_t *ctrl)
{
    if (ctrl->addr) {
        munmap(ctrl->addr, ctrl->size);
    }
    pt_ctrl_reset(ctrl);

    return 0;
}

void pt_ctrl_reset(pt_ctrl_t *ctrl)
{
    ctrl->addr = NULL;
    ctrl->size = 0;
}

void pt_ctrl_clean_all(pt_ctrl_t *ctrl)
{
    memset(ctrl->addr, 0x00, ctrl->size);
}
