#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include <stddef.h>

int parse_ids_from_response(char *response, int *ids, int max_count, const char *id_pattern);

// Đổi tên recv_line -> recv_response để phản ánh đúng bản chất (nhận 1 cục)
int recv_response(int sockfd, char *buf, size_t maxlen);
int recv_multiline(int sockfd, char *buf, size_t maxlen);
int is_error_response(const char *response);
void trim_newline(char *s);

#endif