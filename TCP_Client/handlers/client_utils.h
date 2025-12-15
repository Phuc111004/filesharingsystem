#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

int parse_ids_from_response(char *response, int *ids, int max_count, const char *id_pattern);

#endif // CLIENT_UTILS_H
