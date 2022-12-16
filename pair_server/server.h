#ifndef MY_SERVER_H
#define MY_SERVER_H

#define _BSD_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h> /* for NI_MAXHOST and NI_MAXSERV */

#define ADDRSTRLEN (NI_MAXHOST + NI_MAXSERV + 10)
#define PORT_NUM 10555
#define BACKLOG 10

#endif
