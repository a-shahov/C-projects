#include "unix_sock2.h"

int main(int argc, char *argv[])
{
    struct sockaddr_un svaddr;
    int sfd;
    ssize_t numBytes;
    char *message = "message from client";

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1) {
        fprintf(stderr, "socket is failed\n");
        exit(-1);
    }
    
    memset(&svaddr, 0, sizeof(struct sockaddr_un));
    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, SV_SOCKET_PATH, sizeof(svaddr.sun_path) - 1);
    
    if (connect(sfd, (struct sockaddr*)&svaddr, sizeof(struct sockaddr_un)) == -1) {
        fprintf(stderr, "connect is failed\n");
        exit(-1);
    }

    while (1) {
        getchar();
        
        if (write(sfd, message, strlen(message)) != strlen(message)) {
            fprintf(stderr, "write is failed\n");
            exit(-1);
        }
    }
}
