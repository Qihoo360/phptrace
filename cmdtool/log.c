/**
 * Copyright 2015 Qihoo 360
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "log.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

static int s_log_level;

void log_level_set(int level)
{
    if (level < LL_DEBUG) {
        level = LL_DEBUG;
    } else if (level > LL_ERROR) {
        level = LL_ERROR + 1;
    }

    s_log_level = level;
}

int log_level_get()
{
    return s_log_level;
}

void log_msg(int level, const char *msg)
{
    const char *c = ".-*#";
    FILE *fp; 
    char buf[64];
    int off; 
    struct timeval tv;

    if (level < s_log_level) {
        return;
    }

    fp = stderr;

    if (fp) {
        gettimeofday(&tv, NULL);
        off = strftime(buf, sizeof(buf), "%d %b %H:%M:%S.", localtime(&tv.tv_sec));
        snprintf(buf+off, sizeof(buf)-off, "%03d", (int)tv.tv_usec/1000);
        fprintf(fp, "[%d] %s %c %s\n", (int)getpid(), buf, c[level], msg);
        fflush(fp);
    }
}

void log_printf(int level, const char *fmt, ...)
{
    va_list ap;
    char msg[MAX_LOGMSG_LEN];

    if (level < s_log_level) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    log_msg(level, msg);
}

