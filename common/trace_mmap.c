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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "trace_mmap.h"

static inline int reset_with_retval(pt_segment_t *seg, int retval)
{
    seg->addr = NULL;
    seg->size = 0;

    return retval;
}

int pt_mmap_open_fd(pt_segment_t *seg, int fd, size_t size)
{
    struct stat st;

    /* ensure filesize is larger than required size */
    if (fd != -1) {
        if (fstat(fd, &st) == -1) {
            return reset_with_retval(seg, -1);
        }
        if (size == 0) {
            size = st.st_size;
        } else if (size > 0 && st.st_size < size) {
            return reset_with_retval(seg, -1);
        }
    }
    seg->size = size;

    /* mmap */
    seg->addr = mmap(NULL, seg->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NOSYNC, fd, 0);
    if (seg->addr == MAP_FAILED) {
        return reset_with_retval(seg, -1);
    }

    return 0;
}

int pt_mmap_open(pt_segment_t *seg, const char *file, size_t size)
{
    int fd;

    /* file open */
    fd = open(file, O_RDWR, DEFFILEMODE);
    if (fd == -1) {
        return reset_with_retval(seg, -1);
    }

    /* mmap open */
    if (pt_mmap_open_fd(seg, fd, size) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int pt_mmap_create(pt_segment_t *seg, const char *file, size_t size)
{
    int fd;

    /* file open */
    umask(0000);
    fd = open(file, O_RDWR | O_CREAT, DEFFILEMODE);
    if (fd == -1) {
        return reset_with_retval(seg, -1);
    }

    /* set size */
    if (ftruncate(fd, size) == -1) {
        close(fd);
        unlink(file);
        return reset_with_retval(seg, -1);
    }

    /* mmap open */
    if (pt_mmap_open_fd(seg, fd, size) < 0) {
        close(fd);
        unlink(file);
        return -1;
    }

    close(fd);
    return 0;
}

int pt_mmap_close(pt_segment_t *seg)
{
    if (munmap(seg->addr, seg->size) == -1) {
        return -1;
    }

    return reset_with_retval(seg, 0);
}
