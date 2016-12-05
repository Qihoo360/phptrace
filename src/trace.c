/**
 * Copyright 2016 Yuchen Wang <phobosw@gmail.com>
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
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include "trace_comm.h"
#include "trace_ctrl.h"
#include "trace_type.h"

#include "cli.h"
#include "trace.h"

static int trace_single_process(int fd, int times)
{
    int i, msg_type;
    pt_comm_message_t *msg;
    pt_frame_t framest;

    for (i = 0; i < times; i++) {
        msg_type = pt_comm_recv_msg(fd, &msg);
        pt_debug("recv message type: 0x%08x len: %d", msg_type, msg->len);

        switch (msg_type) {
            case PT_MSG_PEERDOWN:
            case PT_MSG_ERR_SOCK:
            case PT_MSG_ERR_BUF:
            case PT_MSG_INVALID:
                pt_warning("recv message error");
                return -1;

            case PT_MSG_EMPTY:
                return 0;

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
                pt_error("unknown message received with type 0x%08x", msg_type);
                break;
        }
    }

    return i;
}

int pt_trace_main(void)
{
    int sfd, cfd, maxfd, retval;
    fd_set client_fds, read_fds;
    struct timeval timeout;

    pt_ctrl_t ctrlst;

    /* socket prepare */
    maxfd = sfd = pt_comm_listen("/tmp/" PT_COMM_FILENAME);
    if (sfd == -1) {
        pt_error("socket listen failed");
        return -1;
    }

    /* control prepare */
    if (pt_ctrl_open(&ctrlst, "/tmp/" PT_CTRL_FILENAME) == -1) {
        pt_error("ctrl open failed");
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
            pt_info("clear all ctrl bits");
            pt_comm_close(sfd, "/tmp/" PT_COMM_FILENAME);
            return 0;
        }

        /* select */
        memcpy(&read_fds, &client_fds, sizeof(fd_set));
        FD_SET(sfd, &read_fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        retval = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
        if (retval == -1) {
            if (errno == EINTR) {
                continue; /* leave interrupted to deal */
            } else {
                pt_warning("select error");
                return -1;
            }
        } else if (retval == 0) {
            continue; /* timeout */
        }

        /* client connected */
        if (FD_ISSET(sfd, &read_fds)) {
            /* accept */
            cfd = pt_comm_accept(sfd);
            if (cfd == -1) {
                pt_error("accept return error");
                continue;
            }

            FD_SET(cfd, &client_fds);
            printf("process attached\n");

            /* max fd */
            if (cfd > maxfd) {
                maxfd = cfd;
                pt_info("maxfd update to %d", cfd);
            }

            /* send do_trace */
            if (pt_comm_send_type(cfd, PT_MSG_DO_TRACE) == -1) {
                pt_error("send do_trace error");
            }

            continue; /* accept first */
        }

        /* read trace data */
        for (cfd = sfd + 1; cfd <= maxfd; cfd++) {
            if (!FD_ISSET(cfd, &read_fds)) {
                continue;
            }

            if (trace_single_process(cfd, 10) == -1) {
                /* client disconnect */
                FD_CLR(cfd, &client_fds);
                pt_comm_close(cfd, NULL);
                printf("process detached\n");
            }
        }
    }

    return 0;
}
