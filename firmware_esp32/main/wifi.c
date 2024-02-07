#include "wifi.h"

static const char *const TAG = "WiFi module";
static uint32_t number_attempts = 0;
static QueueHandle_t internalWiFiQueueHandle = NULL;
static char connected_ssid[MAX_SSID_LEN] = {0};
static char received_ip[IP4_MAX_SIZE] = {0};
static char broadcast_ip[IP4_MAX_SIZE] = {0};
static char network_ip[IP4_MAX_SIZE] = {0};
EventGroupHandle_t stateWiFiEventGroup = NULL;

static int write_in_wifi_conf(wifi_mode_t type, wifi_config_t *wifi_config, const void *ssid, const void *pass)
{
    /* 1 - success, 0 - failed */

    if (type == WIFI_MODE_STA) {
        memcpy((void*)(wifi_config->sta.ssid), ssid, MAX_SSID_LEN);
        memcpy((void*)(wifi_config->sta.password), pass, MAX_PASSPHRASE_LEN);
    } else if (type == WIFI_MODE_AP) {
        wifi_config->ap.ssid_len = strlen((const char*)ssid); 
        memcpy((void*)(wifi_config->ap.ssid), ssid, MAX_SSID_LEN);
        memcpy((void*)(wifi_config->ap.password), pass, MAX_PASSPHRASE_LEN);
    } else {
        return 0;
    }

    return 1;
}

static void on_sta_start(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{   
    EventBits_t state_bits = xEventGroupGetBits(stateWiFiEventGroup);

    ESP_LOGI(TAG, "sta start!");
    if (!(state_bits & STATE_START_FOR_SCAN)) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
    }
}

static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *evt = (ip_event_got_ip_t*)event_data;
    esp_ip4_addr_t network, broadcast;
    internal_wifi_data my_internal_wifi_data;
    EventBits_t state_bits = xEventGroupClearBits(stateWiFiEventGroup, STATE_INTERNAL_QUEUE);
    
    ip4_addr_get_network(&network, &evt->ip_info.ip, &evt->ip_info.netmask);
    broadcast.addr = (~evt->ip_info.netmask.addr | network.addr);

    sprintf(my_internal_wifi_data.ip, IPSTR, IP2STR(&evt->ip_info.ip));
    memcpy((void*)received_ip, (const void*)my_internal_wifi_data.ip, IP4_MAX_SIZE);
    sprintf(my_internal_wifi_data.network, IPSTR, IP2STR(&network));
    memcpy((void*)network_ip, (const void*)my_internal_wifi_data.network, IP4_MAX_SIZE);
    sprintf(my_internal_wifi_data.broadcast, IPSTR, IP2STR(&broadcast));
    memcpy((void*)broadcast_ip, (const void*)my_internal_wifi_data.broadcast, IP4_MAX_SIZE);

    if (state_bits & STATE_INTERNAL_QUEUE) {
        xQueueSendToBack(internalWiFiQueueHandle, &my_internal_wifi_data, 0);
    }

    ESP_LOGI(TAG, "broadcast = %s", my_internal_wifi_data.broadcast);
    ESP_LOGI(TAG, "network = %s", my_internal_wifi_data.network);
    ESP_LOGI(TAG, "got ip = %s", my_internal_wifi_data.ip);

    number_attempts = 0;
    xEventGroupClearBits(stateWiFiEventGroup, STATE_SEVERAL_TRIES);
    xEventGroupClearBits(stateWiFiEventGroup, STATE_LOST_IP);
    xEventGroupSetBits(stateWiFiEventGroup, STATE_GOT_IP);
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base, 
                               int32_t event_id, void *event_data)
{
    wifi_event_sta_disconnected_t *evt = (wifi_event_sta_disconnected_t*)event_data;
    EventBits_t state_bits = xEventGroupGetBits(stateWiFiEventGroup);

    ESP_LOGI(TAG, "disconnected from: %s, err = %d", (char*)(evt->ssid), evt->reason);
    memset((void*)connected_ssid, 0, MAX_SSID_LEN);

    if (state_bits & STATE_SEVERAL_TRIES) {
        if (number_attempts) {
            --number_attempts;
            ESP_LOGI(TAG, "connect to ap, there are still attempts left: %d", number_attempts);
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
        } else {
            xEventGroupClearBits(stateWiFiEventGroup, STATE_SEVERAL_TRIES);
            xEventGroupClearBits(stateWiFiEventGroup, STATE_GOT_IP);
            xEventGroupSetBits(stateWiFiEventGroup, STATE_LOST_IP);
            if (state_bits & STATE_CEASELESS_C) {
                xEventGroupClearBits(stateWiFiEventGroup, STATE_INTERNAL_QUEUE);
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
            }
        }
    } else if (state_bits & STATE_CEASELESS_C) {
        xEventGroupClearBits(stateWiFiEventGroup, STATE_GOT_IP);
        xEventGroupSetBits(stateWiFiEventGroup, STATE_LOST_IP);
        xEventGroupClearBits(stateWiFiEventGroup, STATE_INTERNAL_QUEUE);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
    } else if (!(state_bits & STATE_DEINITIALIZING)) {
        xEventGroupClearBits(stateWiFiEventGroup, STATE_GOT_IP);
        xEventGroupSetBits(stateWiFiEventGroup, STATE_LOST_IP);
    }
}

static void on_wifi_connect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t *evt = (wifi_event_sta_connected_t*)event_data;
    memcpy((void*)connected_ssid, (void*)evt->ssid, MAX_SSID_LEN);
    ESP_LOGI(TAG, "connected to: %s", (char*)(evt->ssid));
}

static void on_wifi_stop(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    EventBits_t state_bits = xEventGroupGetBits(stateWiFiEventGroup);

    xEventGroupClearBits(stateWiFiEventGroup, STATE_DEINITIALIZING);
    xEventGroupSetBits(stateWiFiEventGroup, STATE_STOPPED);
    ESP_LOGI(TAG, "wifi stop");
}

static void on_lost_ip(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    memset((void*)received_ip, 0, IP4_MAX_SIZE);
    memset((void*)broadcast_ip, 0, IP4_MAX_SIZE);
    memset((void*)network_ip, 0, IP4_MAX_SIZE);
    xEventGroupClearBits(stateWiFiEventGroup, STATE_GOT_IP);
    xEventGroupSetBits(stateWiFiEventGroup, STATE_LOST_IP);
    ESP_LOGI(TAG, "IP has been lost");
}

ppipe_interconnection wifi_init()
{
    esp_err_t ret;
    ppipe_interconnection wifi_pipe = NULL;
    
    if (!stateWiFiEventGroup) {
        stateWiFiEventGroup = xEventGroupCreate();
        if (!stateWiFiEventGroup) {
            goto end;
        }
    }
    
    if (!internalWiFiQueueHandle){
        internalWiFiQueueHandle = xQueueCreate(1, sizeof(internal_wifi_data));
        if (!internalWiFiQueueHandle) {
            goto end;
        }
    }

    ret = esp_netif_init();
    if (ret != ESP_OK) {
        goto end;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        goto end;
    }

    wifi_pipe = (ppipe_interconnection)malloc(sizeof(pipe_interconnection));
    if (!wifi_pipe) {
        goto end;
    }
    wifi_pipe->pipeEventGroup = xEventGroupCreate();
    if (!wifi_pipe->pipeEventGroup) {
        goto failed;
    }
    wifi_pipe->pipeQueueHandle = xQueueCreate(1, sizeof(wifi_data));
    if (!wifi_pipe->pipeQueueHandle) {
        goto failed;
    }
    wifi_pipe->pipeMessageBuffer = NULL;
    wifi_pipe->pipeMutex = xSemaphoreCreateMutex();
    if (!wifi_pipe->pipeMutex) {
        goto failed;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &on_lost_ip, NULL, NULL);
    if (ret != ESP_OK) {
        goto failed;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL, NULL);
    if (ret != ESP_OK) {
        goto failed;
    }
    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &on_wifi_stop, NULL, NULL);
    if (ret != ESP_OK) {
        goto failed;
    }
    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, NULL, NULL);
    if (ret != ESP_OK) {
        goto failed;
    }
    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, &on_wifi_stop, NULL, NULL);
    if (ret != ESP_OK) {
        goto failed;
    }
    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, &on_sta_start, NULL, NULL);
    if (ret != ESP_OK) {
        goto failed;
    }
    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL, NULL);
    if (ret != ESP_OK) {
        goto failed;
    }

    goto end;
failed:
    vSemaphoreDelete(wifi_pipe->pipeMutex); 
    vQueueDelete(wifi_pipe->pipeQueueHandle);
    vEventGroupDelete(wifi_pipe->pipeEventGroup);
    free(wifi_pipe);
end:
    xEventGroupSetBits(stateWiFiEventGroup, STATE_PRE_INIT);
    return wifi_pipe;
}

static void deinitialize_wifi_block(esp_netif_t *default_netif)
{
    esp_wifi_stop();
    esp_netif_destroy_default_wifi(default_netif);
    esp_wifi_deinit();
}

void wifi_connection(void *arg)
{
    EventGroupHandle_t WifiEventGroup = ((ppipe_interconnection)arg)->pipeEventGroup;
    QueueHandle_t WifiQueueHandle = ((ppipe_interconnection)arg)->pipeQueueHandle;
    
    esp_err_t err;
    esp_netif_t *default_netif = NULL;
    wifi_data my_wifi_data;
    internal_wifi_data my_internal_wifi_data;
    EventBits_t bits, state_bits;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t my_wifi_config = {};
    uint16_t max_number_aps = SCAN_LIST_SIZE;
    uint16_t ap_count;
    wifi_ap_record_t ap_info[SCAN_LIST_SIZE];

    while (true) {
        bits = xEventGroupWaitBits(WifiEventGroup, CONNECT_TO_AP | MODE_AP | DEINIT_WIFI | SCAN_AND_CONNECT_TO_AP, pdTRUE, pdFALSE, portMAX_DELAY);
        state_bits = xEventGroupGetBits(stateWiFiEventGroup);
        if (bits & CONNECT_TO_AP) {
            if (xQueueReceive(WifiQueueHandle, &my_wifi_data, 0)) {

                /* checking an existing connection */
                if (strcmp(my_wifi_data.ssid, connected_ssid) == 0) {
                    ESP_LOGI(TAG, "The connection has already been made!");
                    if (my_wifi_data.need_response) {
                        memset((void*)&my_wifi_data, 0, sizeof(my_wifi_data));
                        memcpy((void*)my_wifi_data.ip, (const void*)received_ip, IP4_MAX_SIZE);
                        memcpy((void*)my_wifi_data.network, (const void*)network_ip, IP4_MAX_SIZE);
                        memcpy((void*)my_wifi_data.broadcast, (const void*)broadcast_ip, IP4_MAX_SIZE);
                        xQueueReset(WifiQueueHandle);
                        if (!xQueueSendToBack(WifiQueueHandle, &my_wifi_data, 0)) {
                            ESP_LOGI(TAG, "failed send to queue");
                            goto failed;
                        }
                    }
                    xEventGroupSetBits(WifiEventGroup, OPERATION_SUCCESS);
                    goto end;
                }

                if (!(state_bits & STATE_PRE_INIT)) {
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_ALL_BITS);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_STA);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_DEINITIALIZING);
                    deinitialize_wifi_block(default_netif);
                    state_bits = xEventGroupWaitBits(stateWiFiEventGroup, STATE_STOPPED, pdTRUE, pdFALSE, MAX_WAITING_TIME_FOR_INTERNAL_EVENTS);
                    if (!(state_bits & STATE_STOPPED)) {
                        ESP_LOGI(TAG, "waiting STATE_STOPPED in CONNECT_TO_AP is failed!");
                        goto failed;
                    }
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_STOPPED);
                } else {
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_ALL_BITS);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_STA);
                }

                memset(&my_wifi_config, 0, sizeof(wifi_config_t));
                if (!write_in_wifi_conf(WIFI_MODE_STA, &my_wifi_config, (const void*)my_wifi_data.ssid, (const void*)my_wifi_data.password)) {
                    ESP_LOGI(TAG, "write_in_wifi_conf in CONNECT_TO_AC is failed!");
                    goto failed;
                }
                if (my_wifi_data.number_attempts) {
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_SEVERAL_TRIES);
                    number_attempts = my_wifi_data.number_attempts;
                }
                if (my_wifi_data.ceaseless_conn) {
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_CEASELESS_C);
                }
                my_wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

                default_netif = esp_netif_create_default_wifi_sta();
                err = esp_wifi_init(&cfg);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_init is failed in CONNECT_TO_AP");
                    goto failed;
                }

                err = esp_wifi_set_mode(WIFI_MODE_STA);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_set_mode is failed in CONNECT_TO_AP");
                    goto failed;
                }

                err = esp_wifi_set_config(WIFI_IF_STA, &my_wifi_config);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_set_config is failed in CONNECT_TO_AP");
                    goto failed;
                }

                if (my_wifi_data.need_response) {
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_INTERNAL_QUEUE); // See on_got_ip
                }

                err = esp_wifi_start();
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_start is failed in CONNECT_TO_AP");
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_INTERNAL_QUEUE);
                    goto failed;
                }

                state_bits = xEventGroupWaitBits(stateWiFiEventGroup, STATE_GOT_IP | STATE_LOST_IP, pdFALSE, pdFALSE, portMAX_DELAY);
                if (state_bits & STATE_GOT_IP) {
                    if (my_wifi_data.need_response) {
                        if (xQueueReceive(internalWiFiQueueHandle, &my_internal_wifi_data, 0)) {
                            memset((void*)&my_wifi_data, 0, sizeof(my_wifi_data));
                            memcpy((void*)my_wifi_data.ip, (const void*)my_internal_wifi_data.ip, IP4_MAX_SIZE);
                            memcpy((void*)my_wifi_data.network, (const void*)my_internal_wifi_data.network, IP4_MAX_SIZE);
                            memcpy((void*)my_wifi_data.broadcast, (const void*)my_internal_wifi_data.broadcast, IP4_MAX_SIZE);
                            xQueueReset(WifiQueueHandle);
                            if (!xQueueSendToBack(WifiQueueHandle, &my_wifi_data, 0)) {
                                ESP_LOGI(TAG, "failed send to queue");
                                goto failed;
                            }
                        } else {
                            ESP_LOGI(TAG, "xQueueReceive(internalWiFiQueueHandle) in CONNECT_TO_AP is failed!");
                            goto failed;
                        }
                    }
                    xEventGroupSetBits(WifiEventGroup, OPERATION_SUCCESS);
                } else {
                    ESP_LOGI(TAG, "state_bits & STATE_GOT_IP = false in CONNECT_TO_AP");
                    goto failed;
                }
            } else {
                ESP_LOGI(TAG, "xQueueReceive(WifiQueueHandle) in CONNECT_TO_AP is failed!");
                goto failed;
            }
        } else if (bits & MODE_AP) {
            if (xQueueReceive(WifiQueueHandle, &my_wifi_data, 0)) {
                if (!(state_bits & STATE_PRE_INIT)) {
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_ALL_BITS);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_SOFTAP);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_DEINITIALIZING);
                    deinitialize_wifi_block(default_netif);
                    state_bits = xEventGroupWaitBits(stateWiFiEventGroup, STATE_STOPPED, pdTRUE, pdFALSE, MAX_WAITING_TIME_FOR_INTERNAL_EVENTS);
                    if (!(state_bits & STATE_STOPPED)) {
                        ESP_LOGI(TAG, "waiting STATE_STOPPED in MODE_AP is failed!");
                        goto failed;
                    }
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_STOPPED);
                } else {
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_ALL_BITS);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_SOFTAP);
                }

                memset(&my_wifi_config, 0, sizeof(wifi_config_t));
                if (!write_in_wifi_conf(WIFI_MODE_AP, &my_wifi_config, (const void*)my_wifi_data.ssid, (const void*)my_wifi_data.password)) {
                    ESP_LOGI(TAG, "write_in_wifi_conf in MODE_AP is failed!");
                    goto failed;
                }
                my_wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
                my_wifi_config.ap.channel = WIFI_CHANNEL;
                my_wifi_config.ap.max_connection = MAX_STA_CONN;

                default_netif = esp_netif_create_default_wifi_ap();
                err = esp_wifi_init(&cfg);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_init is failed in MODE_AP");
                    goto failed;
                }

                err = esp_wifi_set_mode(WIFI_MODE_AP);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_set_mode is failed in MODE_AP");
                    goto failed;
                }
                
                err = esp_wifi_set_config(WIFI_IF_AP, &my_wifi_config);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_set_config is failed in MODE_AP");
                    goto failed;
                }

                err = esp_wifi_start();
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_start is failed in MODE_AP");
                    goto failed;
                }

                xEventGroupSetBits(WifiEventGroup, OPERATION_SUCCESS);
            } else {
                ESP_LOGI(TAG, "xQueueReceive(WifiQueueHandle) in MODE_AP is failed!");
                goto failed;
            }
        } else if (bits & DEINIT_WIFI) {
            if (!(state_bits & STATE_PRE_INIT)) {
                xEventGroupClearBits(stateWiFiEventGroup, STATE_ALL_BITS);
                xEventGroupSetBits(stateWiFiEventGroup, STATE_PRE_INIT);
                xEventGroupSetBits(stateWiFiEventGroup, STATE_DEINITIALIZING);
                deinitialize_wifi_block(default_netif);
                state_bits = xEventGroupWaitBits(stateWiFiEventGroup, STATE_STOPPED, pdTRUE, pdFALSE, MAX_WAITING_TIME_FOR_INTERNAL_EVENTS);
                if (!(state_bits & STATE_STOPPED)) {
                    ESP_LOGI(TAG, "waiting STATE_STOPPED in DEINIT_WIFI is failed!");
                    goto failed;
                }
                xEventGroupClearBits(stateWiFiEventGroup, STATE_STOPPED);
            }
            xEventGroupSetBits(WifiEventGroup, OPERATION_SUCCESS);
        } else if (bits & SCAN_AND_CONNECT_TO_AP) {
            if (xQueueReceive(WifiQueueHandle, &my_wifi_data, 0)) {
                if (!(state_bits & STATE_PRE_INIT)) {
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_ALL_BITS);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_STA);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_DEINITIALIZING);
                    deinitialize_wifi_block(default_netif);
                    state_bits = xEventGroupWaitBits(stateWiFiEventGroup, STATE_STOPPED, pdTRUE, pdFALSE, MAX_WAITING_TIME_FOR_INTERNAL_EVENTS);
                    if (!(state_bits & STATE_STOPPED)) {
                        ESP_LOGI(TAG, "waiting STATE_STOPPED in SCAN_AND_CONNECT_TO_AP is failed!");
                        goto failed;
                    }
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_STOPPED);
                } else {
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_ALL_BITS);
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_STA);
                }

                memset(&my_wifi_config, 0, sizeof(wifi_config_t));
                memset(ap_info, 0, sizeof(ap_info));
                if (my_wifi_data.number_attempts) {
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_SEVERAL_TRIES);
                    number_attempts = my_wifi_data.number_attempts;
                }
                if (my_wifi_data.ceaseless_conn) {
                    xEventGroupSetBits(stateWiFiEventGroup, STATE_CEASELESS_C);
                }

                default_netif = esp_netif_create_default_wifi_sta();
                err = esp_wifi_init(&cfg);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_init is failed in SCAN_AND_CONNECT_TO_AP");
                    goto failed;
                }

                err = esp_wifi_set_mode(WIFI_MODE_STA);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_set_mode is failed in SCAN_AND_CONNECT_TO_AP");
                    goto failed;
                }

                xEventGroupSetBits(stateWiFiEventGroup, STATE_START_FOR_SCAN); //See on_sta_start
                err = esp_wifi_start();
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_start is failed in SCAN_AND_CONNECT_TO_AP");
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_START_FOR_SCAN);
                    goto failed;
                }

                err = esp_wifi_scan_start(NULL, true);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_scan_start is failed in SCAN_AND_CONNECT_TO_AP");
                    goto failed;
                }

                err = esp_wifi_scan_get_ap_records(&max_number_aps, ap_info);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_scan_get_ap_records is failed in SCAN_AND_CONNECT_TO_AP");
                    goto failed;
                }

                err = esp_wifi_scan_get_ap_num(&ap_count);
                if (err != ESP_OK){
                    ESP_LOGI(TAG, "esp_wifi_scan_get_ap_num is failed in SCAN_AND_CONNECT_TO_AP");
                    goto failed;
                }
                ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
                
                bool found_sought_ap = false;
                for (int i = 0; (i < SCAN_LIST_SIZE) && (i < ap_count); ++i) {
                    int length_ssid = strlen((const char*)my_wifi_data.ssid);
                    if (strncmp((const char*)my_wifi_data.ssid, (const char*)ap_info[i].ssid, length_ssid) == 0) {
                        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
                        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
                        my_wifi_config.sta.threshold.authmode = ap_info[i].authmode;
                        my_wifi_config.sta.threshold.rssi = ap_info[i].rssi;
                        if (!write_in_wifi_conf(WIFI_MODE_STA, &my_wifi_config, (const void*)ap_info[i].ssid, (const void*)my_wifi_data.password)) {
                            ESP_LOGI(TAG, "write_in_wifi_conf in SCAN_AND_CONNECT_TO_AP is failed!");
                            goto failed;
                        }
                        found_sought_ap = true;
                        break;
                    }
                }

                if (found_sought_ap) {
                    xEventGroupClearBits(stateWiFiEventGroup, STATE_START_FOR_SCAN);
                    err = esp_wifi_stop();
                    if (err != ESP_OK){
                        ESP_LOGI(TAG, "esp_wifi_stop is failed in SCAN_AND_CONNECT_TO_AP");
                        goto failed;
                    }

                    err = esp_wifi_init(&cfg);
                    if (err != ESP_OK){
                        ESP_LOGI(TAG, "esp_wifi_init is failed in SCAN_AND_CONNECT_TO_AP");
                        goto failed;
                    }

                    err = esp_wifi_set_mode(WIFI_MODE_STA);
                    if (err != ESP_OK){
                        ESP_LOGI(TAG, "esp_wifi_set_mode is failed in SCAN_AND_CONNECT_TO_AP");
                        goto failed;
                    }

                    err = esp_wifi_set_config(WIFI_IF_STA, &my_wifi_config);
                    if (err != ESP_OK){
                        ESP_LOGI(TAG, "esp_wifi_set_config is failed in SCAN_AND_CONNECT_TO_AP");
                        goto failed;
                    }
                    if (my_wifi_data.need_response) {
                        xEventGroupSetBits(stateWiFiEventGroup, STATE_INTERNAL_QUEUE); // See on_got_ip
                    }

                    err = esp_wifi_start();
                    if (err != ESP_OK){
                        ESP_LOGI(TAG, "esp_wifi_start is failed in SCAN_AND_CONNECT_TO_AP");
                        xEventGroupClearBits(stateWiFiEventGroup, STATE_INTERNAL_QUEUE);
                        goto failed;
                    }
                    
                    state_bits = xEventGroupWaitBits(stateWiFiEventGroup, STATE_GOT_IP | STATE_LOST_IP, pdFALSE, pdFALSE, portMAX_DELAY);
                    if (state_bits & STATE_GOT_IP) {
                        if (my_wifi_data.need_response) {
                            if (xQueueReceive(internalWiFiQueueHandle, &my_internal_wifi_data, 0)) {
                                memset((void*)&my_wifi_data, 0, sizeof(my_wifi_data));
                                memcpy((void*)my_wifi_data.ip, (const void*)my_internal_wifi_data.ip, IP4_MAX_SIZE);
                                memcpy((void*)my_wifi_data.network, (const void*)my_internal_wifi_data.network, IP4_MAX_SIZE);
                                memcpy((void*)my_wifi_data.broadcast, (const void*)my_internal_wifi_data.broadcast, IP4_MAX_SIZE);
                                xQueueReset(WifiQueueHandle);
                                if (!xQueueSendToBack(WifiQueueHandle, &my_wifi_data, 0)) {
                                    ESP_LOGI(TAG, "xQueueSendToBack(WifiQueueHandle) in SCAN_AND_CONNECT_TO_AP is failed!");
                                    goto failed;
                                }
                            } else {
                                ESP_LOGI(TAG, "xQueueReceive(internalWiFiQueueHandle) in SCAN_AND_CONNECT_TO_AP is failed!");
                                goto failed;
                            }
                        }
                        xEventGroupSetBits(WifiEventGroup, OPERATION_SUCCESS);
                    } else {
                        ESP_LOGI(TAG, "state_bits & STATE_GOT_IP = false in SCAN_AND_CONNECT_TO_AP");
                        goto failed;
                    }
                } else {
                    ESP_LOGI(TAG, "found_sought_ap = false in SCAN_AND_CONNECT_TO_AP");
                    goto failed;
                }
            }
        }
        goto end;
        failed:
        xEventGroupSetBits(WifiEventGroup, OPERATION_FAILED);
        end:
        xQueueReset(internalWiFiQueueHandle);
        memset((void*)&my_wifi_data, 0, sizeof(my_wifi_data));
    }
}