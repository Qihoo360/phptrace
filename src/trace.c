/**
 * Copyright 2015 Yuchen Wang <phobosw@gmail.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* socket(), bind() */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* sockaddr_un */

#include "cli.h"
#include "trace.h"

/* trace */
#include "trace_comm.h"
#include "trace_ctrl.h"
#include "trace_type.h"

/* select() */
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h> /* unlink() */

int pt_trace_main(void)
{
    int sfd;
    struct sockaddr_un saddr;
    struct sockaddr_un caddr;

    /* TODO config it */
    char *sock_path = "/tmp/phptrace.sock";
    pt_ctrl_t ctrlst;

    /* socket create */
    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("create");
        return -1;
    }

    /* socket address */
    memset(&saddr, 0, sizeof(struct sockaddr_un));
    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path, sock_path, sizeof(saddr.sun_path) - 1);

    /* socket bind */
    unlink(saddr.sun_path);
    if (bind(sfd, (struct sockaddr *) &saddr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        return -1;
    }

    /* socket listen */
    if (listen(sfd, 128) == -1) {
        perror("listen");
        return -1;
    }

    /* control init */
    if (pt_ctrl_open(&ctrlst, "/tmp/phptrace.ctrl") == -1) {
        pt_log(PT_ERROR, "ctrl open failed");
        return -1;
    }

    /* control switch on */
    if (clictx.pid == PT_PID_ALL) {
        pt_ctrl_set_all(&ctrlst, PT_CTRL_ACTIVE);
    } else {
        pt_ctrl_set_active(&ctrlst, clictx.pid);
    }

    /* TODO fd declaration */
    int maxfd, fd_count;
    struct timeval timeout = {1, 0};
    fd_set cfds, rfds;
    FD_ZERO(&cfds);
    maxfd = sfd;

    /* TODO */
    socklen_t calen;

    /* trace message */
    size_t msg_buflen;
    ssize_t rlen;
    void *msg_buf;
    pt_comm_message_t *msg;
    pt_frame_t framest;

    int i, cfd, sock_rcvbuf = 1024 * 1024;
    socklen_t optlen = sizeof(sock_rcvbuf);

    msg_buflen = sizeof(pt_comm_message_t);
    msg = msg_buf = malloc(msg_buflen);

    for (;;) {
        if (interrupted) {
            pt_ctrl_clear_all(&ctrlst);
            break;
        }

        memcpy(&rfds, &cfds, sizeof(fd_set)); /* TODO better way */
        FD_SET(sfd, &rfds);
        fd_count = select(maxfd + 1, &rfds, NULL, NULL, &timeout);
        if (fd_count <= 0) {
            /* timeout or error */
            continue;
        }

        /* accept */
        if (FD_ISSET(sfd, &rfds)) {
            calen = sizeof(caddr);
            cfd = accept(sfd, (struct sockaddr *) &caddr, &calen);
            if (cfd == -1) {
                perror("accept");
                return -1;
            }
            pt_log(PT_INFO, "accept %d", cfd);

            /* add to cfds */
            FD_SET(cfd, &cfds);
            if (cfd > maxfd) {
                maxfd = cfd;
            }

            /* send do_trace */
            msg->type = PT_MSG_DO_TRACE;
            msg->len = 0;
            rlen = send(cfd, msg, sizeof(pt_comm_message_t), 0);
            pt_log(PT_INFO, "send do_trace, return: %ld", rlen);

            /* set socket opt */
            /* http://man7.org/linux/man-pages/man7/socket.7.html */
            /* http://www.cyberciti.biz/faq/linux-tcp-tuning/ */
            if (getsockopt(cfd, SOL_SOCKET, SO_RCVBUF, &sock_rcvbuf, &optlen) == -1) {
                perror("getsockopt");
            }
            pt_log(PT_INFO, "rcvbuf before: %d %u", sock_rcvbuf, optlen);

            if (setsockopt(cfd, SOL_SOCKET, SO_RCVBUF, &sock_rcvbuf, optlen) == -1) {
                perror("setsockopt");
            }

            if (getsockopt(cfd, SOL_SOCKET, SO_RCVBUF, &sock_rcvbuf, &optlen) == -1) {
                perror("getsockopt");
            }
            pt_log(PT_INFO, "rcvbuf after: %d %u", sock_rcvbuf, optlen);

            /* accept as much as we can */
            continue;
        }

        /* recv trace data */
        for (cfd = sfd + 1; cfd < 256; cfd++) {
            if (!FD_ISSET(cfd, &rfds)) {
                continue;
            }
            for (i = 0; i < 10; i++) { /* process 10 frames max per process */
                /* header */
                rlen = recv(cfd, msg_buf, sizeof(pt_comm_message_t), MSG_DONTWAIT);
                pt_log(PT_INFO, "recv header return: %ld", rlen);
                if (rlen == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    break;
                } else if (rlen <= 0) {
                    FD_CLR(cfd, &cfds);
                    break;
                }
                pt_log(PT_INFO, "msg->type: %x len: %d", msg->type, msg->len);
                if (msg->type != PT_MSG_RET || msg->len <= 0) {
                    continue;
                }

                /* prepare body space */
                if (msg_buflen < sizeof(pt_comm_message_t) + msg->len) {
                    /* optimize dynamic alloc */
                    msg_buflen = sizeof(pt_comm_message_t) + msg->len;
                    msg_buf = realloc(msg_buf, msg_buflen);
                    msg = msg_buf; /* address may be changed after realloc */
                    pt_log(PT_INFO, "alloc %ld bytes for new buffer", msg_buflen);
                }

                /* body */
                rlen = recv(cfd, msg_buf + sizeof(pt_comm_message_t), msg->len, 0);
                pt_log(PT_INFO, "recv body return: %ld", rlen);
                if (rlen <= 0) {
                    FD_CLR(cfd, &cfds);
                    break;
                }

                /* unpack */
                pt_type_unpack_frame(&framest, msg->data);
                printf("[pid %5u]", 0);
                if (framest.type == PT_FRAME_ENTRY) {
                    pt_type_display_frame(&framest, 1, "> ");
                } else {
                    pt_type_display_frame(&framest, 1, "< ");
                }
            }
        }
    }

    return 0;
}
