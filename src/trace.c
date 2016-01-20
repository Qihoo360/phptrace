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
    int i, sfd, cfd, maxfd, retval, msg_type;
    fd_set client_fds, read_fds;
    struct timeval timeout = {1, 0};

    pt_comm_socket_t sock;
    pt_ctrl_t ctrlst;
    pt_comm_message_t *msg;
    pt_frame_t framest;

    /* socket prepare */
    pt_comm_init(&sock);
    if (pt_comm_listen(&sock, "/tmp/" PT_COMM_FILENAME) == -1) {
        pt_log(PT_ERROR, "socket listen failed");
        return -1;
    }
    maxfd = sfd= sock.fd;

    /* control prepare */
    if (pt_ctrl_open(&ctrlst, "/tmp/" PT_CTRL_FILENAME) == -1) {
        pt_log(PT_ERROR, "ctrl open failed");
        return -1;
    }
    if (clictx.pid == PT_PID_ALL) {
        pt_ctrl_set_all(&ctrlst, PT_CTRL_ACTIVE);
    } else {
        pt_ctrl_set_active(&ctrlst, clictx.pid);
    }

    /* select prepare */
    FD_ZERO(&client_fds);
    FD_ZERO(&read_fds);

    for (;;) {
        if (interrupted) {
            pt_ctrl_clear_all(&ctrlst);
            return 0;
        }

        /* select */
        memcpy(&read_fds, &client_fds, sizeof(fd_set));
        FD_SET(sfd, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        retval = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
        if (retval == -1) {
            pt_log(PT_ERROR, "select return error");
            return -1;
        } else if (retval == 0) {
            continue; /* timeout */
        }

        /* accept */
        if (FD_ISSET(sfd, &read_fds)) {
            sock.fd = sfd;
            if ((cfd = pt_comm_accept(&sock)) == -1) {
                pt_log(PT_ERROR, "accept return error");
                continue;
            }
            FD_SET(cfd, &client_fds);
            pt_log(PT_INFO, "accepted %d", cfd);

            /* max fd */
            if (cfd > maxfd) {
                maxfd = cfd;
                pt_log(PT_INFO, "maxfd update to %d", cfd);
            }

            /* send do_trace */
            sock.fd = cfd;
            pt_comm_build_msg(&sock, &msg, 0, PT_MSG_DO_TRACE);
            retval = pt_comm_send_msg(&sock);
            if (retval != 0) {
                pt_log(PT_INFO, "send do_trace, return: %d error: %s", retval, strerror(errno));
            }

            continue; /* accept first */
        }

        /* read trace data */
        for (cfd = sfd + 1; cfd <= maxfd; cfd++) {
            if (!FD_ISSET(cfd, &read_fds)) {
                continue;
            }
            sock.fd = cfd;

            for (i = 0; i < 10; i++) { /* read 10 frames per process */
                msg_type = pt_comm_recv_msg(&sock, &msg);
                pt_log(PT_DEBUG, "recv message type: 0x%08x len: %d", msg_type, msg->len);

                switch (msg_type) {
                    case PT_MSG_PEERDOWN:
                    case PT_MSG_ERRSOCK:
                    case PT_MSG_ERRINNER:
                    case PT_MSG_INVALID:
                        pt_log(PT_WARNING, "recv message error errno: %d errmsg: %s", errno, strerror(errno));
                        FD_CLR(cfd, &client_fds);
                        close(cfd);
                        pt_log(PT_WARNING, "close %d", cfd);
                        i = 11; /* jump out for loop */
                        break;

                    case PT_MSG_EMPTY:
                        i = 11; /* jump out for loop */
                        break;

                    case PT_MSG_RET:
                        pt_type_unpack_frame(&framest, msg->data);
                        printf("[pid %5u]", msg->pid);
                        if (framest.type == PT_FRAME_ENTRY) {
                            pt_type_display_frame(&framest, 1, "> ");
                        } else {
                            pt_type_display_frame(&framest, 1, "< ");
                        }
                        break;

                    default:
                        pt_log(PT_ERROR, "Trace unknown message received with type 0x%08x", msg_type);
                        break;
                }
            }
        }
    }

    return 0;
}
