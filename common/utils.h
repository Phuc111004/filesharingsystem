#ifndef UTILS_H
#define UTILS_H


#include <stddef.h>


char* str_trim(char* s);
int utils_hash_password(const char* password, char* out_hash, size_t out_size);
void mkdir_p(const char *path);
int split_args(char *line, char **args, int max_args);


#endif // UTILS_H
