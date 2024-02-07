#include "udp_client.h"

static const char *const TAG = "UDP client";

ppipe_interconnection udp_cli_init()
{
    ppipe_interconnection udp_pipe = (ppipe_interconnection)malloc(sizeof(pipe_interconnection));
    udp_pipe->pipeEventGroup = xEventGroupCreate();
    udp_pipe->pipeQueueHandle = xQueueCreate(1, sizeof(udp_data));
    udp_pipe->pipeMessageBuffer = xMessageBufferCreate(SIZE_MESSAGE_BUFFER_UDP);
    udp_pipe->pipeMutex = xSemaphoreCreateMutex();

    return udp_pipe;
}

void udp_client(void *arg)
{
    EventGroupHandle_t UDPEventGroup = ((ppipe_interconnection)arg)->pipeEventGroup;
    QueueHandle_t UDPQueueHandle = ((ppipe_interconnection)arg)->pipeQueueHandle;
    MessageBufferHandle_t UDPMessageBuffer = ((ppipe_interconnection)arg)->pipeMessageBuffer;
 
    void *message_buffer_udp = NULL; 
    size_t buffer_length, sent_bytes;
    udp_data my_udp_data;
    EventBits_t bits;
    struct sockaddr_in dest_addr;
    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);
    struct timeval timeout = {.tv_sec = RESPONSE_WAITING_TIME};
    fd_set readfds;
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    int sock;

    dest_addr.sin_family = addr_family;

    while (true) {
        bits = xEventGroupWaitBits(UDPEventGroup, SEND_MESSAGE, pdTRUE, pdFALSE, portMAX_DELAY);
        sock = -1;
        if (bits & SEND_MESSAGE) {
            if (xQueueReceive(UDPQueueHandle, &my_udp_data, 0)) {
                if (!custom_allocate_memory_for_buffer(UDPMessageBuffer, &message_buffer_udp, &buffer_length)) {
                    xMessageBufferReset(UDPMessageBuffer);
                    ESP_LOGI(TAG, "no message for sending in SEND_MESSAGE or malloc failed");
                    goto failed;
                }
                xMessageBufferReceive(UDPMessageBuffer, message_buffer_udp, buffer_length, 0);
                dest_addr.sin_addr.s_addr = inet_addr((const char*)my_udp_data.ip);
                dest_addr.sin_port = htons(my_udp_data.port);

                sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
                if (sock == -1) {
                    ESP_LOGE(TAG, "Unable to create socket in SEND_MESSAGE: errno %d", errno);
                    goto failed;
                }
                ESP_LOGI(TAG, "Socket created, sending to %s:%d", my_udp_data.ip, my_udp_data.port);

                int err = sendto(sock, (const void*)message_buffer_udp, buffer_length, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending in SEND_MESSAGE: errno %d", errno);
                    goto failed;
                }

                if (my_udp_data.need_response) { 
                    FD_ZERO(&readfds);
                    FD_SET(sock, &readfds);
                    if (select(sock+1, &readfds, NULL, NULL, &timeout) > 0) {
                        if (FD_ISSET(sock, &readfds)) {
                            //нужен серьёзный рефакторинг! изучить вопрос с сокетами 
                            int len = recvfrom(sock, (void*)message_buffer_udp, sizeof(message_buffer_udp) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
                            if (len > 0 && len <= sizeof(message_buffer_udp) - 1) {
                                message_buffer_udp[len++] = '\0';
                                sent_bytes = xMessageBufferSend(UDPMessageBuffer, (void*)message_buffer_udp, len, 0);
                                if (sent_bytes != len) {
                                    ESP_LOGI(TAG, "Socket failed to pass data to MessageBuffer in SEND_MESSAGE");
                                    goto failed;  
                                }

                                memset((void*)&my_udp_data, 0, sizeof(my_udp_data));
                                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, &my_udp_data.ip, sizeof(my_udp_data.ip));
                                my_udp_data.port = ((struct sockaddr_in *)&source_addr)->sin_port;
                                if (!xQueueSendToBack(UDPQueueHandle, (void*)&my_udp_data, 0)) {
                                    ESP_LOGI(TAG, "xQueueSendToBack is failed");
                                    goto failed;
                                }
                                ESP_LOGI(TAG, "receive message from: %s:%d", my_udp_data.ip, my_udp_data.port);
                            } else {
                                ESP_LOGI(TAG, "recvfrom is failed in SEND_MESSAGE");
                                goto failed;
                            }
                        } else {
                            ESP_LOGI(TAG, "FD_ISSET failed verification in SEND_MESSAGE");
                            goto failed;
                        }
                    } else {
                        ESP_LOGI(TAG, "Socket response timeout in SEND_MESSAGE");
                        goto failed;
                    }
                }
                
                xEventGroupSetBits(UDPEventGroup, OPERATION_SUCCESS);
            } else {
                ESP_LOGI(TAG, "xQueueReceive(UDPQueueHandle) in SEND_MESSAGE is failed!");
                goto failed;
            }
        }
        goto end;
        failed:
        xMessageBufferReset(UDPMessageBuffer);
        xEventGroupSetBits(UDPEventGroup, OPERATION_FAILED);
        end:
        free(message_buffer_udp);
        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
        }
    }
}