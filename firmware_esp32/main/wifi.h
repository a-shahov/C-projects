#ifndef MY_WIFI_H
#define MY_WIFI_H

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/timer.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "interconn_pipe.h"

#define WIFI_CHANNEL 3
#define MAX_STA_CONN 2
#define SCAN_LIST_SIZE 20
#define IP4_MAX_SIZE 16
#define MAX_WAITING_TIME_FOR_INTERNAL_EVENTS (6000 / portTICK_PERIOD_MS)

// Bits for external events
typedef enum _external_wifi_events {
    CONNECT_TO_AP = BIT2,
    MODE_AP = BIT3,
    DEINIT_WIFI = BIT4,
    SCAN_AND_CONNECT_TO_AP = BIT5,
} external_wifi_events;

// Bits for internal events related to states of wifi module
typedef enum _internal_wifi_states {
    STATE_ALL_BITS = 0xFFFFFF,
    STATE_STA = BIT0, /* mode station */
    STATE_GOT_IP = BIT1, /* it is set when the station receives an ip */
    STATE_LOST_IP = BIT2, /* it is set every time you disconnect from the access point */
    STATE_CEASELESS_C = BIT3, /* it is set when you need to constantly maintain a connection with a given access point */
    STATE_SEVERAL_TRIES = BIT4, /* it is set when several connection attempts are needed */
    STATE_START_FOR_SCAN = BIT5, /* it is set when wifi is started for scanning */
    STATE_SOFTAP = BIT6, /* access point */
    STATE_GIVE_IP = BIT7, /* does not use */
    STATE_PRE_INIT = BIT8, /* the state when wifi does not have a defined state */
    STATE_DEINITIALIZING = BIT9, /* installed before deinitialization */
    STATE_STOPPED = BIT10, /* synchronization with the moment of deinitialization wifi */
    STATE_INTERNAL_QUEUE = BIT11, /* synchronization of the internal queue */
} internal_wifi_states;

typedef struct _wifi_data {
    union {
        struct {
            char ssid[MAX_SSID_LEN];
            char password[MAX_PASSPHRASE_LEN];
            union {
                struct { /* this part is used as a query in CONNECT_TO_AP and SCAN_AND_CONNECT_TO_AP */
                    bool ceaseless_conn;
                    bool need_response;
                    uint16_t number_attempts;
                };
                struct { /* this part is used as a query in MODE_AP */
                    int useless;
                };
            };
        };
        struct { /* this part is used as an answer */
            char ip[IP4_MAX_SIZE];
            char network[IP4_MAX_SIZE];
            char broadcast[IP4_MAX_SIZE];
        };
    };
} wifi_data, *pwifi_data;

typedef struct _internal_wifi_data {
    char ip[IP4_MAX_SIZE];
    char network[IP4_MAX_SIZE];
    char broadcast[IP4_MAX_SIZE];
} internal_wifi_data, *pinternal_wifi_data;

void wifi_connection(void *arg);
ppipe_interconnection wifi_init();

#endif