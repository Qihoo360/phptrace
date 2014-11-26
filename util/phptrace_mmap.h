#ifndef _PHPTRACE_MMAP_H_
#define _PHPTRACE_MMAP_H_

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

typedef struct _phptrace_segment_t phptrace_segment_t;

struct _phptrace_segment_t {
    size_t size;
    void* shmaddr;
};

phptrace_segment_t phptrace_mmap_write(char *file_mask, size_t size);
phptrace_segment_t phptrace_mmap_read(char *file_mask);
int phptrace_unmap(phptrace_segment_t* segment);

#endif // _PHPTRACE_MMAP_H_
