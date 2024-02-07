#ifndef MY_UDPCLIENT_H
#define MY_UDPCLIENT_H

#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/message_buffer.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "interconn_pipe.h"
#include "wifi.h"

// Bits for external events
typedef enum _external_udp_events {
    SEND_MESSAGE = BIT2,
} external_udp_events;

#define SIZE_MESSAGE_BUFFER_UDP 1024
#define RESPONSE_WAITING_TIME 2

typedef struct _udp_data {
    char ip[IP4_MAX_SIZE];
    uint16_t port;
    union {
        struct {
            bool need_response;
        };
        struct {
            int useless;
        };
    };
} udp_data, *pudp_data;

void udp_client(void *arg);
ppipe_interconnection udp_cli_init();

#endif