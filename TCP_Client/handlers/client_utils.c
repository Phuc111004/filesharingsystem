#include "client_utils.h"
#include "../../common/utils.h"
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

// Group selection helper
int get_selected_group_id(int sockfd) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_MY_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) return -1;
    buffer[n] = '\0';

    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
        return -1;
    } else {
        printf("\nYour Groups:\n%s\n", buffer);
    }

    int ids[100];
    int count = parse_ids_from_response(buffer, ids, 100, "[ID: ");
    
    if (count == 0) {
        printf("No groups found.\n");
        return -1;
    }

    int choice;
    printf("Select group number (1-%d): ", count);
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > count) {
        while(getchar() != '\n');
        printf("Invalid selection.\n");
        return -1;
    }
    while(getchar() != '\n');
    return ids[choice - 1];
}

// Folder selection helper
int get_selected_folder_id(int sockfd, int group_id, const char *prompt) {
    printf("\n%s\n", prompt ? prompt : "Select destination folder:");
    
    int current_folder_id = 0; // Start at root
    char current_folder_name[256] = "Root";
    
    while (1) {
        // List current folder contents
        char buffer[32768];
        snprintf(buffer, sizeof(buffer), "LIST_FILE %d %d\r\n", group_id, current_folder_id);
        send_all(sockfd, buffer, strlen(buffer));
        
        memset(buffer, 0, sizeof(buffer));
        int n = recv_response(sockfd, buffer, sizeof(buffer));
        if (n <= 0) return -1;
        
        printf("\nCurrent Folder: %s (ID: %d)\n", current_folder_name, current_folder_id);
        printf("Contents:\n%s\n", buffer);
        
        // Parse folders only
        int folder_ids[100];
        char folder_names[100][256];
        int folder_count = 0;
        
        char *saveptr;
        char *dup = strdup(buffer);
        char *line = strtok_r(dup, "\n", &saveptr);
        
        while (line && folder_count < 100) {
            if (strstr(line, "100 List:") || strlen(line) < 5) {
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }
            if (strstr(line, "203 End")) break;
            
            char type_str[16], name[256], id_str[16];
            char *id_ptr = strstr(line, "[ID: ");
            if (id_ptr && strncmp(line, "FOLDER", 6) == 0) {
                sscanf(id_ptr, "[ID: %15[^]]", id_str);
                folder_ids[folder_count] = atoi(id_str);
                
                sscanf(line, "%15s %255s", type_str, name);
                strncpy(folder_names[folder_count], name, 255);
                folder_names[folder_count][255] = '\0';
                folder_count++;
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
        free(dup);
        
        // Show options
        printf("\nOptions:\n");
        printf("0. Select this folder (ID: %d)\n", current_folder_id);
        if (current_folder_id != 0) {
            printf("1. Go back to parent\n");
        }
        for (int i = 0; i < folder_count; i++) {
            printf("%d. Enter folder: %s\n", i + 2, folder_names[i]);
        }
        
        int choice;
        printf("Choice: ");
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }
        while(getchar() != '\n');
        
        if (choice == 0) {
            return current_folder_id;
        } else if (choice == 1 && current_folder_id != 0) {
            // Go back to parent (simplified - go to root)
            current_folder_id = 0;
            strcpy(current_folder_name, "Root");
        } else if (choice >= 2 && choice < folder_count + 2) {
            // Enter selected folder
            int idx = choice - 2;
            current_folder_id = folder_ids[idx];
            strncpy(current_folder_name, folder_names[idx], 255);
            current_folder_name[255] = '\0';
        } else {
            printf("Invalid choice.\n");
        }
    }
}