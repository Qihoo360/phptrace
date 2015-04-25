#ifndef PHPTRACE_COMM_H
#define PHPTRACE_COMM_H

#include <string.h>
#include "phptrace_mmap.h"

#define SEQ_MAX             10000
#define PT_MSG_EMPTY        0x00000000
#define PT_MSG_ROTATE       0x00000001
#define PT_MSG_RESEQ        0x00000002
/* User defined type should not begin with 0 at left-most bit */
#define PT_MSG_NORMAL       0x10000000

typedef struct {
    phptrace_segment_t seg; /* mmap segment */

    size_t size;            /* buffer size */
    void *head;             /* head of buffer */
    void *tail;             /* tail of buffer */

    void *current;          /* current pointer */
    unsigned int sequence;  /* current sequence */
} phptrace_comm_handler;

typedef struct {
    unsigned int seq;       /* message sequence */
    unsigned int type;      /* message type */
    size_t len;             /* message length */
    char data[];            /* scaleable data */
} phptrace_comm_message;

#define phptrace_comm_freesize(handler) \
    ((int) (handler->tail - handler->current) - (int) sizeof(phptrace_comm_message))

int phptrace_comm_create(phptrace_comm_handler *handler, const char *filename, size_t size);
int phptrace_comm_open(phptrace_comm_handler *handler, const char *filename);
void phptrace_comm_close(phptrace_comm_handler *handler);
void phptrace_comm_init(phptrace_comm_handler *handler);
void phptrace_comm_clear(phptrace_comm_handler *handler);
phptrace_comm_message *phptrace_comm_write_begin(phptrace_comm_handler *handler, size_t size);
void phptrace_comm_write_end(phptrace_comm_handler *handler, phptrace_comm_message *msg, unsigned int type);
phptrace_comm_message *phptrace_comm_write(phptrace_comm_handler *handler, unsigned int type, void *buf, size_t size);
phptrace_comm_message *phptrace_comm_read(phptrace_comm_handler *handler);
phptrace_comm_message *phptrace_comm_next(phptrace_comm_handler *handler);

#endif
