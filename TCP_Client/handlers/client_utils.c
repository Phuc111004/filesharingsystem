#include "client_utils.h"
#include <string.h>
#include <stdlib.h>

// Helper to parse IDs from server response (format: [ID: xxx] or [ReqID: xxx] or [InvID: xxx])
int parse_ids_from_response(char *response, int *ids, int max_count, const char *id_pattern) {
    int count = 0;
    char *line = strtok(response, "\n");
    
    while (line != NULL && count < max_count) {
        char *id_start = strstr(line, id_pattern);
        if (id_start) {
            id_start += strlen(id_pattern);
            char *id_end = strchr(id_start, ']');
            if (id_end) {
                char id_str[16];
                int len = id_end - id_start;
                if (len < 16) {
                    strncpy(id_str, id_start, len);
                    id_str[len] = '\0';
                    ids[count++] = atoi(id_str);
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    
    return count;
}
