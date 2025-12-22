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

/**
 * @function trim_newline
 * Removes trailing newline characters from a string.
 *
 * @param s The string to trim
 */
 void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}


/**
 * @function recv_line
 * Receives a single line of text from the socket.
 *
 * @param sockfd Socket file descriptor
 * @param buf Buffer to store the received line
 * @param maxlen Maximum size of the buffer
 * @return int
 * - Number of bytes received on success
 * - -1 on failure or error
 */
int recv_line(int sockfd, char *buf, size_t maxlen) {
    if (!buf || maxlen == 0) return -1;
    size_t i = 0;
    while (i < maxlen - 1) {
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n <= 0) break;
        
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    trim_newline(buf); // Clean up \r\n
    return (int)i;
}
