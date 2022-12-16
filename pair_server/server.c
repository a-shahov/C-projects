#include "server.h"

int main(int argv, char *argc[])
{
    struct sockaddr_in svaddr, claddr;
    int sfd, cfd;
    socklen_t len;
    char addr_str[ADDRSTRLEN];
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
#ifdef DEBUG
        printf("socket is failed\n");
#endif
        exit(-1);
    }
    
    memset(&svaddr, 0, sizeof(struct sockaddr_in));
    svaddr.sin_family = AF_INET;
    svaddr.sin_port = htons(PORT_NUM);
    svaddr.sin_addr = inaddr_any;
    
    if (bind(sfd, (struct sockaddr *)&svaddr, sizeof(struct sockaddr_in)) == -1) {
#ifdef DEBUG
        printf("bind is failed\n");
#endif
        exit(-1);
    }
    
    if (listen(sfd, BACKLOG)) {
#ifdef DEBUG
        printf("listen is failed\n");
#endif
        exit(-1);
    }
    
    while (true) {
        cfd = accept(sfd, (struct sockaddr *)&claddr, &len);
        if (cfd == -1) {
#ifdef DEBUG
            printf("accept is failed\n");
#endif
            exit(-1);
        }
        
        if (getnameinfo((struct sockaddr *)&claddr, len, host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
            snprintf(addr_str, ADDRSTRLEN, "(%s, %s)", host, service);
        } else {
            snprintf(addr_str, ADDRSTRLEN, "(?UNKNOWN?)");
        }
        printf("Connection from: %s", addr_str);
        
    }
    
}
