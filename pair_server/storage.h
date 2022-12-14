#ifndef MY_STORAGE_H
#define MY_STORAGE_H
#include "config.h"

int get_item(const char* key, char* out_value, size_t out_len);
int insert_item(const char* key, const char* value);
int delete_item(const char* key);
int pop_item(const char* key, char* out_value, size_t out_len);

#endif
