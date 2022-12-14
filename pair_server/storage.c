#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include "storage.h"
#include "config.h"
#include "mbedtls/sha256.h"

#define SHA256 (0)
#define TABLE_SIZE (4096)

#if defined(DEBUG) || defined(INFO)
static const char *TAG = "STORAGE";
#endif

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

#ifdef INFO
    printf("%s: called calculate_sha256\n", TAG);
#endif

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

int get_item(const char* key, char* out_value, size_t out_len) //Сообщать необходимую длину!
{
    int err = 0;
    struct cell *current; 
    unsigned long long hash;

#ifdef INFO
    printf("%s: called get_item\n", TAG);
#endif

    err = calculate_sha256(key, &hash);
    if (err != 0) {
#ifdef DEBUG
        printf("%s: calculate_sha256 is failed in get_item\n", TAG);
#endif
        errno = EINVAL;
        return err;
    }

    if (hash_table[hash].init_cell && strcmp(key, hash_table[hash].key) == 0) {
        if (out_len >= strlen(hash_table[hash].value)) {
            strcpy(out_value, hash_table[hash].value);
            return 0;
        } else {
            errno = ENOMEM;
#ifdef DEBUG
            printf("%s: out of memory for out buffer in get_item\n", TAG);
#endif
            return -1;
        }
    } else {
        current = hash_table[hash].next_cell;
    }

    for (;current != NULL; current = current->next_cell) {
        if (strcmp(key, current->key) == 0) {
            if (out_len >= strlen(current->value)) {
                strcpy(out_value, current->value);
                return 0;
            } else {
#ifdef DEBUG
                printf("%s: out of memory for out buffer in get_item\n", TAG);
#endif
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

#ifdef INFO
    printf("%s: called insert_item\n", TAG);
#endif

    err = calculate_sha256(key, &hash);
    if (err != 0) {
#ifdef DEBUG
        printf("%s: calculate_sha256 is failed in insert_item\n", TAG);
#endif
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
        current->value = (char*)malloc(strlen(value) + 1);
        strcpy(current->value, value);
    }
    
    return 0;
}

int delete_item(const char* key)
{
    int err = 0;
    struct cell *current, *prev, *next; 
    unsigned long long hash;

#ifdef INFO
    printf("%s: called delete_item\n", TAG);
#endif

    err = calculate_sha256(key, &hash);
    if (err != 0) {
#ifdef DEBUG
        printf("%s: calculate_sha256 is failed in delete_item\n", TAG);
#endif
        errno = EINVAL;
        return err;
    }

    if (hash_table[hash].init_cell && strcmp(key, hash_table[hash].key) == 0) {
        hash_table[hash].init_cell = 0; 
        return 0;
    } else {
        current = hash_table[hash].next_cell;
    }
    
    for (; current != NULL; current = current->next_cell) {
        if (strcmp(key, current->key) == 0) {
            prev = current->prev_cell;
            next = current->next_cell;
            free(current->value);
            free(current);
            prev->next_cell = next;
            if (next) {
                next->prev_cell = prev;
            }
            return 0;
        }
    }

    return -1;
}

int pop_item(const char* key, char* out_value, size_t out_len)
{
    int err = 0;

#ifdef INFO
    printf("%s: called pop_item\n", TAG);
#endif
    
    err = get_item(key, out_value, out_len);
    if (err != 0) {
        return -1;
    }
    
    err = delete_item(key);
    
    return err;
}

int main()
{
    char buff[1024], *key = "gggggg";
     
    insert_item(key, "hdfihiudfhgiudfhguihdfuighdfuign\ndfdjfglkjdflkg\nfdlkhglkdfglkfhdgkj\noidfgjoidfjgoidfjgoi");
    get_item(key, buff, 1024);
    printf("%s\n\n", buff);
    memset(buff, 0, 1024);
    pop_item(key, buff, 1024);
    printf("%s\n\n", buff);
    return 0;
}
