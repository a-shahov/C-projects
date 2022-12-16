#ifndef MY_STORAGE_H
#define MY_STORAGE_H

int get_item(const char* key, unsigned int user_id, char* out_value, size_t out_len, size_t *write_bytes) ;
int insert_item(const char* key, const char* value, unsigned int user_id);
int delete_item(const char* key, unsigned int user_id);
int pop_item(const char* key, unsigned int user_id, char* out_value, size_t out_len, size_t *write_bytes);
unsigned long get_size(void);

#endif
