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

#ifndef TRACE_UTIL_H
#define TRACE_UTIL_H

#include "trace.h"

#include "trace_count.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PHPTRACE_NAME               "phptrace"
#define PHPTRACE_VERSION            "0.3.1-dev"
#define PHPTRACE_DEVELOPER          "360 Infra Webcore Team"

#define PHPTRACE_LOG_DIR            "/tmp"

#define MAX_TEMP_LENGTH             100
#define MAX_PRINT_LENGTH            600

#define RECORD_STRING_LENGTH        (4 * 1024)

#define DEFAULT_TOP_N               20
#define DEFAULT_MAX_LEVEL           2048

#define OPT_FLAG_STATUS             0x0001                              /* flag of status */
#define OPT_FLAG_CLEANUP            0x0002                              /* flag of cleanup */
#define OPT_FLAG_COUNT              0x0004                              /* flag of count */

#define OUTPUT_FLAG_WRITE           0x0001                              /* flag of write */
#define OUTPUT_FLAG_JSON            0x0002                              /* flag of json */

#define OPEN_DATA_WAIT_INTERVAL     100                                 /* ms */
#define MAX_OPEN_DATA_WAIT_INTERVAL (OPEN_DATA_WAIT_INTERVAL * 16)      /* ms */
#define DATA_WAIT_INTERVAL          10                                  /* ms */
#define MAX_DATA_WAIT_INTERVAL      (DATA_WAIT_INTERVAL * 20)           /* ms */

#define grow_interval(t, mx)        (((t)<<1) > (mx) ? (mx) : ((t) << 1))  

#define pt_msleep(ms)       (void) usleep((ms) * 1000)

#define MIN(x, y)           ((x) < (y) ? (x) : (y))
#define MAX(x, y)           ((x) > (y) ? (x) : (y))

#define PHP52 2
#define PHP53 3
#define PHP54 4
#define PHP55 5
#define PHP56 6

enum {
    ERR = 1,
    ERR_INVALID_PARAM,
    ERR_STACK,
    ERR_CTRL,
    ERR_TRACE,
    ERR_COUNT,
};

enum {
    STATE_OPEN = 0,
    STATE_HEADER,
    STATE_RECORD,
    STATE_TAILER,
    STATE_END
};

volatile int interrupted;        /* flag of interrupt (CTRL + C) */

long hexstring2long(const char*s, size_t len);
unsigned int string2uint(const char *str);

void verror_msg(pt_context_t *ctx, int err_code, const char *fmt, va_list p);
void error_msg(pt_context_t *ctx, int err_code, const char *fmt, ...);
void die(pt_context_t *ctx, int exit_code);

void usage();
void pt_context_init(pt_context_t *ctx);

void process_opt_e(pt_context_t *ctx);

/* print utils */
sds sdscatrepr_noquto(sds s, const char *p, size_t len);
sds print_indent_str(sds s, char* str, int32_t size);
sds pt_repr_function(sds buf, pt_frame_t *f);

sds standard_transform(pt_context_t *ctx, pt_comm_message_t *msg, pt_frame_t *f);
sds dump_transform(pt_context_t *ctx, pt_comm_message_t *msg, pt_frame_t *f);
sds json_transform(pt_context_t *ctx, pt_comm_message_t *msg, pt_frame_t *f);

void trace_start(pt_context_t *ctx);
void trace(pt_context_t *ctx);
void trace_cleanup(pt_context_t *ctx);

void frame_free_sds(pt_frame_t *f);

#endif
