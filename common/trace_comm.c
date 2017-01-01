/**
 * Copyright 2017 Qihoo 360
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

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include "trace_comm.h"

/* buffer */
typedef struct {
    void *data;
    size_t size;
} pt_comm_buffer_t;

static pt_comm_buffer_t bufst = {NULL, 0}, *buf = &bufst;

static int buf_prepare(size_t require)
{
    if (buf->size >= require) {
        return 0;
    }

    /* alloc double required space */
    buf->size = require * 2;

    if (buf->size < 4096) {
        buf->size = 4096;
    }

    if (buf->data == NULL) {
        buf->data = malloc(buf->size);
    } else {
        buf->data = realloc(buf->data, buf->size);
    }
    if (buf->data == NULL) {
        return -1;
    }

    return 0;
}

/* socket */
int pt_comm_connect(const char *addrstr)
{
    int fd;
    struct sockaddr_un addr;

    /* create */
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        return -1;
    }

    /* address */
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, addrstr, sizeof(addr.sun_path) - 1);

    /* connect */
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        return -1;
    }

    return fd;
}

int pt_comm_listen(const char *addrstr)
{
    int fd;
    struct sockaddr_un addr;

    /* create */
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        return -1;
    }

    /* address */
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, addrstr, sizeof(addr.sun_path) - 1);

    /* bind */
    if (unlink(addr.sun_path) == -1 && errno != ENOENT) {
        return -1;
    }
    if (bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
        return -1;
    }
    if (chmod(addr.sun_path, ALLPERMS) == 0) {
        /* errno may be non-zero, but return 0 */
        errno = 0;
    }

    /* listen */
    if (listen(fd, PT_COMM_BACKLOG) == -1) {
        return -1;
    }

    return fd;
}

int pt_comm_accept(int fd)
{
    /* useless... */
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);

    return accept(fd, (struct sockaddr *) &addr, &addrlen);
}

int pt_comm_recv_msg(int fd, pt_comm_message_t **msg_ptr)
{
    int i;
    ssize_t retval, recvlen;
    pt_comm_message_t *msg;

    /* prepare buffer */
    if (buf_prepare(PT_MSG_HEADER_SIZE) == -1) {
        return PT_MSG_ERR_BUF;
    }
    msg = *msg_ptr = buf->data;

    /* header */
    retval = recv(fd, msg, PT_MSG_HEADER_SIZE, MSG_DONTWAIT);
    if (retval == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PT_MSG_EMPTY;
        } else {
            return PT_MSG_ERR_SOCK;
        }
    } else if (retval == 0) {
        return PT_MSG_PEERDOWN;
    } else if (retval != PT_MSG_HEADER_SIZE) {
        return PT_MSG_INVALID; /* recv wrong size */
    } else if (msg->len > PT_MSG_SIZE_MAX || msg->len < PT_MSG_SIZE_MIN) {
        return PT_MSG_INVALID; /* msg->size invalid */
    }

    /* only header */
    if (msg->len == 0) {
        return msg->type;
    }

    /* prepare buffer */
    if (buf_prepare(PT_MSG_SIZE(msg)) == -1) {
        return PT_MSG_ERR_BUF;
    }
    msg = *msg_ptr = buf->data;

    /* body */
    for (i = 0, recvlen = 0; i < PT_COMM_MAXRECV; i++) {
        retval = recv(fd, msg->data + recvlen, msg->len - recvlen, 0); /* would block */
        if (retval == -1) {
            return PT_MSG_ERR_SOCK;
        } else if (retval == 0) {
            return PT_MSG_PEERDOWN;
        }

        recvlen += retval;

        /* recv finish */
        if (recvlen == msg->len) {
            break;
        }
    }
    if (recvlen != msg->len) {
        return PT_MSG_INVALID; /* recv wrong size */
    }

    /* all fine */
    return msg->type;
}

int pt_comm_build_msg(pt_comm_message_t **msg_ptr, size_t size, int type)
{
    pt_comm_message_t *msg;

    /* prepare buffer */
    if (buf_prepare(PT_MSG_HEADER_SIZE + size) == -1) {
        return -1;
    }
    msg = *msg_ptr = buf->data;

    /* construct */
    msg->len = size;
    msg->type = type;

    return 0;
}

int pt_comm_send_type(int fd, int type)
{
    pt_comm_message_t *msg;

    if (pt_comm_build_msg(&msg, 0, type) == -1) {
        return -1;
    }

    return pt_comm_send_msg(fd, msg);
}

int pt_comm_send_msg(int fd, pt_comm_message_t *msg)
{
    ssize_t retval;

    /* header */
    retval = send(fd, msg, PT_MSG_HEADER_SIZE, 0);
    if (retval == -1) {
        return -1;
    }

    /* only header */
    if (msg->len == 0) {
        return 0;
    }

    /* body */
    retval = send(fd, msg->data, msg->len, 0);
    if (retval == -1) {
        return -1;
    }

    return 0;
}

int pt_comm_close(int fd, const char *addrstr)
{
    if (addrstr != NULL) {
        unlink(addrstr);
    }

    return close(fd);
}
