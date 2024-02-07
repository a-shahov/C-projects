#ifndef MY_AC_GREE_H
#define MY_AC_GREE_H

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/message_buffer.h"

#include "esp_log.h"
#include "esp_err.h"

#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

#include "cJSON.h"

#include "interconn_pipe.h"
#include "wifi.h"
#include "udp_client.h"

#define SIZE_MESSAGE_BUFFER_AC 256
#define NUMBER_AC_PARAMETERS 16 /* see ac_parameters below */

/* Bits for external events */
typedef enum _external_ac_events {
    BINDING_WITH_AC = BIT2, /* when this event is called, the message buffer must be sent to the mac */ 
    SEND_CONTROL_TO_AC = BIT3, /* when this event is called, the message buffer must be sent to the mac and set of parameters */ 
    SEND_WLAN_TO_AC = BIT4, /* as a response, it sends to the messagebuffer mac */
    SCAN_AC = BIT5, /* this event is used to find out the ip addresses of air conditioners on the network and add new ac */
} external_ac_events;

/* Parameters AC */
typedef enum _ac_parameters {
    Pow = 0,
    Mod = 1,
    TemUn = 2,
    SetTem = 3, 
    WdSpd = 4,
    Air = 5,
    Blo = 6,
    Health = 7, 
    SwhSlp = 8,
    Lig = 9, 
    SwingLfRig = 10,
    SwUpDn = 11,
    Quiet = 12,
    Tur = 13,
    StHt = 14,
    SvSt = 15,
} ac_parameters;

typedef struct _control_ac_pair {
    ac_parameters parameter;
    int value;
} control_ac_pair, *pcontrol_ac_pair;

typedef struct _ac_data {
    union {
        struct { /* this part is used as a query in SEND_WLAN_TO_AC and BINDING_WITH_AC and SCAN_AC */
            char ssid[MAX_SSID_LEN];
            char password[MAX_PASSPHRASE_LEN];
        };
        struct { /* this part is used as a answer in BINDING_WITH_AC */ 
            bool valid_wifi;
            bool valid_mac;
        };
    };
} ac_data, *pac_data;

typedef struct _ac_instance {
    struct _ac_instance *next;
    struct _ac_instance *prev;
    char ip[IP4_MAX_SIZE];
    char *mac;
    char *aes_key;
} ac_instance, *pac_instance;

ppipe_interconnection ac_cli_init();
void ac_client(void *arg);

#endif