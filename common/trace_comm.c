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

/* These functions doing a general propose communication between process on a
 * fixed-size memory through mmap. And provide some socket like interface. */

#include <string.h>
#include <unistd.h>
#include "trace_comm.h"

int pt_comm_screate(pt_comm_socket_t *sock, const char *filename, int crossover, size_t s2c_size, size_t c2s_size)
{
    void *p;
    pt_comm_socket_meta_t *meta;

    sock->filename = (char *) filename;
    /* TODO use zero fill this file before mmap, make sure another process has
     * a clear space after mmap immediately. */
    if (pt_mmap_create(&sock->seg, sock->filename, sizeof(pt_comm_socket_meta_t) + s2c_size + c2s_size) < 0) {
        return -1;
    }

    /* Init meta info */
    meta = (pt_comm_socket_meta_t *) sock->seg.addr;
    meta->magic = 0;
    meta->s2c_size = s2c_size;
    meta->c2s_size = c2s_size;
    p = sock->seg.addr + sizeof(pt_comm_socket_meta_t);

    /* Attach handler */
    if (crossover) {
        /* recv: server -> client
         * send: server <- client */
        pt_comm_init(&sock->recv_handler, p, meta->s2c_size);
        pt_comm_init(&sock->send_handler, p + meta->s2c_size, meta->c2s_size);
    } else {
        /* send: server -> client
         * recv: server <- client */
        pt_comm_init(&sock->send_handler, p, meta->s2c_size);
        pt_comm_init(&sock->recv_handler, p + meta->s2c_size, meta->c2s_size);
    }
    pt_comm_clear(&sock->send_handler);
    pt_comm_clear(&sock->recv_handler);

    /* Make it accessable at last step */
    meta->magic = PT_MAGIC_NUMBER;
    sock->active = 1;

    return 0;
}

int pt_comm_sopen(pt_comm_socket_t *sock, const char *filename, int crossover)
{
    void *p;
    pt_comm_socket_meta_t *meta;

    sock->filename = (char *) filename;
    if (pt_mmap_open(&sock->seg, filename, 0) < 0) {
        return -1;
    }

    /* Init meta info */
    meta = (pt_comm_socket_meta_t *) sock->seg.addr;
    if (meta->magic != PT_MAGIC_NUMBER) {
        return -1;
    }
    p = sock->seg.addr + sizeof(pt_comm_socket_meta_t);

    /* Attach handler */
    if (crossover) {
        /* recv: server -> client
         * send: server <- client */
        pt_comm_init(&sock->recv_handler, p, meta->s2c_size);
        pt_comm_init(&sock->send_handler, p + meta->s2c_size, meta->c2s_size);
    } else {
        /* send: server -> client
         * recv: server <- client */
        pt_comm_init(&sock->send_handler, p, meta->s2c_size);
        pt_comm_init(&sock->recv_handler, p + meta->s2c_size, meta->c2s_size);
    }

    sock->active = 1;

    return 0;
}

void pt_comm_sclose(pt_comm_socket_t *sock, int delfile)
{
    sock->active = 0;

    pt_comm_uninit(&sock->send_handler);
    pt_comm_uninit(&sock->recv_handler);
    pt_mmap_close(&sock->seg);

    if (delfile) {
        unlink(sock->filename);
    }
    sock->filename = NULL;
}

void pt_comm_init(pt_comm_handler_t *handler, void *head, size_t size)
{
    /* init handler element */
    handler->size = size;
    handler->head = head;

    handler->current = handler->head;
    handler->sequence = 0;
}

void pt_comm_uninit(pt_comm_handler_t *handler)
{
    handler->size = handler->sequence = 0;
    handler->head = handler->current = NULL;
}

void pt_comm_clear(pt_comm_handler_t *handler)
{
    memset(handler->head, 0, handler->size);

    handler->current = handler->head;
    handler->sequence = 0;
}

pt_comm_message_t *pt_comm_next(pt_comm_handler_t *handler)
{
    pt_comm_message_t *msg = (pt_comm_message_t *) handler->current;

    handler->sequence++;

    if (pt_comm_freesize(handler) - msg->len < sizeof(pt_comm_message_t)) {
        /* rotate, because no space to hold message meta info */
        return handler->current = handler->head;
    } else {
        return handler->current = handler->current + sizeof(pt_comm_message_t) + msg->len;
    }
}

pt_comm_message_t *pt_comm_write_begin(pt_comm_handler_t *handler, size_t size)
{
    pt_comm_message_t *msg = (pt_comm_message_t *) handler->current;

    if (size > handler->size) {
        return NULL;
    }

    /* rotate, because no space to hold message */
    if (pt_comm_freesize(handler) < size) {
        msg->seq = handler->sequence;
        msg->type = PT_MSG_ROTATE;
        msg->len = 0;

        msg = (pt_comm_message_t *) (handler->current = handler->head);
    }

    /* set message except real type */
    msg->seq = handler->sequence;
    msg->type = PT_MSG_EMPTY;
    msg->len = size;

    return msg;
}

void pt_comm_write_end(pt_comm_handler_t *handler, unsigned int type, pt_comm_message_t *msg)
{
    /* make next block empty */
    pt_comm_next(handler);
    pt_comm_write_begin(handler, 0);

    /* set message type, make it readable */
    msg->type = type;

    /* reset sequence */
    if (handler->sequence > PT_COMM_SEQMAX) {
        handler->sequence = 0;
        pt_comm_write_end(handler, PT_MSG_RESEQ, pt_comm_write_begin(handler, 0));
    }
}

/**
 * Quick way to write from a alloced buffer
 */
pt_comm_message_t *pt_comm_write(pt_comm_handler_t *handler, unsigned int type, void *buf, size_t size)
{
    pt_comm_message_t *msg;

    msg = pt_comm_write_begin(handler, size);
    if (msg == NULL) {
        return NULL;
    }
    if (buf != NULL && size > 0) {
        memcpy(msg->data, buf, size);
    }
    pt_comm_write_end(handler, type, msg);

    return msg;
}

/**
 * dump a frame to a buffer with message header
 */
pt_comm_message_t *pt_comm_write_message(uint32_t seq, uint32_t type, uint32_t len, pt_frame_t *f, void *buf)
{
    pt_comm_message_t *msg = (pt_comm_message_t *)buf;

    /* set message except real type */
    msg->seq = seq;
    msg->type = type;
    msg->len = len;
    pt_type_pack_frame(f, msg->data);

    return msg;
}

unsigned int pt_comm_read(pt_comm_handler_t *handler, pt_comm_message_t **msg_ptr, int movenext)
{
    pt_comm_message_t *msg = (pt_comm_message_t *) handler->current;

    /* Handle special type */
    if (msg->type == PT_MSG_RESEQ) {
        /* Sequence reset */
        handler->sequence = 0;
        pt_comm_next(handler);

        return pt_comm_read(handler, msg_ptr, movenext);
    } else if (msg->type == PT_MSG_ROTATE) {
        /* Rotate */
        handler->current = handler->head;
        msg = (pt_comm_message_t *) handler->current;
    }

    /* Invalid types */
    if (msg->type == PT_MSG_EMPTY) {
        /* Empty */
        return PT_MSG_EMPTY;
    } else if (msg->seq != handler->sequence) {
        /* Sequence error */
        return PT_MSG_INVALID;
    }

    if (movenext) {
        pt_comm_next(handler);
    }

    /* normal */
    *msg_ptr = msg;
    return msg->type;
}
