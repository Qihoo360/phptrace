#include <sys/time.h>
#include <sys/resource.h>
#include "phptrace_time.h"
uint64_t phptrace_time_usec() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
}
uint64_t phptrace_cputime_usec() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return (uint64_t)usage.ru_utime.tv_sec * 1000000 +
           (uint64_t)usage.ru_utime.tv_usec +
           (uint64_t)usage.ru_stime.tv_sec * 1000000 +
           (uint64_t)usage.ru_stime.tv_usec;

}
