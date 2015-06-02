#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "trace_comm.h"
#include "trace_ctrl.h"

#define PTL(format, ...) fprintf(stderr, "[PTLog:%d] " format "\n", __LINE__, ##__VA_ARGS__)
#if 0
#define PTD(format, ...) fprintf(stderr, "[PTDebug:%d] " format "\n", __LINE__, ##__VA_ARGS__)
#else
#define PTD(format, ...)
#endif

void pt_frame_display(pt_frame_t *frame, int indent, const char *format, ...)
{
    int i, has_bracket = 1;
    va_list ap;

    /* indent */
    if (indent) {
        printf("%*s", (frame->level - 1) * 4, "");
    }

    /* format */
    if (format) {
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }

    /* frame */
    if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_NORMAL ||
            frame->functype & PT_FUNC_TYPES & PT_FUNC_INCLUDES) {
        printf("%s(", frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_MEMBER) {
        printf("%s->%s(", frame->class, frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_STATIC) {
        printf("%s::%s(", frame->class, frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_EVAL) {
        printf("%s", frame->function);
        has_bracket = 0;
    } else {
        printf("unknown");
        has_bracket = 0;
    }

    /* arguments */
    if (frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            printf("%s", frame->args[i]);
            if (i < frame->arg_count - 1) {
                printf(", ");
            }
        }
    }
    if (has_bracket) {
        printf(")");
    }

    /* return value */
    if (frame->type == PT_FRAME_EXIT && frame->retval) {
        printf(" = %s", frame->retval);
    }

    /* TODO output relative filepath */
    printf(" called at [%s:%d]", frame->filename, frame->lineno);
    if (frame->type == PT_FRAME_EXIT) {
        printf(" wt: %.3f ct: %.3f\n",
                ((frame->exit.wall_time - frame->entry.wall_time) / 1000000.0),
                ((frame->exit.cpu_time - frame->entry.cpu_time) / 1000000.0));
    } else {
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    int i, sfd, cfd;
    pid_t pid;
    socklen_t calen;
    struct sockaddr_un saddr, caddr;
    pt_ctrl_t ctrlst;

    /* argument */
    if (argc < 2) {
        PTL("usage: %s <pid>...", argv[0]);
        return 0;
    }

    /* create socket */
    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("socket");
        return -1;
    }

    /* address */
    memset(&saddr, 0, sizeof(struct sockaddr_un));
    saddr.sun_family = AF_UNIX;
    strcpy(saddr.sun_path, "/tmp/phptrace.sock");

    /* bind */
    unlink(saddr.sun_path);
    PTL("bind addr %s", saddr.sun_path);
    if (bind(sfd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        perror("bind");
        return -1;
    }

    /* listen */
    PTL("listen");
    if (listen(sfd, 128) == -1) {
        perror("listen");
        return -1;
    }

    /* ctrl set active */
    if (pt_ctrl_open(&ctrlst, "/tmp/phptrace.ctrl") == -1) {
        PTL("ctrl open failed");
        return -1;
    }
    if (strcmp("all", argv[1]) == 0) {
        pt_ctrl_set_all(&ctrlst, PT_CTRL_ACTIVE);
    } else {
        for (i = 1; i < argc; i++) {
            pid = atoi(argv[i]);
            pt_ctrl_set_active(&ctrlst, pid);
            PTL("ctrl set active %d", pid);
        }
    }

    /* select */
    int maxfd;
    fd_set cfds, rfds;
    FD_ZERO(&cfds);
    maxfd = sfd + 1;

    /* trace message */
    size_t msg_buflen;
    ssize_t rlen;
    void *msg_buf;
    pt_comm_message_t *msg;
    pt_frame_t framest;

    int sock_rcvbuf = 1024 * 1024;
    socklen_t optlen = sizeof(sock_rcvbuf);

    msg_buflen = sizeof(pt_comm_message_t);
    msg = msg_buf = malloc(msg_buflen);

    for (;;) {
        memcpy(&rfds, &cfds, sizeof(fd_set)); /* TODO better way */
        FD_SET(sfd, &rfds);
        select(maxfd + 1, &rfds, NULL, NULL, NULL);

        /* accept */
        if (FD_ISSET(sfd, &rfds)) {
            calen = sizeof(caddr);
            cfd = accept(sfd, (struct sockaddr *) &caddr, &calen);
            if (cfd == -1) {
                perror("accept");
                return -1;
            }
            PTL("accept %d", cfd);

            /* add to cfds */
            FD_SET(cfd, &cfds);
            if (cfd > maxfd) {
                maxfd = cfd;
            }

            /* send do_trace */
            msg->seq = 0;
            msg->type = PT_MSG_DO_TRACE;
            msg->len = 0;
            rlen = send(cfd, msg, sizeof(pt_comm_message_t), 0);
            PTL("send do_trace, return: %ld", rlen);

            /* set socket opt */
            /* http://man7.org/linux/man-pages/man7/socket.7.html */
            /* http://www.cyberciti.biz/faq/linux-tcp-tuning/ */
            if (getsockopt(cfd, SOL_SOCKET, SO_RCVBUF, &sock_rcvbuf, &optlen) == -1) {
                perror("getsockopt");
            }
            PTL("rcvbuf before: %d %u", sock_rcvbuf, optlen);

            if (setsockopt(cfd, SOL_SOCKET, SO_RCVBUF, &sock_rcvbuf, optlen) == -1) {
                perror("setsockopt");
            }

            if (getsockopt(cfd, SOL_SOCKET, SO_RCVBUF, &sock_rcvbuf, &optlen) == -1) {
                perror("getsockopt");
            }
            PTL("rcvbuf after: %d %u", sock_rcvbuf, optlen);

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
                PTD("recv header return: %ld", rlen);
                if (rlen == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    break;
                } else if (rlen <= 0) {
                    FD_CLR(cfd, &cfds);
                    break;
                }
                PTD("msg->type: %x len: %d", msg->type, msg->len);
                if (msg->type != PT_MSG_RET || msg->len <= 0) {
                    continue;
                }

                /* prepare body space */
                if (msg_buflen < sizeof(pt_comm_message_t) + msg->len) {
                    /* optimize dynamic alloc */
                    msg_buflen = sizeof(pt_comm_message_t) + msg->len;
                    msg_buf = realloc(msg_buf, msg_buflen);
                    msg = msg_buf; /* address may be changed after realloc */
                    PTL("alloc %ld bytes for new buffer", msg_buflen);
                }

                /* body */
                rlen = recv(cfd, msg_buf + sizeof(pt_comm_message_t), msg->len, 0);
                PTD("recv body return: %ld", rlen);
                if (rlen <= 0) {
                    FD_CLR(cfd, &cfds);
                    break;
                }

                /* unpack */
                pt_type_unpack_frame(&framest, msg->data);
                printf("[pid %5u]", msg->seq);
                if (framest.type == PT_FRAME_ENTRY) {
                    pt_frame_display(&framest, 1, "> ");
                } else {
                    pt_frame_display(&framest, 1, "< ");
                }
            }
        }
    }

    return 0;
}
