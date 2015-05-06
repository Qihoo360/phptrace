#ifndef PHPTRACE_STATUS_H
#define PHPTRACE_STATUS_H

#include "phptrace.h"
#include "phptrace_util.h"


#define MAX_STACK_DEEP 16
#define MAX_RETRY 3

#define valid_ptr(p) ((p) && 0 == ((p) & (sizeof(long) - 1)))

/* stack related */
int stack_dump_once(phptrace_context_t* ctx);
int stack_dump(phptrace_context_t* ctx);
void process_opt_s(phptrace_context_t *ctx);

#endif
