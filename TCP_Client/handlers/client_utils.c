#include "client_utils.h"
#include "../../common/utils.h"
#include "../../common/file_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>

// Helper to parse IDs from server response
int parse_ids_from_response(char *response, int *ids, int max_count, const char *id_pattern) {
    int count = 0;
    char *current = response;
    
    while (current && count < max_count) {
        char *id_start = strstr(current, id_pattern);
        if (!id_start) break;
        
        id_start += strlen(id_pattern);
        char *id_end = strchr(id_start, ']');
        if (id_end) {
            char id_str[16];
            int len = id_end - id_start;
            if (len < 16 && len > 0) {
                strncpy(id_str, id_start, len);
                id_str[len] = '\0';
                ids[count++] = atoi(id_str);
            }
            current = id_end + 1;
        } else {
            break;
        }
    }
    return count;
}

// Helper to remove trailing newlines
void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

/**
 * Nhận phản hồi từ server (dạng block/chunk).
 */
int recv_response(int sockfd, char *buf, size_t maxlen) {
    if (!buf || maxlen == 0) return -1;
    // Nhận dữ liệu một lần
    ssize_t n = recv(sockfd, buf, maxlen - 1, 0);
    if (n <= 0) return -1; 
    
    buf[n] = '\0'; 
    return (int)n;
}

int is_error_response(const char *response) {
    if (!response || strlen(response) < 3) return 0;
    // Check if starts with 4xx or 5xx
    if ((response[0] == '4' || response[0] == '5') && 
        (response[1] >= '0' && response[1] <= '9') &&
        (response[2] >= '0' && response[2] <= '9')) {
        return 1;
    }
    return 0;
}
