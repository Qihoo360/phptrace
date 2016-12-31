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

#define DEINIT_RETURN(code) ret = code; goto deinit;

static int trace_single_process(int fd, int times)
{
    int i, msg_type;
    pt_comm_message_t *msg;
    pt_frame_t framest;
    pt_request_t requestst;

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

            case PT_MSG_FRAME:
                pt_type_unpack_frame(&framest, msg->data);
                printf("[pid %5u]", msg->pid);
                if (framest.type == PT_FRAME_ENTRY) {
                    pt_type_display_frame(&framest, 1, "> ");
                } else {
                    pt_type_display_frame(&framest, 1, "< ");
                }
                break;

            case PT_MSG_REQ:
                pt_type_unpack_request(&requestst, msg->data);
                printf("[pid %5u]", msg->pid);
                if (requestst.type == PT_FRAME_ENTRY) {
                    pt_type_display_request(&requestst, "> ");
                } else {
                    pt_type_display_request(&requestst, "< ");
                }
                break;

            default:
                pt_error("unknown message received with type 0x%08x", msg_type);
                break;
        }
    }

    return 0;
}

static int trace_ext(void)
{
    int cfd, maxfd, ret;
    fd_set client_fds, read_fds;
    struct timeval timeout;

    maxfd = clictx.sfd;
    FD_ZERO(&client_fds);
    FD_ZERO(&read_fds);

    for (;;) {
        if (interrupted) {
            DEINIT_RETURN(0);
        }

        memcpy(&read_fds, &client_fds, sizeof(fd_set));
        FD_SET(clictx.sfd, &read_fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        /* Waiting connect */
        ret = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret == -1) {
            if (errno == EINTR) {
                continue; /* leave interrupted to deal */
            } else {
                pt_error("Waiting for client connect failed");
                DEINIT_RETURN(-1);
            }
        } else if (ret == 0) {
            continue; /* timeout */
        }

        /* Client accept & send command */
        if (FD_ISSET(clictx.sfd, &read_fds)) {
            cfd = pt_comm_accept(clictx.sfd);
            printf("process attached\n");

            FD_SET(cfd, &client_fds);
            if (cfd > maxfd) {
                maxfd = cfd;
                pt_info("maxfd increase to %d", maxfd);
            }

            if (pt_comm_send_type(cfd, PT_MSG_DO_TRACE) == -1) {
                pt_error("Command sending failed");
            }

            continue; /* accept first */
        }

        /* recv trace data */
        for (cfd = clictx.sfd + 1; cfd <= maxfd; cfd++) {
            if (!FD_ISSET(cfd, &read_fds)) {
                continue;
            }

            if (trace_single_process(cfd, 10) == -1) {
                FD_CLR(cfd, &client_fds);
                pt_comm_close(cfd, NULL);
                printf("process detached\n");
            }
        }
    }

deinit:
    for (cfd = clictx.sfd + 1; cfd <= maxfd; cfd++) {
        if (!FD_ISSET(cfd, &client_fds)) {
            continue;
        }
        pt_comm_close(cfd, NULL);
        printf("process detached\n");
    }

    return ret;
}

int pt_trace_main(void)
{
    int ret;

    /* Control switch on */
    if (clictx.pid == PT_PID_ALL) {
        pt_ctrl_set_all(&clictx.ctrl, PT_CTRL_ACTIVE);
    } else {
        pt_ctrl_set_active(&clictx.ctrl, clictx.pid);
    }
    pt_debug("Control switch on");

    ret = trace_ext();

    /* Control switch off */
    if (clictx.pid == PT_PID_ALL) {
        pt_ctrl_clear_all(&clictx.ctrl);
    } else {
        pt_ctrl_set_inactive(&clictx.ctrl, clictx.pid);
    }
    pt_debug("Control switch off");

    return ret;
}
