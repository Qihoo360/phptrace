#include <sys/time.h>
#include "phptrace_time.h"
uint64_t phptrace_time_usec() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}
