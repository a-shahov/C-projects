#ifndef MY_MAIN_HEADER_H
#define MY_MAIN_HEADER_H

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"

#include "driver/gpio.h"
#include "driver/timer.h"

#include "esp_log.h"
#include "esp_err.h"

#include "wifi.h"
#include "udp_client.h"
#include "interconn_pipe.h"
#include "pins.h"
#include "ac_gree.h"
#include "custom_setup.h"

#endif