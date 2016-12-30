/**
 * Copyright 2016 Yuchen Wang <phobosw@gmail.com>
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

#define PT_PID_INVALID  -1
#define PT_PID_ALL      0xF0000001

/* Context */
typedef struct {
    int command;
    int verbose;

    int pid;
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

int pt_log(int level, const char *format, ...);

#endif
