#ifndef MY_UNIX_SOCK_2
#define MY_UNIX_SOCK_2

#include <sys/un.h>
#include <sys/socket.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#define BUF_SIZE 16
#define BACKLOG 5

#define SV_SOCKET_PATH "/tmp/tmp_socket"

#endif
