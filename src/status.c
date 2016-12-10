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
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "trace_comm.h"
#include "trace_ctrl.h"
#include "trace_type.h"

#include "cli.h"
#include "trace.h"
#include "ptrace.h"

struct php_info {
    sds binary;
    int version;

    void *sapi_module;
    void *sapi_globals;
    void *executor_globals;
};

static int get_php_info(struct php_info *info, pid_t pid)
{
    FILE *fp;
    long addr;
    ssize_t len;
    char path[128], buf[BUFSIZ];

    /* PHP binary path */
    sprintf(path, "/proc/%d/exe", pid);
    len = readlink(path, buf, PATH_MAX);
    if (len == -1 || len >= PATH_MAX) {
        return -1;
    }
    info->binary = sdsnewlen(buf, len);
    pt_info("PHP binary: %s", info->binary);

    /* PHP version */
    sprintf(buf, "%s -v | awk 'NR == 1 {print $2}'", info->binary);
    pt_debug("popen commmand: %s", buf);
    if ((fp = popen(buf, "r")) == NULL) {
        return -1;
    }
    fgets(buf, sizeof(buf), fp);
    pclose(fp);

    /* Convert version string "5.6.3" to id 50603 */
    if (strlen(buf) < 5 || (buf[0] != '7' && buf[0] != '5') || buf[1] != '.' || buf[3] != '.') {
        pt_error("invalid PHP version string \"%s\"", buf);
        return -1;
    }
    info->version  = (buf[0] - '0') * 10000;
    info->version += (buf[2] - '0') * 100;
    if (isdigit(buf[5])) {
        info->version += (buf[4] - '0') * 10;
        info->version += (buf[5] - '0') * 1;
    } else {
        info->version += (buf[4] - '0') * 1;
    }
    pt_info("PHP version id: %d", info->version);

    /* global address */
    sprintf(buf, "gdb --batch -nx %s %d "
                 "-ex 'p &sapi_module' -ex 'p &sapi_globals' -ex 'p &executor_globals' "
                 "2>/dev/null | awk '$1 ~ /^\\$/ {print $1 $(NF-1)}'" , info->binary, pid);
    pt_debug("popen commmand: %s", buf);
    if ((fp = popen(buf, "r")) == NULL) {
        return -1;
    }
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        addr = strtol(buf + 4, NULL, 16);

        if (addr == 0) {
            /* some unexpected happend... */
            pt_error("global address invalid, set more verbose to check");
            pclose(fp);
            return -1;
        }

        if (buf[1] == '1') {
            info->sapi_module = (void *) addr;
        } else if (buf[1] == '2') {
            info->sapi_globals = (void *) addr;
        } else if (buf[1] == '3') {
            info->executor_globals = (void *) addr;
        }
    }
    pclose(fp);

    pt_info("sapi_module:       0x%lx", info->sapi_module);
    pt_info("sapi_globals:      0x%lx", info->sapi_globals);
    pt_info("executor_globals:  0x%lx", info->executor_globals);

    return 0;
}

int pt_status_ptrace(void)
{
    void *addr_current_ex;
    struct php_info info;
    pt_status_t statusst;

    if (get_php_info(&info, clictx.pid) == -1) {
        pt_error("Fetching PHP info failed");
        return -1;
    }

    pt_ptrace_preset_t *preset = pt_ptrace_preset(info.version, info.sapi_module,
            info.sapi_globals, info.executor_globals);
    if (preset == NULL) {
        pt_error("Preset not found for PHP %d", info.version);
        return -1;
    }

    if (pt_ptrace_attach(clictx.pid) == -1) {
        pt_error("ptrace attach failed");
        return -1;
    }
    pt_info("ptrace attach success");

    /* Fetch current_execute_data */
    addr_current_ex = pt_ptrace_fetch_current_ex(preset, clictx.pid);
    if (addr_current_ex == NULL) {
        pt_error("Process %d not active", clictx.pid);
        return -1;
    }

    /* Fetch status */
    if (pt_ptrace_build_status(&statusst, preset, clictx.pid, addr_current_ex) != 0) {
        pt_error("Status fetch failed");
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
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    /* Waiting connect */
    ret = select(sfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == -1) {
        pt_error("Waiting client connect failed");
        return -1;
    } else if (ret == 0) {
        pt_warning("Waiting client connect timeout");
        pt_comm_close(sfd, "/tmp/" PT_COMM_FILENAME);
        return 0;
    }

    /* client accept */
    cfd = pt_comm_accept(sfd);
    pt_info("client accepted fd:%d", cfd);
    pt_comm_close(sfd, "/tmp/" PT_COMM_FILENAME);

    /* send do_trace */
    pt_debug("send do_status");
    if (pt_comm_send_type(cfd, PT_MSG_DO_STATUS) == -1) {
        pt_error("Operation sending failed");
        pt_comm_close(cfd, NULL);
        return -1;
    }

    /* wait message */
    FD_ZERO(&read_fds);
    FD_SET(cfd, &read_fds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    ret = select(cfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == -1) {
        pt_error("Waiting client send failed");
        return -1;
    } else if (ret == 0) {
        pt_warning("Waiting client send timeout");
        pt_comm_close(cfd, NULL);
        return 0;
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
        pt_comm_close(cfd, NULL);
        return -1;
    }

    pt_comm_close(cfd, NULL);

    return 0;
}
