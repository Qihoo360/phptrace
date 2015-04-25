#include "phptrace_comm.h"


/**
 * These functions doing a general propose communication between process
 * through a fixed-size memory (mmap).
 *
 * write example:
 * {
 *     phptrace_comm_handler ch;
 *     phptrace_comm_message *msg;
 * 
 *     phptrace_comm_create(&ch, filename, filesize);
 *
 *     msg = phptrace_comm_write_begin(&ch, your_source_len);
 *     memcpy(msg->data, your_source, msg->len);
 *     phptrace_comm_write_end(&ch, msg, PT_MSG_NORMAL);
 *
 *     phptrace_comm_close(&ch);
 * }
 *
 * read example:
 * {
 *     phptrace_comm_handler ch;
 *     phptrace_comm_message *msg;
 * 
 *     phptrace_comm_open(&ch, filename);
 *
 *     msg = phptrace_comm_read(&ch);
 *     if (msg->type != PT_MSG_EMPTY) {
 *         handle_message();
 *         phptrace_comm_next(&ch);
 *     }
 *
 *     phptrace_comm_close(&ch);
 * }
 */


int phptrace_comm_create(phptrace_comm_handler *handler, const char *filename, size_t size)
{
    handler->seg = phptrace_mmap_create(filename, size);
    if (handler->seg.shmaddr == MAP_FAILED) {
        return -1;
    }
    phptrace_comm_init(handler);
    return 0;
}

int phptrace_comm_open(phptrace_comm_handler *handler, const char *filename)
{
    handler->seg = phptrace_mmap_write(filename);
    if (handler->seg.shmaddr == MAP_FAILED) {
        return -1;
    }
    phptrace_comm_init(handler);
    return 0;
}

void phptrace_comm_init(phptrace_comm_handler *handler)
{
    /* init handler element */
    handler->size = handler->seg.size;
    handler->head = handler->seg.shmaddr;
    handler->tail = handler->seg.shmaddr + handler->seg.size;
    handler->current = handler->head;
    handler->sequence = 0;
}

void phptrace_comm_close(phptrace_comm_handler *handler)
{
    /* clear handler element */
    handler->size = handler->sequence = 0;
    handler->head = handler->tail = handler->current = NULL;

    phptrace_unmap(&handler->seg);
}

void phptrace_comm_clear(phptrace_comm_handler *handler)
{
    handler->current = handler->head;
    handler->sequence = 0;
    memset(handler->head, 0, handler->size);
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
    phptrace_comm_message *msg;

    if (size > handler->size) {
        return NULL;
    }

    /* rotate, because no space to hold message */
    if (phptrace_comm_freesize(handler) < size) {
        msg = (phptrace_comm_message *) handler->current;
        msg->seq = handler->sequence;
        msg->type = PT_MSG_ROTATE;
        msg->len = 0;

        handler->current = handler->head;
    }

    msg = (phptrace_comm_message *) handler->current;
    msg->seq = handler->sequence;
    msg->type = PT_MSG_EMPTY;
    msg->len = size;

    return msg;
}

void phptrace_comm_write_end(phptrace_comm_handler *handler, phptrace_comm_message *msg, unsigned int type)
{
    msg->type = type;

    phptrace_comm_next(handler);

    /* reset sequence */
    if (handler->sequence > SEQ_MAX) {
        handler->sequence = 0;
        phptrace_comm_write_end(handler, phptrace_comm_write_begin(handler, 0), PT_MSG_RESEQ);
    }

    phptrace_comm_write_begin(handler, 0); /* make next block empty */
}

/**
 * Quick-way to write from a alloced buffer
 */
phptrace_comm_message *phptrace_comm_write(phptrace_comm_handler *handler, unsigned int type, void *buf, size_t size)
{
    phptrace_comm_message *msg;

    msg = phptrace_comm_write_begin(handler, size);
    if (msg == NULL) {
        return NULL;
    }
    memcpy(msg->data, buf, size);
    phptrace_comm_write_end(handler, msg, type);

    return msg;
}


/**
 * Reading propose
 * --------------------
 */
phptrace_comm_message *phptrace_comm_read(phptrace_comm_handler *handler)
{
    phptrace_comm_message *msg = (phptrace_comm_message *) handler->current;

    /* handle sequence reset */
    if (msg->type == PT_MSG_RESEQ && msg->seq == 0) {
        handler->sequence = 0;
    }

    if (msg->seq != handler->sequence && msg->type != PT_MSG_EMPTY) {
        return NULL;
    }

    /* handle rotate */
    if (msg->type == PT_MSG_ROTATE) {
        handler->current = handler->head;
    }

    return (phptrace_comm_message *) handler->current;
}
