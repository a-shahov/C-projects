#ifndef MY_PIPES_H
#define MY_PIPES_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"
#include "freertos/semphr.h"

/* Bits for common pipe events */ 
typedef enum _common_pipe_events {
    OPERATION_SUCCESS = BIT0,
    OPERATION_FAILED = BIT1,
} common_pipe_events;

#define MAX_TIME_FOR_TAKE_MUTEX (10000 / portTICK_PERIOD_MS) 

typedef struct _pipe_interconnection {
    EventGroupHandle_t pipeEventGroup;
    QueueHandle_t pipeQueueHandle;
    MessageBufferHandle_t pipeMessageBuffer;
    SemaphoreHandle_t pipeMutex;
} pipe_interconnection, *ppipe_interconnection;

int custom_allocate_memory_for_buffer(MessageBufferHandle_t message_buffer, void **dest, size_t *out_length);

#endif