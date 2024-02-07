#include "unix_sock.h"

int main(int argc, char *argv[])
{
    struct sockaddr_un svaddr, claddr;
    int sfd;
    ssize_t numBytes;
    socklen_t len;
    char buf[BUF_SIZE];
    
    sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sfd == -1) {
        fprintf(stderr, "socket is failed\n");
        exit(-1);
    }
    
    if (remove(SV_SOCKET_PATH) == -1 && errno != ENOENT) {
        fprintf(stderr, "remove is failed\n");
        exit(-1);
    }
    
    memset(&svaddr, 0, sizeof(struct sockaddr_un));
    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, SV_SOCKET_PATH, sizeof(svaddr.sun_path) - 1);
    
    if (bind(sfd, (const struct sockaddr *)&svaddr, sizeof(struct sockaddr_un)) == -1) {
        fprintf(stderr, "bind is failed\n");
        exit(-1);
    }
    
    while (1) {
        len = sizeof(struct sockaddr_un);
        numBytes = recvfrom(sfd, buf, BUF_SIZE, 0, (struct sockaddr *)&claddr, &len);
        
        if (numBytes == -1) {
            fprintf(stderr, "recvfrom is failed\n");
            exit(-1);
        }
        
        printf("received: %s  from %s\n", buf, claddr.sun_path);
        
        sleep(3);
    }
}
