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

phptrace_segment_t phptrace_mmap_fd(int fd, size_t size, int prot)
{
    int flags ;
    phptrace_segment_t segment;
    struct stat st;
    
    flags = MAP_SHARED | MAP_NOSYNC;

    if (size == 0) {
        fstat(fd, &st);
        size = st.st_size;
    }
    
    segment.shmaddr = (void *)mmap(NULL, size, prot, flags, fd, 0);
    if (segment.shmaddr == MAP_FAILED) {
        segment.size = 0;
    }
    segment.size = size;
    if( fd != -1) {
        close(fd);
    }
    return segment;
}

int phptrace_file_open(const char *file, int flags, int mode)
{
    int fd;
    if (file == NULL) {
        return -1;
    }
    fd = open(file, flags, mode);
    return fd;
}

phptrace_segment_t phptrace_mmap_create(const char *file, size_t size)
{
    int fd;
    phptrace_segment_t segment; 

    /*Give all users rw permissions*/
    umask(0);
    fd = phptrace_file_open(file, O_RDWR|O_CREAT,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd == -1) {
        goto error;
    }
    if (ftruncate(fd, size) < 0) {
        close(fd);
        unlink(file);
        goto error;
    }
    return phptrace_mmap_fd(fd, size, PROT_READ|PROT_WRITE);
error:
    segment.shmaddr = MAP_FAILED;
    segment.size = 0;
    return segment;
}

phptrace_segment_t phptrace_mmap_write(const char *file)
{
    int fd;
    phptrace_segment_t segment; 

    fd = phptrace_file_open(file, O_RDWR, 
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd == -1) {
        goto error;
    }

    return phptrace_mmap_fd(fd, 0, PROT_READ|PROT_WRITE);

error:
    segment.shmaddr = MAP_FAILED;
    segment.size = 0;

    return segment;
}

phptrace_segment_t phptrace_mmap_read(const char *file)
{
    int fd;
    phptrace_segment_t segment; 

    fd = phptrace_file_open(file, O_RDONLY, 
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd == -1) {
        goto error;
    }

    return phptrace_mmap_fd(fd, 0, PROT_READ);

error:
    segment.shmaddr = MAP_FAILED;
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

