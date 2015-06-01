#include <error.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define PTD(format, ...) fprintf(stderr, "[PTDebug:%d] " format "\n", __LINE__, ##__VA_ARGS__)

int main(int argc, char *argv[])
{
    int cfd;
    struct sockaddr_un saddr;

    /* create socket */
    cfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (cfd == -1) {
        perror("socket");
        return -1;
    }

    /* address */
    memset(&saddr, 0, sizeof(struct sockaddr_un));
    saddr.sun_family = AF_UNIX;
    strcpy(saddr.sun_path, "/tmp/monque.sock"); /* TODO pass socket path by ctrl-module */

    /* connect */
    PTD("connect");
    if (connect(cfd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        perror("connect");
        return -1;
    }
    PTD("connected");

    return 0;
}
