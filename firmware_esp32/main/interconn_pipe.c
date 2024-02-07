#include "interconn_pipe.h"

int custom_allocate_memory_for_buffer(MessageBufferHandle_t message_buffer, void **dest, size_t *out_length)
{
    int err; /* 1 - success, 0 - failed */
    size_t received_bytes;

    free(*dest);
    
    received_bytes = xMessageBufferReceive(message_buffer, NULL, 0, 0);
    *out_length = received_bytes;
    if (received_bytes == 0) {
        *dest = NULL;
        return 0;
    }

    *dest = malloc(received_bytes);
    if (!(*dest)) {
        *out_length = 0;
        return 0;
    }

    return 1;
}