#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include <stddef.h>

int parse_ids_from_response(char *response, int *ids, int max_count, const char *id_pattern);
int recv_line(int sockfd, char *buf, size_t maxlen);
int recv_multiline(int sockfd, char *buf, size_t maxlen);
void trim_newline(char *s);

#endif // CLIENT_UTILS_H
