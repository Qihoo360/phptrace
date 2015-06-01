#include <error.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define PTD(format, ...) fprintf(stderr, "[PTDebug:%d] " format "\n", __LINE__, ##__VA_ARGS__)

int main(int argc, char *argv[])
{
    int sfd, cfd;
    socklen_t calen;
    struct sockaddr_un saddr, caddr;

    /* create socket */
    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("socket");
        return -1;
    }

    /* address */
    memset(&saddr, 0, sizeof(struct sockaddr_un));
    saddr.sun_family = AF_UNIX;
    strcpy(saddr.sun_path, "/tmp/monque.sock"); /* TODO pass socket path by ctrl-module */

    /* bind */
    unlink(saddr.sun_path);
    PTD("bind addr %s", saddr.sun_path);
    if (bind(sfd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        perror("bind");
        return -1;
    }

    /* listen */
    PTD("listen");
    if (listen(sfd, 128) == -1) {
        perror("listen");
        return -1;
    }

    /* ctrl set active */
    PTD("sleep before accept");
    sleep(10);

    /* accept */
    PTD("accept");
    calen = sizeof(caddr);
    cfd = accept(sfd, (struct sockaddr *) &caddr, &calen);
    if (cfd == -1) {
        perror("accept");
        return -1;
    }
    PTD("client connected");

    return 0;
}
