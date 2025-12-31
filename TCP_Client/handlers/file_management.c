#include "file_management.h"
#include "client_utils.h"
#include "../../common/file_utils.h"
#include "../../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

typedef struct {
    int id;
    char name[256];
    int is_folder;
} ClientItem;

static int fetch_items(int sockfd, int group_id, int parent_id, ClientItem *items, int max_items) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_FILE %d %d\r\n", group_id, parent_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0 || is_error_response(buffer)) return 0;
    
    int count = 0;
    char *saveptr;
    char *dup = strdup(buffer);
    char *line = strtok_r(dup, "\n", &saveptr);
    
    while (line && count < max_items) {
        if (strstr(line, "100 List:") || strlen(line) < 5) {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }
        if (strstr(line, "203 End")) break;
        
        char type_str[16], name[256], id_str[16];
        char *id_ptr = strstr(line, "[ID: ");
        if (id_ptr) {
            sscanf(id_ptr, "[ID: %15[^]]", id_str);
            items[count].id = atoi(id_str);
            
            sscanf(line, "%15s %255s", type_str, name);
            strncpy(items[count].name, name, 255);
            items[count].is_folder = (strcmp(type_str, "FOLDER") == 0);
            count++;
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(dup);
    return count;
}

static int select_item(ClientItem *items, int count, int folder_only, const char *prompt) {
    int display_indices[100];
    int display_count = 0;

    printf("\n--- %s ---\n", prompt);
    for (int i = 0; i < count; i++) {
        if (folder_only && !items[i].is_folder) continue;
        printf("%d. [%s] %s (ID: %d)\n", display_count + 1, items[i].is_folder ? "DIR" : "FILE", items[i].name, items[i].id);
        display_indices[display_count++] = i;
        if (display_count >= 100) break;
    }

    if (display_count == 0) {
        printf("No matching items found.\n");
        return -1;
    }

    int choice;
    printf("Selection (1-%d, 0 to cancel): ", display_count);
    if (scanf("%d", &choice) != 1 || choice == 0) {
        while(getchar() != '\n');
        return -1;
    }
    while(getchar() != '\n');

    if (choice < 1 || choice > display_count) {
        printf("Invalid selection.\n");
        return -1;
    }

    return display_indices[choice - 1];
}

static void list_files_in_group(int sockfd, int group_id, int parent_id) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_FILE %d %d\r\n", group_id, parent_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv_response(sockfd, buffer, sizeof(buffer));
    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nItems (Current Folder ID: %d):\n%s\n", parent_id, buffer);
    }
}

static int get_selected_group_id(int sockfd) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_MY_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) return -1;
    buffer[n] = '\0';

    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nYour Groups:\n%s\n", buffer);
    }

    int ids[100];
    int count = parse_ids_from_response(buffer, ids, 100, "[ID: ");
    
    if (count == 0) {
        return -1;
    }

    int choice;
    printf("Select group number to manage (1-%d): ", count);
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > count) {
        while(getchar() != '\n');
        printf("Invalid selection.\n");
        return -1;
    }
    while(getchar() != '\n');
    return ids[choice - 1];
}

void handle_file_management(int sockfd) {
    int group_id = get_selected_group_id(sockfd);
    if (group_id == -1) return;

    char group_name[256] = "Unknown";
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "GET_GROUP_NAME %d\r\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    recv_response(sockfd, group_name, sizeof(group_name));
    trim_newline(group_name);

    int current_folder_id = 0; // 0 is root
    char current_folder_name[256] = "Root";

    while (1) {
        printf("\n--- File Management (Group: %s) ---\n", group_name);
        printf("Current Folder: %s (ID: %d)\n", current_folder_name, current_folder_id);
        printf("1. List Contents\n");
        printf("2. Enter Folder\n");
        printf("3. Go to Root\n");
        printf("4. Create Folder (MKDIR)\n");
        printf("5. Rename File/Folder\n");
        printf("6. Delete File/Folder\n");
        printf("7. Copy File/Folder\n");
        printf("8. Move File/Folder\n");
        printf("9. Back to Main Menu\n");
        printf("Select: ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }
        while(getchar() != '\n');

        if (choice == 9) break;

        char buffer[1024];
        char resp[32768];
        int item_id; 
        char name_arg[256];
        ClientItem current_items[100];
        int item_count;

        switch (choice) {
            case 1:
                list_files_in_group(sockfd, group_id, current_folder_id);
                break;
            case 2:
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                if (item_count == 0) {
                    printf("No folders found.\n");
                } else {
                    int selected_idx = select_item(current_items, item_count, 1, "Select folder to enter");
                    if (selected_idx != -1) {
                        current_folder_id = current_items[selected_idx].id;
                        strncpy(current_folder_name, current_items[selected_idx].name, 255);
                        current_folder_name[255] = '\0';
                    }
                }
                break;
            case 3:
                current_folder_id = 0;
                strcpy(current_folder_name, "Root");
                printf("Back to Root.\n");
                break;
            case 4:
                printf("New folder name: ");
                scanf("%255s", name_arg);
                while(getchar() != '\n');
                snprintf(buffer, sizeof(buffer), "MKDIR %d %s %d\r\n", group_id, name_arg, current_folder_id);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            case 5:
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                int r_idx = select_item(current_items, item_count, 0, "Select item to rename");
                if (r_idx == -1) break;
                item_id = current_items[r_idx].id;

                printf("New name: ");
                scanf("%255s", name_arg);
                while(getchar() != '\n');
                snprintf(buffer, sizeof(buffer), "RENAME %d %d %s\r\n", group_id, item_id, name_arg);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            case 6:
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                int d_idx = select_item(current_items, item_count, 0, "Select item to delete");
                if (d_idx == -1) break;
                item_id = current_items[d_idx].id;

                snprintf(buffer, sizeof(buffer), "DELETE %d %d\r\n", group_id, item_id);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            case 7:
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                int c_idx = select_item(current_items, item_count, 0, "Select item to copy");
                if (c_idx == -1) break;
                item_id = current_items[c_idx].id;

                snprintf(buffer, sizeof(buffer), "COPY %d %d %d\r\n", group_id, item_id, current_folder_id);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            case 8:
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                int m_idx = select_item(current_items, item_count, 0, "Select item to move");
                if (m_idx == -1) break;
                item_id = current_items[m_idx].id;

                printf("Enter target Folder ID (manual entry or use another session to find ID): ");
                int target_parent;
                if (scanf("%d", &target_parent) != 1) { while(getchar() != '\n'); break; }
                while(getchar() != '\n');
                
                snprintf(buffer, sizeof(buffer), "MOVE %d %d %d\r\n", group_id, item_id, target_parent);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            default:
                printf("Invalid choice.\n");
                break;
        }
    }
}
