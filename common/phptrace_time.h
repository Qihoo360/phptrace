#ifndef PHPTRACE_TIME_H
#define PHPTRACE_TIME_H

#include <stdint.h>
#include <sys/resource.h>
#include <sys/time.h>

static inline uint32_t phptrace_time_sec()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint32_t)tv.tv_sec;
}

static inline uint64_t phptrace_time_usec()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
}

static inline uint64_t phptrace_cputime_usec()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return (uint64_t)usage.ru_utime.tv_sec * 1000000 +
           (uint64_t)usage.ru_utime.tv_usec +
           (uint64_t)usage.ru_stime.tv_sec * 1000000 +
           (uint64_t)usage.ru_stime.tv_usec;
}

#endif
