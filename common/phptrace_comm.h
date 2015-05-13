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

#ifndef PHPTRACE_COMM_H
#define PHPTRACE_COMM_H

#include <stdint.h>
#include "phptrace_mmap.h"
#include "phptrace_type.h"

#define PHPTRACE_LOG_DIR                "/tmp"
#define PHPTRACE_TRACE_FILENAME         "phptrace.trace"
#define PHPTRACE_CTRL_FILENAME          "phptrace.ctrl"
#define PHPTRACE_STATUS_FILENAME        "phptrace.status"
#define PHPTRACE_COMM_FILENAME          "phptrace.comm"

/*We use PID_MAX+1 as the size of file phptrace.ctrl
 *4 million is the hard limit of linux kernel so far,
 *It is 99999 on Mac OS X which is coverd by this value.
 *So 4*1024*1024 can serve both linux and unix(include darwin)
 * */
#define PID_MAX                         4194304             /* 4*1024*1024 */


#define PT_MAGIC_NUMBER     0x6563617274706870 /* ascii codes of "phptrace" */
#define PT_COMM_SEQMAX      1000

#define PT_MSG_EMPTY                    0x00000000
#define PT_MSG_ROTATE                   0x00000001
#define PT_MSG_RESEQ                    0x00000002
/* User defined type should not begin with 0 at left-most bit */
#define PT_MSG_NORMAL                   0x10000000

#define PT_MSG_TYPE_BASE                0
#define PT_MSG_DO_BASE                  (PT_MSG_TYPE_BASE + 0x00010000)
#define PT_MSG_DO_TRACE                 (PT_MSG_DO_BASE + 1)
#define PT_MSG_DO_STATUS                (PT_MSG_DO_BASE + 2)
#define PT_MSG_DO_FILTER                (PT_MSG_DO_BASE + 3)
#define PT_MSG_DO_PING                  (PT_MSG_DO_BASE + 4)

#define PT_MSG_GET_BASE                 (PT_MSG_TYPE_BASE + 0x00100000)
#define PT_MSG_GET_FILTER_ARGS          (PT_MSG_GET_BASE + 1);

#define PT_MSG_SET_BASE                 (PT_MSG_TYPE_BASE + 0x00110000)

#define PT_MSG_RET                      0x80000000

typedef struct {
    uint64_t size;              /* buffer size */
    void *head;                 /* head of buffer */
    void *current;              /* current pointer */
    uint32_t sequence;          /* current sequence */
} phptrace_comm_handler;

typedef struct {
    uint32_t seq;               /* message sequence */
    uint32_t type;              /* message type */
    uint32_t len;               /* message length */
    char data[];                /* scaleable data */
} phptrace_comm_message;

typedef struct {
    phptrace_segment_t seg;     /* mmap segment */
    char *filename;
    phptrace_comm_handler send_handler;
    phptrace_comm_handler recv_handler;
} phptrace_comm_socket;

typedef struct {
    uint64_t magic;
    size_t send_size;           /* send handler size */
    size_t recv_size;           /* recv handler size */
} phptrace_comm_socket_meta;

/* Some socket like functions */
#define phptrace_comm_swrite_begin(socket, size) \
    phptrace_comm_write_begin(&(socket)->send_handler, size)
#define phptrace_comm_swrite_end(socket, type, msg) \
    phptrace_comm_write_end(&(socket)->send_handler, type, msg)
#define phptrace_comm_swrite(socket, type, buf, size) \
    phptrace_comm_write(&(socket)->send_handler, type, buf, size)
#define phptrace_comm_sread(socket) \
    phptrace_comm_read(&(socket)->recv_handler, 1)

#define phptrace_comm_offset(handler, msg) \
    (size_t) ((void *) msg - (handler)->head)
#define phptrace_comm_freesize(handler) \
    (size_t) ((handler)->size - ((handler)->current - (handler)->head)) - (size_t) sizeof(phptrace_comm_message)

#define phptrace_comm_sread_type(p_socket) \
    (((phptrace_comm_message *) ((p_socket)->recv_handler.current))->type)

int phptrace_comm_screate(phptrace_comm_socket *sock, const char *filename, int crossover, size_t send_size, size_t recv_size);
int phptrace_comm_sopen(phptrace_comm_socket *sock, const char *filename, int crossover);
void phptrace_comm_sclose(phptrace_comm_socket *sock, int delfile);
void phptrace_comm_init(phptrace_comm_handler *handler, void *head, size_t size);
void phptrace_comm_uninit(phptrace_comm_handler *handler);
void phptrace_comm_clear(phptrace_comm_handler *handler);
phptrace_comm_message *phptrace_comm_next(phptrace_comm_handler *handler);
phptrace_comm_message *phptrace_comm_write_begin(phptrace_comm_handler *handler, size_t size);
void phptrace_comm_write_end(phptrace_comm_handler *handler, unsigned int type, phptrace_comm_message *msg);
phptrace_comm_message *phptrace_comm_write(phptrace_comm_handler *handler, unsigned int type, void *buf, size_t size);
phptrace_comm_message *phptrace_comm_read(phptrace_comm_handler *handler, int movenext);

phptrace_comm_message *phptrace_comm_write_message(uint32_t seq, uint32_t type, uint32_t len, phptrace_frame *f, void *buf);

#endif
