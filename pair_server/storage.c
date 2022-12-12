#include <stdlib.h>
#include <string.h>
#include "mbedtls/sha256.h"

#include <stdio.h>

#define SHA256 (0)
#define TABLE_SIZE (4096)

static mbedtls_sha256_context ctx; 

struct cell {
    struct cell *next_cell;
    char key[64];
    char* value;
};

static struct cell hash_table[TABLE_SIZE];

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

int get_item(const char* key, char* out_value) 
{
    return 0;
}

int insert_item(const char* key, const char* value)
{
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
