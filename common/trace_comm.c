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

int pt_comm_socket(void)
{
    return socket(AF_UNIX, SOCK_STREAM, 0);
}

int pt_comm_connect(int fd, const char *addrstr)
{
    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, addrstr, strlen(addrstr));

    return connect(fd, (struct sockaddr *) &addr, sizeof(addr));
}

int pt_comm_recv_msg(int fd, void **buf_ptr, size_t *bufsiz_ptr)
{
    int i;
    ssize_t retval, recvlen;
    pt_comm_message_t *msg;

    /* prepare buffer */
    if (pt_comm_buf_prepare(buf_ptr, bufsiz_ptr, PT_MSG_HEADER_SIZE) == -1) {
        return PT_MSG_ERRINNER;
    }
    msg = *buf_ptr;

    /* header */
    retval = recv(fd, msg, PT_MSG_HEADER_SIZE, MSG_DONTWAIT);
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
    if (pt_comm_buf_prepare(buf_ptr, bufsiz_ptr, PT_MSG_SIZE(msg)) == -1) {
        return PT_MSG_ERRINNER;
    }
    msg = *buf_ptr;

    /* body */
    for (i = 0, recvlen = 0; i < PT_COMM_MAXRECV; i++) {
        retval = recv(fd, msg->data + recvlen, msg->len - recvlen, 0); /* would block */
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

int pt_comm_send_msg(int fd, pt_comm_message_t *msg)
{
    ssize_t retval;

    /* header */
    retval = send(fd, msg, PT_MSG_HEADER_SIZE, 0);
    if (retval == -1) {
        return PT_MSG_ERRSOCK;
    }

    /* body */
    retval = send(fd, msg->data, msg->len, 0);
    if (retval == -1) {
        return PT_MSG_ERRSOCK;
    }

    return 0;
}

int pt_comm_close(int fd)
{
    return close(fd);
}

int pt_comm_buf_init(void **buf_ptr, size_t *bufsiz_ptr)
{
    *bufsiz_ptr = 0;
    *buf_ptr = NULL;

    return 0;
}

int pt_comm_buf_prepare(void **buf_ptr, size_t *bufsiz_ptr, size_t require)
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
