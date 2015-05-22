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

#ifndef TRACE_COMM_H
#define TRACE_COMM_H

#include <stdint.h>
#include "trace_mmap.h"
#include "trace_type.h"

#define PT_MAGIC_NUMBER                 0x6563617274706870 /* Ascii codes of "phptrace" */

#define PT_COMM_FILENAME                "phptrace.comm"
#define PT_COMM_SEQMAX                  1000
#define PT_COMM_T2E_SIZE                1048576 * 4  /* tool -> extension buffer size */
#define PT_COMM_E2T_SIZE                1048576 * 64 /* tool <- extension buffer size */

/* Type codes of inner message */
#define PT_MSG_EMPTY                    0x00000000
#define PT_MSG_ROTATE                   0x00000001
#define PT_MSG_RESEQ                    0x00000002
#define PT_MSG_INVALID                  0x00000003
#define PT_MSG_NORMAL                   0x10000000

/* Type codes of user defined message
 * Should >= 0x80000000 */
#define PT_MSG_RET                      0x80000000
#define PT_MSG_DO_BASE                  0x80010000
#define PT_MSG_DO_TRACE                 (PT_MSG_DO_BASE + 1)
#define PT_MSG_DO_STATUS                (PT_MSG_DO_BASE + 2)
#define PT_MSG_DO_PING                  (PT_MSG_DO_BASE + 4)

typedef struct {
    uint64_t size;              /* buffer size */
    void *head;                 /* head of buffer */
    void *current;              /* current pointer */
    uint32_t sequence;          /* current sequence */
} pt_comm_handler_t;

typedef struct {
    uint32_t seq;               /* message sequence */
    uint32_t type;              /* message type */
    uint32_t len;               /* message length */
    char data[];                /* scaleable data */
} pt_comm_message_t;

typedef struct {
    int8_t active;              /* active status */
    pt_segment_t seg;           /* mmap segment */
    char *filename;
    pt_comm_handler_t send_handler;
    pt_comm_handler_t recv_handler;
} pt_comm_socket_t;

typedef struct {
    uint64_t magic;
    size_t s2c_size;            /* server -> client handler size */
    size_t c2s_size;            /* server <- client handler size */
} pt_comm_socket_meta_t;

/* Some socket like functions */
#define pt_comm_swrite_begin(s, size)       pt_comm_write_begin(&(s)->send_handler, size)
#define pt_comm_swrite_end(s, type, msg)    pt_comm_write_end(&(s)->send_handler, type, msg)
#define pt_comm_swrite(s, type, buf, size)  pt_comm_write(&(s)->send_handler, type, buf, size)
#define pt_comm_sread(s, msgp, next)        pt_comm_read(&(s)->recv_handler, msgp, next)
#define pt_comm_sread_type(s)               (((pt_comm_message_t *) ((s)->recv_handler.current))->type)

#define pt_comm_offset(handler, msg) \
    (size_t) ((void *) msg - (handler)->head)
#define pt_comm_freesize(handler) \
    (size_t) ((handler)->size - ((handler)->current - (handler)->head)) - (size_t) sizeof(pt_comm_message_t)

int pt_comm_screate(pt_comm_socket_t *sock, const char *filename, int crossover, size_t s2c_size, size_t c2s_size);
int pt_comm_sopen(pt_comm_socket_t *sock, const char *filename, int crossover);
void pt_comm_sclose(pt_comm_socket_t *sock, int delfile);
void pt_comm_init(pt_comm_handler_t *handler, void *head, size_t size);
void pt_comm_uninit(pt_comm_handler_t *handler);
void pt_comm_clear(pt_comm_handler_t *handler);
pt_comm_message_t *pt_comm_next(pt_comm_handler_t *handler);
pt_comm_message_t *pt_comm_write_begin(pt_comm_handler_t *handler, size_t size);
void pt_comm_write_end(pt_comm_handler_t *handler, unsigned int type, pt_comm_message_t *msg);
pt_comm_message_t *pt_comm_write(pt_comm_handler_t *handler, unsigned int type, void *buf, size_t size);
unsigned int pt_comm_read(pt_comm_handler_t *handler, pt_comm_message_t **msg_ptr, int movenext);

pt_comm_message_t *pt_comm_write_message(uint32_t seq, uint32_t type, uint32_t len, pt_frame_t *f, void *buf);

#endif
