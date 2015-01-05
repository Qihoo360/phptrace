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
