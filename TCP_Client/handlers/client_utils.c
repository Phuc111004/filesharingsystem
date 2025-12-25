#include "client_utils.h"
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
 * Nhận một dòng từ socket, kết thúc bằng \n.
 * Hỗ trợ nhận từng byte để đảm bảo không đọc quá 1 dòng.
 */
int recv_line(int sockfd, char *buf, size_t maxlen) {
    if (!buf || maxlen == 0) return -1;
    size_t total = 0;
    while (total < maxlen - 1) {
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n <= 0) {
            if (total == 0) return -1;
            break;
        }
        if (c == '\n') break;
        if (c != '\r') buf[total++] = c;
    }
    buf[total] = '\0';
    return (int)total;
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

// Receives lines until an empty line is received, accumulating them into buf.
// Returns total bytes received or -1 on error.
int recv_multiline(int sockfd, char *buf, size_t maxlen) {
    if (!buf || maxlen == 0) return -1;
    
    size_t total_len = 0;
    buf[0] = '\0';
    
    char line_buf[4096];
    while (1) {
        // Peek at the buffer size safety
        if (total_len >= maxlen - 1) {
            printf("Buffer overflow in recv_multiline\n");
            break; 
        }

        int n = recv_line(sockfd, line_buf, sizeof(line_buf));
        if (n < 0) return -1; // Error
        
        size_t line_len = strlen(line_buf);

        if (total_len == 0 && line_len > 0) {
            if ((line_buf[0] == '4' || line_buf[0] == '5') && line_len >= 3) {
                strncpy(buf, line_buf, maxlen - 1);
                buf[maxlen - 1] = '\0';
                return (int)strlen(buf);
            }
        }

        if (line_len == 0) {
           break; // End of transmission (Empty line received)
        }
        
        if (n == 0) {
           break; // Connection closed
        }
        
        // Check for space
        if (total_len + line_len + 2 > maxlen) {
             printf("Buffer full, truncating...\n");
             break;
        }

        memcpy(buf + total_len, line_buf, line_len);
        total_len += line_len;
        buf[total_len] = '\n';
        total_len++;
        buf[total_len] = '\0';
    }
    return (int)total_len;
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
