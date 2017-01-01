/**
 * Copyright 2017 Yuchen Wang <phobosw@gmail.com>
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

#ifdef PT_PTRACE_ENABLE
#include "ptrace.h"
#endif

#define DEINIT_RETURN(code) ret = code; goto deinit;

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

    memset(info, 0, sizeof(struct php_info));

    /* PHP binary path */
    sprintf(path, "/proc/%d/exe", pid);
    len = readlink(path, buf, PATH_MAX);
    if (len == -1 || len >= PATH_MAX) {
        return -1;
    }
    info->binary = sdsnewlen(buf, len);

    if (info->binary[len - 3] == 'p' &&
        info->binary[len - 2] == 'h' &&
        info->binary[len - 1] == 'p') {
        pt_info("PHP binary: %s", info->binary);
    } else {
        pt_error("\"%s\" looks not like a PHP binary", info->binary);
        return -1;
    }

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

#ifdef PT_PTRACE_ENABLE
static int status_ptrace(void)
{
    int ret;
    void *addr_current_ex;
    struct php_info info;
    pt_status_t statusst;

    if (get_php_info(&info, clictx.pid) == -1) {
        pt_error("Fetching PHP info failed");
        return -1;
    }
    sdsfree(info.binary);

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
    if (addr_current_ex == (void *) -1) {
        pt_error("ptrace peek failed");
        DEINIT_RETURN(-1);
    } else if (addr_current_ex == NULL) {
        pt_error("Process %d not active", clictx.pid);
        DEINIT_RETURN(-1);
    }

    /* Fetch status */
    if (pt_ptrace_build_status(&statusst, preset, clictx.pid, addr_current_ex,
                               info.version) != 0) {
        pt_error("Status fetch failed");
        DEINIT_RETURN(-1);
    }
    pt_type_display_status(&statusst);
    pt_type_destroy_status(&statusst, 1);
    DEINIT_RETURN(0);

deinit:
    if (pt_ptrace_detach(clictx.pid) == -1) {
        pt_error("ptrace detach failed");
        return -1;
    }
    pt_info("ptrace detach success");

    return ret;
}
#else
static int status_ptrace(void)
{
    pt_error("ptrace supported only on Linux");

    return -1;
}
#endif

static int status_ext(void)
{
    int cfd, ret, msg_type;
    fd_set read_fds;
    struct timeval timeout;

    pt_comm_message_t *msg;
    pt_status_t statusst;

    cfd = 0;
    FD_ZERO(&read_fds);
    FD_SET(clictx.sfd, &read_fds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    pt_ctrl_set_active(&clictx.ctrl, clictx.pid);

    /* Waiting connect */
    ret = select(clictx.sfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == -1) {
        if (errno == EINTR) {
            DEINIT_RETURN(0);
        } else {
            pt_error("Waiting for client connect failed");
            DEINIT_RETURN(-1);
        }
    } else if (ret == 0) {
        pt_warning("Waiting for client connect timeout");
        DEINIT_RETURN(-2); /* timeout */
    }

    /* Client accept & send command */
    cfd = pt_comm_accept(clictx.sfd);
    pt_info("client accepted fd:%d", cfd);

    if (pt_comm_send_type(cfd, PT_MSG_DO_STATUS) == -1) {
        pt_error("Command sending failed");
        DEINIT_RETURN(-1);
    }

    FD_ZERO(&read_fds);
    FD_SET(cfd, &read_fds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    /* Waiting recv */
    ret = select(cfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == -1) {
        if (errno == EINTR) {
            DEINIT_RETURN(0);
        } else {
            pt_error("Waiting for client send failed");
            DEINIT_RETURN(-1);
        }
    } else if (ret == 0) {
        pt_warning("Waiting for client send timeout");
        DEINIT_RETURN(-2); /* timeout */
    }

    /* recv message */
    msg_type = pt_comm_recv_msg(cfd, &msg);
    pt_debug("recv message type: 0x%08x len: %d", msg_type, msg->len);
    if (msg_type == PT_MSG_STATUS) {
        pt_type_unpack_status(&statusst, msg->data);
        pt_type_display_status(&statusst);
        pt_type_destroy_status(&statusst, 1);
        DEINIT_RETURN(0);
    } else {
        pt_error("unknown message received with type 0x%08x", msg_type);
        DEINIT_RETURN(-1);
    }

deinit:
    if (cfd) {
        pt_comm_close(cfd, NULL);
    }
    pt_ctrl_set_inactive(&clictx.ctrl, clictx.pid);

    return ret;
}

int pt_status_main(void)
{
    int ret, try_ptrace;

    try_ptrace = 0;

    if (!clictx.ptrace) {
        ret = status_ext();

        if (ret == -1) {
            printf("Fetch status error\n");
            try_ptrace = 1;
        } else if (ret == -2) {
            printf("Operation timed out, no response received, "
                   "make sure PHP process is active and extension already installed.\n");
            try_ptrace = 1;
        }
    }

#ifdef PT_PTRACE_ENABLE
    if (try_ptrace) {
        printf("Retry with ptrace...\n");
    }
#else
    try_ptrace = 0;
#endif

    if (clictx.ptrace || try_ptrace) {
        ret = status_ptrace();
    }

    return ret;
}
