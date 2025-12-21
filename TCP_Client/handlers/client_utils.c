#include "client_utils.h"
#include <string.h>
#include <stdlib.h>

// Helper to parse IDs from server response (format: [ID: xxx] or [ReqID: xxx] or [InvID: xxx])
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

#include <sys/socket.h>
#include <stdio.h>

// Helper to remove trailing newlines
void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

// Updated recv_line as character-by-character reader
int recv_line(int sockfd, char *buf, size_t maxlen) {
    if (!buf || maxlen == 0) return -1;
    size_t i = 0;
    while (i < maxlen - 1) {
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n <= 0) break; // Error or Connection closed
        
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    trim_newline(buf); // Clean up \r\n immediately
    return (int)i;
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
        
        // If n==0, it could be empty line (after trim) or connection closed
        // recv_line returns length AFTER trim. 
        // Logic: if line was "\r\n", recv_line trims to "" and returns 0.
        // But if connection closed, recv returns 0 or -1. 
        // We need to differentiate? 
        // Actually, recv_line in previous step: returns i (length). 
        // If i=0, it means it hit \n immediately or EOF immediately.
        // We need a robust empty line check.
        
        // Check if line is empty (length 0)
        // Correct logic depends on how recv_line handles it.
        // If Server sends "\r\n", recv_line sees '\r' then next is '\n' (or just '\n').
        // It trims and returns 0.
        // So length 0 means empty line.
        
        // Fix: recv_line returns total bytes READ from socket (e.g., 2 for "\r\n").
        // We must check if the trimmed string is empty to detect the protocol terminator.
        size_t line_len = strlen(line_buf);
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

        // Use memcpy instead of strcpy, but only copy the actual string content (line_len)
        memcpy(buf + total_len, line_buf, line_len);
        total_len += line_len;
        buf[total_len] = '\n';
        total_len++;
        buf[total_len] = '\0';
    }
    return (int)total_len;
}
