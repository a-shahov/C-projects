#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "mbedtls/sha256.h"

#include <stdio.h>

#define SHA256 (0)
#define TABLE_SIZE (4096)

static mbedtls_sha256_context ctx; 

struct cell {
    unsigned char init_cell;
    struct cell *next_cell;
    struct cell *prev_cell;
    char key[64];
    char* value;
};

static struct cell hash_table[TABLE_SIZE] = {0};

static int calculate_sha256(const unsigned char *key, unsigned long long *hash)
{
    int err = 0;
    unsigned char _hash[64];
    
    mbedtls_sha256_init(&ctx);
     
    err = mbedtls_sha256_starts(&ctx, SHA256);
    if (err != 0) {
        goto end;
    }
    
    err = mbedtls_sha256_update(&ctx, key, strlen(key));
    if (err != 0) {
        goto end;
    }
    
    err = mbedtls_sha256_finish(&ctx, _hash);
    if (err != 0) {
        goto end;
    }

end:
    *hash = *(unsigned long long*)_hash % TABLE_SIZE;
    mbedtls_sha256_free(&ctx);
    return err;
}

int get_item(const char* key, char* out_value, size_t out_len) 
{
    int err = 0;
    struct cell *current; 
    unsigned long long hash;
    
    out_value = NULL;
    
    err = calculate_sha256(key, &hash);
    if (err != 0) {
        errno = EINVAL;
        return err;
    }

    //Добавить обработку для элемента в массиве

    for (current = current->next_cell; current != NULL; current = current->next_cell) {
        if (strcmp(key, current->key) == 0) {
            if (out_len >= strlen(current->value)) {
                strcpy(out_value, current->value);
                return 0;
            } else {
                errno = ENOMEM;
                return -1;
            }
        }
    }
    
    return -1;
}

int insert_item(const char* key, const char* value)
{
    int err = 0;
    struct cell *current, *prev; 
    unsigned long long hash;
    
    err = calculate_sha256(key, &hash);
    if (err != 0) {
        errno = EINVAL;
        return err;
    }
    
    current = &hash_table[hash];
    
    if (current->init_cell == 0) {
        current->init_cell = 1;
        strcpy(current->key, key);
        free(current->value);
        current->value = (char*)malloc(strlen(value) + 1);
        strcpy(current->value, value);
    } else {
        while (current->next_cell) {
            current = current->next_cell;
        }
        current->next_cell = (struct cell*)calloc(1, sizeof(struct cell));
        prev = current;
        current = current->next_cell;
        
        current->init_cell = 1;
        current->prev_cell = prev;
        strcpy(current->key, key);
        current->value = (cahr*)malloc(strlen(value) + 1);
        strcpy(current->value, value);
    }
    
    return 0;
}

int delete_item(const char* key)
{
    return 0;
}

int pop_item(const char* key, char* out_value)
{
    return 0;
}

int main()
{
    return 0;
}
