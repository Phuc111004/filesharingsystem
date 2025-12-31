#include "group_helper.h"
#include "../../common/file_utils.h"
#include "../../common/utils.h"
#include "../handlers/client_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

int get_selected_admin_group(int sockfd) {
    char buffer[32768];
    
    // [SỬA] \n -> \r\n
    snprintf(buffer, sizeof(buffer), "LIST_ADMIN_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    // [SỬA] recv_line -> recv_response
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return -1;
    }
    
    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nYour Admin Groups:\n%s\n", buffer);
    }
    
    int group_ids[100];
    int group_count = parse_ids_from_response(buffer, group_ids, 100, "[ID: ");
    
    if (group_count == 0) return -1;
    
    int selection;
    printf("Select group number (1-%d): ", group_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return -1;
    }
    
    return group_ids[selection - 1];
}

int get_selected_group_id(int sockfd) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_MY_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return -1;
    }

    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
        return -1;
    } else {
        printf("\nYour Groups:\n%s\n", buffer);
    }

    int group_ids[100];
    int group_count = parse_ids_from_response(buffer, group_ids, 100, "[ID: ");

    if (group_count == 0) return -1;

    int selection;
    printf("Select group number (1-%d): ", group_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return -1;
    }
    while(getchar() != '\n'); // Consume newline

    return group_ids[selection - 1];
}

// Reuse logic from get_selected_folder but generic
int get_selected_folder_id(int sockfd, int group_id, const char *prompt) {
    printf("\n%s\n", prompt ? prompt : "Select destination folder:");
    
    int current_folder_id = 0; // Start at group root
    
    // Fetch Group Root ID first
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "GET_GROUP_ROOT_ID %d\r\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    char root_id_str[64];
    recv_response(sockfd, root_id_str, sizeof(root_id_str));
    trim_newline(root_id_str);
    int root_id = atoi(root_id_str);
    
    current_folder_id = root_id;
    char current_folder_name[256] = "Group Root";
    
    while (1) {
        // List current folder contents
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
        if (current_folder_id != root_id) {
            printf("1. Go back to root\n");
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
        } else if (choice == 1 && current_folder_id != root_id) {
            // Go back to root
            current_folder_id = root_id;
            strcpy(current_folder_name, "Group Root");
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