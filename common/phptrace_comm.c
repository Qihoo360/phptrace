/* These functions doing a general propose communication between process on a
 * fixed-size memory through mmap. And provide some socket like interface. */

#include <string.h>
#include <unistd.h>
#include "phptrace_comm.h"


int phptrace_comm_screate(phptrace_comm_socket *sock, const char *filename, int crossover, size_t send_size, size_t recv_size)
{
    void *p;
    phptrace_comm_socket_meta *meta;

    sock->filename = (char *) filename;
    /* TODO use zero fill this file before mmap, make sure another process has
     * a clear space after mmap immediately. */
    sock->seg = phptrace_mmap_create(filename, sizeof(phptrace_comm_socket_meta) + send_size + recv_size);
    if (sock->seg.shmaddr == MAP_FAILED) {
        return -1;
    }

    /* Init meta info */
    meta = (phptrace_comm_socket_meta *) sock->seg.shmaddr;
    meta->magic = 0;
    meta->send_size = send_size;
    meta->recv_size = recv_size;
    p = sock->seg.shmaddr + sizeof(phptrace_comm_socket_meta);

    /* Init, attach handler */
    if (crossover) {
        phptrace_comm_init(&sock->recv_handler, p, meta->send_size);
        phptrace_comm_init(&sock->send_handler, p + meta->send_size, meta->recv_size);
    } else {
        phptrace_comm_init(&sock->send_handler, p, meta->send_size);
        phptrace_comm_init(&sock->recv_handler, p + meta->send_size, meta->recv_size);
    }
    phptrace_comm_clear(&sock->send_handler);
    phptrace_comm_clear(&sock->recv_handler);

    /* Make it accessable at last step */
    meta->magic = PT_MAGIC_NUMBER;

    return 0;
}

int phptrace_comm_sopen(phptrace_comm_socket *sock, const char *filename, int crossover)
{
    void *p;
    phptrace_comm_socket_meta *meta;

    sock->filename = (char *) filename;
    sock->seg = phptrace_mmap_write(filename);
    if (sock->seg.shmaddr == MAP_FAILED) {
        return -1;
    }

    /* Init meta info */
    meta = (phptrace_comm_socket_meta *) sock->seg.shmaddr;
    if (meta->magic != PT_MAGIC_NUMBER) {
        return -1;
    }
    p = sock->seg.shmaddr + sizeof(phptrace_comm_socket_meta);

    /* Attach handler */
    if (crossover) {
        phptrace_comm_init(&sock->recv_handler, p, meta->send_size);
        phptrace_comm_init(&sock->send_handler, p + meta->send_size, meta->recv_size);
    } else {
        phptrace_comm_init(&sock->send_handler, p, meta->send_size);
        phptrace_comm_init(&sock->recv_handler, p + meta->send_size, meta->recv_size);
    }

    return 0;
}

void phptrace_comm_sclose(phptrace_comm_socket *sock, int delfile)
{
    phptrace_comm_uninit(&sock->send_handler);
    phptrace_comm_uninit(&sock->recv_handler);

    phptrace_unmap(&sock->seg);
    sock->seg.shmaddr = MAP_FAILED;
    sock->seg.size = 0;

    if (delfile) {
        unlink(sock->filename);
    }
}

void phptrace_comm_init(phptrace_comm_handler *handler, void *head, size_t size)
{
    /* init handler element */
    handler->size = size;
    handler->head = head;

    handler->current = handler->head;
    handler->sequence = 0;
}

void phptrace_comm_uninit(phptrace_comm_handler *handler)
{
    handler->size = handler->sequence = 0;
    handler->head = handler->current = NULL;
}

void phptrace_comm_clear(phptrace_comm_handler *handler)
{
    memset(handler->head, 0, handler->size);

    handler->current = handler->head;
    handler->sequence = 0;
}

phptrace_comm_message *phptrace_comm_next(phptrace_comm_handler *handler)
{
    phptrace_comm_message *msg = (phptrace_comm_message *) handler->current;

    handler->sequence++;

    if (phptrace_comm_freesize(handler) - msg->len < sizeof(phptrace_comm_message)) {
        /* rotate, because no space to hold message meta info */
        return handler->current = handler->head;
    } else {
        return handler->current = handler->current + sizeof(phptrace_comm_message) + msg->len;
    }
}


/**
 * Writing propose
 * --------------------
 */
phptrace_comm_message *phptrace_comm_write_begin(phptrace_comm_handler *handler, size_t size)
{
    phptrace_comm_message *msg = (phptrace_comm_message *) handler->current;

    if (size > handler->size) {
        return NULL;
    }

    /* rotate, because no space to hold message */
    if (phptrace_comm_freesize(handler) < size) {
        msg->seq = handler->sequence;
        msg->type = PT_MSG_ROTATE;
        msg->len = 0;

        msg = (phptrace_comm_message *) (handler->current = handler->head);
    }

    /* set message except real type */
    msg->seq = handler->sequence;
    msg->type = PT_MSG_EMPTY;
    msg->len = size;

    return msg;
}

void phptrace_comm_write_end(phptrace_comm_handler *handler, unsigned int type, phptrace_comm_message *msg)
{
    /* make next block empty */
    phptrace_comm_next(handler);
    phptrace_comm_write_begin(handler, 0);

    /* set message type, make it readable */
    msg->type = type;

    /* reset sequence */
    if (handler->sequence > PT_COMM_SEQMAX) {
        handler->sequence = 0;
        phptrace_comm_write_end(handler, PT_MSG_RESEQ, phptrace_comm_write_begin(handler, 0));
    }
}

/**
 * Quick way to write from a alloced buffer
 */
phptrace_comm_message *phptrace_comm_write(phptrace_comm_handler *handler, unsigned int type, void *buf, size_t size)
{
    phptrace_comm_message *msg;

    msg = phptrace_comm_write_begin(handler, size);
    if (msg == NULL) {
        return NULL;
    }
    if (buf != NULL && size > 0) {
        memcpy(msg->data, buf, size);
    }
    phptrace_comm_write_end(handler, type, msg);

    return msg;
}

/**
 * dump a frame to a buffer with message header
 */
phptrace_comm_message *phptrace_comm_write_message(uint32_t seq, uint32_t type, uint32_t len, phptrace_frame *f, void *buf)
{
    phptrace_comm_message *msg = (phptrace_comm_message *)buf;

    /* set message except real type */
    msg->seq = seq;
    msg->type = type;
    msg->len = len;
    phptrace_type_pack_frame(f, msg->data);

    return msg;
}


/**
 * Reading propose
 * --------------------
 */
phptrace_comm_message *phptrace_comm_read(phptrace_comm_handler *handler, int movenext)
{
    phptrace_comm_message *msg = (phptrace_comm_message *) handler->current;

    /* Handle special type */
    if (msg->type == PT_MSG_RESEQ) {
        /* Sequence reset */
        handler->sequence = 0;
        phptrace_comm_next(handler);

        return phptrace_comm_read(handler, movenext);
    } else if (msg->type == PT_MSG_ROTATE) {
        /* Rotate */
        handler->current = handler->head;
        msg = (phptrace_comm_message *) handler->current;
    }

    /* Invalid type */
    if (msg->type == PT_MSG_EMPTY) {
        /* Empty */
        return NULL;
    } else if (msg->seq != handler->sequence) {
        /* Sequence error */
        return NULL;
    }

    if (movenext) {
        phptrace_comm_next(handler);
    }

    return msg;
}

