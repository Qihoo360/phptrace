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

#ifndef _PHPTRACE_MMAP_H_
#define _PHPTRACE_MMAP_H_

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

typedef struct phptrace_segment_s {
    size_t size;
    void* shmaddr;
} phptrace_segment_t;

phptrace_segment_t phptrace_mmap_create(const char *file, size_t size);
phptrace_segment_t phptrace_mmap_write(const char *file);
phptrace_segment_t phptrace_mmap_read(const char *file);
int phptrace_unmap(phptrace_segment_t* segment);

#endif // _PHPTRACE_MMAP_H_
