/**
 * Copyright 2016 Qihoo 360
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

#ifndef TRACE_COMM_H
#define TRACE_COMM_H

#define PT_COMM_FILENAME                "phptrace.sock"
#define PT_COMM_MAXRECV                 10
#define PT_COMM_BACKLOG                 128

#define PT_MSG_HEADER_SIZE              sizeof(pt_comm_message_t)
#define PT_MSG_SIZE(msg)                sizeof(pt_comm_message_t) + msg->len
#define PT_MSG_SIZE_MIN                 0
#define PT_MSG_SIZE_MAX                 1024 * 1024

/* Type codes of inner message */
#define PT_MSG_EMPTY                    0x00000000
#define PT_MSG_PEERDOWN                 0x00000001
#define PT_MSG_ERR_SOCK                 0x00000002
#define PT_MSG_ERR_BUF                  0x00000003
#define PT_MSG_INVALID                  0x00000004

/* Type codes of user defined message
 * Should >= 0x80000000 */
#define PT_MSG_RET                      0x80000000
#define PT_MSG_DO_BASE                  0x80010000
#define PT_MSG_DO_TRACE                 (PT_MSG_DO_BASE + 1)
#define PT_MSG_DO_STATUS                (PT_MSG_DO_BASE + 2)
#define PT_MSG_DO_PING                  (PT_MSG_DO_BASE + 4)

typedef struct {
    int32_t len;                /* message length */
    int32_t type;               /* message type */
    int32_t pid;                /* pid TODO to be moved out */
    char data[];                /* scaleable data */
} pt_comm_message_t;

/* function declation */
int pt_comm_connect(const char *addrstr);
int pt_comm_listen(const char *addrstr);
int pt_comm_accept(int fd);
int pt_comm_recv_msg(int fd, pt_comm_message_t **msg_ptr);
int pt_comm_build_msg(pt_comm_message_t **msg_ptr, size_t size, int type);
int pt_comm_send_type(int fd, int type);
int pt_comm_send_msg(int fd, pt_comm_message_t *msg);
int pt_comm_close(int fd);

#endif
