#include <stdio.h>
#include <stdlib.h>

#include "phptrace_comm.h"
#include "phptrace_ctrl.h"
#include "phptrace_type.h"

#define PT_CTRL_FILE "/tmp/phptrace.ctrl"
#define PT_COMM_FILE "/tmp/phptrace.comm"
#define CTRL(pid) ((int8_t *) (ctrl.ctrl_seg.shmaddr))[pid]
#define CTRL_SET_ACTIVE(pid) (CTRL(pid) |= PT_CTRL_ACTIVE)
#define CTRL_SET_INACTIVE(pid) (CTRL(pid) &= ~PT_CTRL_ACTIVE)

#define PTD(format, args...) fprintf(stderr, "[PTDebug:%d] " format "\n", __LINE__, ## args)

volatile int interrupted; /* flag of interrupt (CTRL + C) */

static void interrupt(int sig)
{
    interrupted = sig;
}

/* mini version of phptrace */
int main(int argc, char *argv[])
{
    int pid;
    char comm_file[256];
    phptrace_comm_message *msg;
    phptrace_comm_socket comm;
    phptrace_ctrl_t ctrl;
    phptrace_frame frame;

    /* arguments */
    if (argc < 2) {
        printf("Usage: %s <pid>\n", argv[0]);
        return 1;
    }
    pid = atoi(argv[1]);

    /* signal handle */
    signal(SIGINT, interrupt);

    /* ctrl open */
    phptrace_ctrl_open(&ctrl, PT_CTRL_FILE);
    if (ctrl.ctrl_seg.shmaddr == MAP_FAILED) {
        PTD("ctrl open %s failed", PT_CTRL_FILE);
        return 1;
    }
    PTD("ctrl open %s done", PT_CTRL_FILE);

    /* ctrl set */
    CTRL_SET_ACTIVE(pid);
    PTD("ctrl set active");

    /* comm open */
    while (1) {
        snprintf(comm_file, sizeof(comm_file), "%s.%d", PT_COMM_FILE, pid);
        if (phptrace_comm_sopen(&comm, comm_file, 1) == 0) {
            break;
        }
        PTD("comm open %s failed", comm_file);
        usleep(200 * 1000);
    }
    PTD("comm open %s done", comm_file);

    /* comm send op */
    phptrace_comm_swrite(&comm, 0x10000001, NULL, 0);
    PTD("comm write op");

    /* debug */
    while (0) {
        if (interrupted) {
            break;
        }
        fprintf(stderr, "0: %08x\n", ((phptrace_comm_message *) (comm.send_handler.head))->type);
        fprintf(stderr, "1: %08x\n", ((phptrace_comm_message *) (comm.send_handler.head + 1))->type);
        usleep(200 * 1000);
    }

    /* comm read */
    while (1) {
        if (interrupted) {
            break;
        }

        msg = phptrace_comm_sread(&comm);

        if (msg == NULL) {
            continue;
        }

        printf("offset: %5ld\t", (void *)msg - comm.recv_handler.head);
        switch (msg->type) {
            case 0x10000101: /* trace result */
                phptrace_type_unpack_frame(&frame, msg->data);
                if (frame.type == PT_FRAME_ENTRY) {
                    printf("%*s> %s\n", frame.level * 2, " ", frame.function);
                } else {
                    printf("%*s< %s\n", frame.level * 2, " ", frame.function);
                }
                break;
            default:
                PTD("msg unknown %d", msg->type);
                break;
        }
    }

    /* close */
    phptrace_comm_sclose(&comm, 0);
    PTD("comm close");
    CTRL_SET_INACTIVE(pid);
    PTD("ctrl set inactive");
    phptrace_ctrl_close(&ctrl);
    PTD("ctrl close");

    return 0;
}
