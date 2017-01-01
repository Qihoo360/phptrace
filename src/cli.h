/**
 * Copyright 2017 Yuchen Wang <phobosw@gmail.com>
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

#ifndef TRACE_CLI_H
#define TRACE_CLI_H

#define PT_PID_INVALID      -1
#define PT_PID_ALL          0xF0000001
#define PT_LIMIT_UNLIMITED  -1

#define PT_DECR_LIMIT(type) ((clictx.limit.type != PT_LIMIT_UNLIMITED) \
    && (clictx.limit.type--))
#define PT_STOP_MATCH(type) (clictx.limit.type == 0)

#include "trace_ctrl.h"
#include "trace_comm.h"
#include "trace_filter.h"
#include "trace_type.h"

/* Context */
typedef struct {
    int command;
    int verbose;

    int pid;
    int ptrace;

    pt_ctrl_t ctrl;         /* control module */
    char ctrl_file[256];    /* ctrl filename */

    int sfd;                /* cli-tool socket fd */
    char listen_addr[256];  /* listen address */

    pt_filter_t pft;

    struct {
        int frame;          /* frame limit */
        int request;        /* request limit */
    }limit;
} pt_context_t;

/* make it accessable, global variables declared in cli.c */
extern pt_context_t clictx;
extern volatile int interrupted;

/* Verbose levels
 * inspired by RFC 5424 http://tools.ietf.org/html/rfc5424 */
enum {
    /* PT_EMERGENCY, */
    /* PT_ALERT, */
    /* PT_CRITICAL, */
    PT_ERROR, 
    PT_WARNING,
    /* PT_NOTICE, */
    PT_INFO, 
    PT_DEBUG, 
};

void pt_log(int level, char *filename, int lineno, const char *format, ...);

#define pt_error(format, ...)   pt_log(PT_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define pt_warning(format, ...) pt_log(PT_WARNING, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define pt_info(format, ...)    pt_log(PT_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define pt_debug(format, ...)   pt_log(PT_DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)

#endif
