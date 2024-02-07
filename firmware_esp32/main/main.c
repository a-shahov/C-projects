#include "main.h"

static const char *const TAG = "Main";

void app_main(void)
{  
    init_nvs();

    ppipe_interconnection pipes_for_ac[3] = {ac_cli_init(), wifi_init(), udp_cli_init()};
    EventBits_t bits;
    ac_data my_ac_data = {.ssid = "idesk2.4", .password = "57883830"};
    wifi_data my_wifi_data = {.ssid = "idesk2.4", .password = "57883830", .ceaseless_conn = true, .number_attempts = 6, .need_response = true};
    udp_data my_udp_data;
    char rx_buffer[256];
    size_t received_bytes;

    xTaskCreate(wifi_connection, "wifi connection", 8192, pipes_for_ac[1], 5, NULL);
    xTaskCreate(udp_client, "UDP client", 8192, pipes_for_ac[2], 5, NULL);
    xTaskCreate(ac_client, "AC client", 8192, pipes_for_ac, 5, NULL);
    
    xSemaphoreTake(pipes_for_ac[1]->pipeMutex, MAX_TIME_FOR_TAKE_MUTEX);
    xQueueReset(pipes_for_ac[1]->pipeQueueHandle);
    xQueueSendToBack(pipes_for_ac[1]->pipeQueueHandle, (void*)&my_wifi_data, 0);
    xEventGroupSetBits(pipes_for_ac[1]->pipeEventGroup, CONNECT_TO_AP);
    bits = xEventGroupWaitBits(pipes_for_ac[1]->pipeEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY);

    ESP_LOGI(TAG, "reconnecting...");

    xQueueReset(pipes_for_ac[1]->pipeQueueHandle);
    xQueueSendToBack(pipes_for_ac[1]->pipeQueueHandle, (void*)&my_wifi_data, 0);
    xEventGroupSetBits(pipes_for_ac[1]->pipeEventGroup, CONNECT_TO_AP);
    bits = xEventGroupWaitBits(pipes_for_ac[1]->pipeEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY);
    if (bits & OPERATION_SUCCESS) {
        xQueueReceive(pipes_for_ac[1]->pipeQueueHandle, (void*)&my_wifi_data, 0);
        ESP_LOGI(TAG, "my ip = %s", my_wifi_data.ip);
    }
    xSemaphoreGive(pipes_for_ac[1]->pipeMutex);

    while (true) {
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        printf("memory = %d\n", xPortGetFreeHeapSize());
    }
}