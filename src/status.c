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
#include "ptrace.h"

int pt_status_ptrace(void)
{
    void *addr_current_ex;
    pt_status_t statusst;

    if (clictx.pid == PT_PID_ALL) {
        pt_error("Status mode supports only single process");
        return -1;
    }

    if (pt_ptrace_attach(clictx.pid) == -1) {
        pt_error("ptrace attach failed");
        return -1;
    }
    pt_info("ptrace attach success");

    /* TODO get php version */
    pt_ptrace_preset_t *preset = pt_ptrace_preset(50619);

    /* Fetch current_execute_data */
    addr_current_ex = pt_ptrace_fetch_current_ex(preset, clictx.pid);
    if (addr_current_ex == NULL) {
        pt_error("PHP is in-active\n");
        return -1;
    }

    /* Fetch status */
    if (pt_ptrace_build_status(&statusst, preset, clictx.pid, addr_current_ex) != 0) {
        pt_error("ptrace fetch failed");
        return -1;
    }
    pt_type_display_status(&statusst);
    pt_type_destroy_status(&statusst, 1);

    if (pt_ptrace_detach(clictx.pid) == -1) {
        pt_error("ptrace detach failed");
        return -1;
    }
    pt_info("ptrace detach success");

    return 0;
}

int pt_status_main(void)
{
    int sfd, cfd, ret, msg_type;
    fd_set read_fds;
    struct timeval timeout;

    pt_ctrl_t ctrlst;
    pt_comm_message_t *msg;
    pt_status_t statusst;

    if (clictx.pid == PT_PID_ALL) {
        pt_error("Status mode supports only single process");
        return -1;
    }

    /* control prepare */
    if (pt_ctrl_open(&ctrlst, "/tmp/" PT_CTRL_FILENAME) == -1) {
        pt_error("ctrl open failed");
        return -1;
    }

    /* socket prepare */
    sfd = pt_comm_listen("/tmp/" PT_COMM_FILENAME);
    if (sfd == -1) {
        pt_error("socket listen failed");
        return -1;
    }

    pt_ctrl_set_active(&ctrlst, clictx.pid);

    /* select prepare */
    FD_ZERO(&read_fds);
    FD_SET(sfd, &read_fds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    ret = select(sfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == -1) {
        pt_error("Waiting client connect failed");
        goto end;
    } else if (ret == 0) {
        pt_warning("Waiting client connect timeout");
        goto end;
    }

    /* client accept */
    cfd = pt_comm_accept(sfd);
    pt_info("client accepted fd:%d", cfd);

    /* send do_trace */
    pt_debug("send do_status");
    if (pt_comm_send_type(cfd, PT_MSG_DO_STATUS) == -1) {
        pt_error("Operation sending failed");
        goto end;
    }

    /* wait message */
    FD_ZERO(&read_fds);
    FD_SET(cfd, &read_fds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    ret = select(cfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == -1) {
        pt_error("Waiting client send failed");
        goto end;
    } else if (ret == 0) {
        pt_warning("Waiting client send timeout");
        goto end;
    }

    /* recv message */
    msg_type = pt_comm_recv_msg(cfd, &msg);
    pt_debug("recv message type: 0x%08x len: %d", msg_type, msg->len);
    if (msg_type == PT_MSG_STATUS) {
        pt_type_unpack_status(&statusst, msg->data);
        pt_type_display_status(&statusst);
        pt_type_destroy_status(&statusst, 1);
    } else {
        pt_error("unknown message received with type 0x%08x", msg_type);
        goto end;
    }

end:
    pt_comm_close(sfd, NULL);
    if (cfd) {
        pt_comm_close(cfd, NULL);
    }

    return 0;
}
