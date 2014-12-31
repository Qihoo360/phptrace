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

int main(int argc, char *argv[])
{
    phptrace_context_t context;

    init(&context, argc, argv);
    process(&context);

    return 0;
}

