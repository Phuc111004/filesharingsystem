#ifndef UTILS_H
#define UTILS_H


#include <stddef.h>


char* str_trim(char* s);
int hash_password(const char* password, char* out_hash, size_t out_size);


#endif // UTILS_H
