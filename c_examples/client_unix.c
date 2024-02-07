#include "unix_sock.h"

int main(int argc, char *argv[])
{
    struct sockaddr_un svaddr, claddr;
    int sfd;
    ssize_t numBytes;
    char *message = "message from client";

    sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sfd == -1) {
        fprintf(stderr, "socket is failed\n");
        exit(-1);
    }

    memset(&claddr, 0, sizeof(struct sockaddr_un));
    claddr.sun_family = AF_UNIX;
    snprintf(claddr.sun_path, sizeof(claddr.sun_path), "/tmp/cli.%ld", (long)getpid());
    
    if (bind(sfd, (struct sockaddr*)&claddr, sizeof(struct sockaddr_un)) == -1) {
        fprintf(stderr, "bind is failed\n");
        exit(-1);
    }

    memset(&svaddr, 0, sizeof(struct sockaddr_un));
    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, SV_SOCKET_PATH, sizeof(svaddr.sun_path) - 1);

    while (1) {
        getchar();
        
        if (sendto(sfd, message, strlen(message), 0, (struct sockaddr*)&svaddr, sizeof(struct sockaddr_un)) != strlen(message)) {
            fprintf(stderr, "sendto is failed\n");
            exit(-1);
        }
    }
}
