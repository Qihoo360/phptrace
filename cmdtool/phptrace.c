/*
 *  trace cmdtools
 */

#include "phptrace_util.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <string.h>
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# include <stdint.h>
#endif

volatile int interrupted;        /* flag of interrupt (CTRL + C) */

static void interrupt(int sig)
{
    log_printf(LL_DEBUG, "catch SIGINT signal");
    interrupted = sig;
}

int main(int argc, char *argv[])
{
    phptrace_context_t context;

    init(&context, argc, argv);

    if (signal(SIGINT, interrupt) == SIG_ERR) {
        error_msg(&context, ERR, "install signal handler failed (%s)", (errno ? strerror(errno) : "null"));
        exit(-1);
    }

    process(&context);

    return 0;
}

