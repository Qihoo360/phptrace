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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include "trace_comm.h"

/* buffer */
static int pt_comm_buf_prepare(void **buf_ptr, size_t *bufsiz_ptr, size_t require)
{
    if (*bufsiz_ptr >= require) {
        return 0;
    }

    /* alloc double required space */
    *bufsiz_ptr = require * 2;

    if (*buf_ptr == NULL) {
        *buf_ptr = malloc(*bufsiz_ptr);
    } else {
        *buf_ptr = realloc(*buf_ptr, *bufsiz_ptr);
    }
    if (*buf_ptr == NULL) {
        return -1;
    }

    return 0;
}

/* socket */
int pt_comm_init(pt_comm_socket_t *sock)
{
    sock->fd = -1;
    sock->buf = NULL;
    sock->bufsiz = 0;

    return 0;
}

int pt_comm_connect(pt_comm_socket_t *sock, const char *addrstr)
{
    struct sockaddr_un addr;

    /* create */
    if (sock->fd == -1) {
        sock->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    }
    if (sock->fd == -1) {
        return -1;
    }

    /* address */
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, addrstr, sizeof(addr.sun_path) - 1);

    return connect(sock->fd, (struct sockaddr *) &addr, sizeof(addr));
}

int pt_comm_listen(pt_comm_socket_t *sock, const char *addrstr)
{
    struct sockaddr_un addr;

    /* create */
    if (sock->fd == -1) {
        sock->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    }
    if (sock->fd == -1) {
        return -1;
    }

    /* address */
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, addrstr, sizeof(addr.sun_path) - 1);

    /* bind */
    unlink(addr.sun_path);
    if (bind(sock->fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
        return -1;
    }

    /* listen */
    if (listen(sock->fd, 128) == -1) {
        return -1;
    }

    return 0;
}

int pt_comm_accept(pt_comm_socket_t *sock)
{
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);

    return accept(sock->fd, (struct sockaddr *) &addr, &addrlen);
}

int pt_comm_recv_msg(pt_comm_socket_t *sock, pt_comm_message_t **msg_ptr)
{
    int i;
    ssize_t retval, recvlen;
    pt_comm_message_t *msg;

    /* prepare buffer */
    if (pt_comm_buf_prepare(&sock->buf, &sock->bufsiz, PT_MSG_HEADER_SIZE) == -1) {
        return PT_MSG_ERRINNER;
    }
    msg = *msg_ptr = sock->buf;

    /* header */
    retval = recv(sock->fd, msg, PT_MSG_HEADER_SIZE, MSG_DONTWAIT);
    if (retval == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PT_MSG_EMPTY;
        } else {
            return PT_MSG_ERRSOCK;
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
    if (pt_comm_buf_prepare(&sock->buf, &sock->bufsiz, PT_MSG_SIZE(msg)) == -1) {
        return PT_MSG_ERRINNER;
    }
    msg = *msg_ptr = sock->buf;

    /* body */
    for (i = 0, recvlen = 0; i < PT_COMM_MAXRECV; i++) {
        retval = recv(sock->fd, msg->data + recvlen, msg->len - recvlen, 0); /* would block */
        if (retval == -1) {
            return PT_MSG_ERRSOCK;
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

int pt_comm_build_msg(pt_comm_socket_t *sock, pt_comm_message_t **msg_ptr, size_t size, int type)
{
    pt_comm_message_t *msg;

    /* prepare buffer */
    if (pt_comm_buf_prepare(&sock->buf, &sock->bufsiz, PT_MSG_HEADER_SIZE + size) == -1) {
        return PT_MSG_ERRINNER;
    }
    msg = *msg_ptr = sock->buf;

    /* construct */
    msg->len = size;
    msg->type = type;

    return msg->type;
}

int pt_comm_send_msg(pt_comm_socket_t *sock)
{
    ssize_t retval;
    pt_comm_message_t *msg = sock->buf;

    /* header */
    retval = send(sock->fd, msg, PT_MSG_HEADER_SIZE, 0);
    if (retval == -1) {
        return PT_MSG_ERRSOCK;
    }

    /* body */
    retval = send(sock->fd, msg->data, msg->len, 0);
    if (retval == -1) {
        return PT_MSG_ERRSOCK;
    }

    return 0;
}

int pt_comm_close(pt_comm_socket_t *sock)
{
    if (close(sock->fd) == -1) {
        return -1;
    }

    sock->fd = -1;
    return 0;
}
