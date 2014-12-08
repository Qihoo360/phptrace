#include "phptrace_mmap.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * Some operating systems (like FreeBSD) have a MAP_NOSYNC flag that
 * tells whatever update daemons might be running to not flush dirty
 * vm pages to disk unless absolutely necessary.  My guess is that
 * most systems that don't have this probably default to only synching
 * to disk when absolutely necessary.
 */
#ifndef MAP_NOSYNC
#define MAP_NOSYNC 0
#endif

/* support for systems where MAP_ANONYMOUS is defined but not MAP_ANON, ie: HP-UX bug #14615 */
#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
# define MAP_ANON MAP_ANONYMOUS
#endif

phptrace_segment_t phptrace_mmap_write(char *file_mask, size_t size)
{
    phptrace_segment_t segment; 

    int fd = -1;
    int flags = MAP_SHARED | MAP_NOSYNC;

    /*
     * we do a normal filesystem mmap
     */
    fd = open(file_mask, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd == -1) {
        goto error;
    }
    if (ftruncate(fd, size) < 0) {
        close(fd);
        unlink(file_mask);
        goto error;
    }

    segment.shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, flags, fd, 0);
    segment.size = size;

    if ((long)segment.shmaddr == -1) {
        segment.size = 0;
    }

    if (fd != -1) close(fd);
    
    return segment;

error:

    segment.shmaddr = (void*)-1;
    segment.size = 0;

    return segment;
}

phptrace_segment_t phptrace_mmap_read(char *file_mask)
{
    phptrace_segment_t segment; 
    struct stat st;

    int fd = -1;
    int flags = MAP_SHARED | MAP_NOSYNC;

    /* If no filename was provided, do an anonymous mmap */
    if (!file_mask || (file_mask && !strlen(file_mask))) {
#if !defined(MAP_ANON)
#else
        fd = -1;
        flags = MAP_SHARED | MAP_ANON;
#endif
    } else if (!strcmp(file_mask,"/dev/zero")) { 
        fd = open("/dev/zero", O_RDWR, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            goto error;
        }
    } else {
        /*
         * Otherwise we do a normal filesystem mmap
         */
        fd = open(file_mask, O_RDWR);
        if (fd == -1) {
            goto error;
        }
        if(fstat(fd, &st)== -1){
            goto error;
        }
    }

    segment.shmaddr = (void *)mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, flags, fd, 0);
    segment.size = st.st_size;

    if ((long)segment.shmaddr == -1) {
        segment.size = 0;
    }

    if (fd != -1) close(fd);
    
    return segment;

error:

    segment.shmaddr = (void*)-1;
    segment.size = 0;

    return segment;
}

int phptrace_unmap(phptrace_segment_t *segment)
{
    if (munmap(segment->shmaddr, segment->size) < 0) {
        return -1;
    }
    return 0;
}

