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
        if (strstr(line, "List:") || strlen(line) < 5) {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }
        if (strstr(line, "End")) break;
        
        char type_str[16], id_str[16];
        char *id_ptr = strstr(line, "[ID: ");
        if (id_ptr) {
            // 1. Parse ID
            sscanf(id_ptr, "[ID: %15[^]]", id_str);
            items[count].id = atoi(id_str);
            
            // 2. Parse Type
            sscanf(line, "%15s", type_str);
            items[count].is_folder = (strcmp(type_str, "FOLDER") == 0);

            // 3. Parse Name (robust handling of spaces)
            char *start_of_name = strchr(line, ' ');
            if (start_of_name) {
                start_of_name++; // skip space after type
                
                // The format is: TYPE NAME SIZE [ID: ID]
                // We find the space before SIZE. SIZE is the word immediately preceding [ID: ]
                char *space_before_id = id_ptr - 1;
                while (space_before_id > start_of_name && *space_before_id == ' ') space_before_id--;
                
                char *start_of_size = space_before_id;
                while (start_of_size > start_of_name && *start_of_size != ' ') start_of_size--;
                
                // Everything between start_of_name and start_of_size is the NAME
                if (start_of_size > start_of_name) {
                    size_t name_len = start_of_size - start_of_name;
                    if (name_len > 255) name_len = 255;
                    strncpy(items[count].name, start_of_name, name_len);
                    items[count].name[name_len] = '\0';
                } else {
                    // Fallback for unexpected format
                    sscanf(start_of_name, "%255s", items[count].name);
                }
            }
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

// Moved to client_utils.c - use the shared function

void handle_file_management(int sockfd) {
    int group_id = get_selected_group_id(sockfd);
    if (group_id == -1) return;

    char group_name[256] = "Unknown";
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "GET_GROUP_NAME %d\r\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    recv_response(sockfd, group_name, sizeof(group_name));
    trim_newline(group_name);

    // Fetch Group Root ID
    int current_folder_id = 0;
    snprintf(buffer, sizeof(buffer), "GET_GROUP_ROOT_ID %d\r\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    char root_id_str[64];
    recv_response(sockfd, root_id_str, sizeof(root_id_str));
    trim_newline(root_id_str);
    current_folder_id = atoi(root_id_str);

    char current_folder_name[256];
    strncpy(current_folder_name, group_name, sizeof(current_folder_name)); // Init with Group Name as Root Name

    // Stack to keep track of folder history for "Back to Parent"
    struct {
        int id;
        char name[256];
    } history_stack[100];
    int history_top = -1;

    while (1) {
        printf("\n--- File Management (Group: %s) ---\n", group_name);
        printf("Current Folder: %s (ID: %d)\n", current_folder_name, current_folder_id);
        printf("1. List Contents\n");
        printf("2. Enter Folder\n");
        printf("3. Back to Parent Folder\n");
        printf("4. Create Folder (MKDIR)\n");
        printf("5. Rename File/Folder\n");
        printf("6. Move File/Folder\n");
        printf("7. Copy File/Folder\n");
        printf("8. Delete File/Folder\n");
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
                        // Push current folder to history stack before entering new one
                        if (history_top < 99) {
                            history_top++;
                            history_stack[history_top].id = current_folder_id;
                            strncpy(history_stack[history_top].name, current_folder_name, 255);
                        }
                        
                        current_folder_id = current_items[selected_idx].id;
                        strncpy(current_folder_name, current_items[selected_idx].name, 255);
                        current_folder_name[255] = '\0';
                    }
                }
                break;
            case 3:
                if (history_top >= 0) {
                    current_folder_id = history_stack[history_top].id;
                    strncpy(current_folder_name, history_stack[history_top].name, 255);
                    history_top--;
                    printf("Back to %s.\n", current_folder_name);
                } else {
                    printf("Already at %s.\n", current_folder_name);
                }
                break;
            case 4:
                printf("New folder name: ");
                scanf("%255s", name_arg);
                while(getchar() != '\n');
                snprintf(buffer, sizeof(buffer), "MKDIR %d \"%s\" %d\r\n", group_id, name_arg, current_folder_id);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            case 5:
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                int r_idx = select_item(current_items, item_count, 0, "Select file/folder to rename");
                if (r_idx == -1) break;
                item_id = current_items[r_idx].id;

                printf("New name: ");
                scanf("%255s", name_arg);
                while(getchar() != '\n');
                snprintf(buffer, sizeof(buffer), "RENAME %d %d \"%s\"\r\n", group_id, item_id, name_arg);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            case 6: // Was Move (8)
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                int m_idx = select_item(current_items, item_count, 0, "Select file/folder to move");
                if (m_idx == -1) break;
                item_id = current_items[m_idx].id;

                // Fetch all folders for destination selection
                snprintf(buffer, sizeof(buffer), "LIST_ALL_FOLDERS %d\r\n", group_id);
                send_all(sockfd, buffer, strlen(buffer));
                
                memset(resp, 0, sizeof(resp));
                recv_response(sockfd, resp, sizeof(resp));
                
                if (is_error_response(resp)) {
                    printf("%s\n", resp);
                    break;
                }

                printf("\n--- Select destination folder ---\n");
                printf("0. Root Directory\n");
                
                // Parse response manually for simple display
                // Response format: 100 List:\n[FOLDER] Name (ID: X)\n...
                int folder_ids[100];
                int folder_count = 0;
                
                char *dup_resp = strdup(resp);
                char *line_tok = strtok(dup_resp, "\n");
                while (line_tok) {
                    if (strstr(line_tok, "[FOLDER]")) {
                        // Display line
                        printf("%d. %s\n", folder_count + 1, line_tok);
                        
                        // Extract ID
                        char *id_ptr = strstr(line_tok, "(ID: ");
                        if (id_ptr) {
                            folder_ids[folder_count] = atoi(id_ptr + 5);
                            folder_count++;
                        }
                    }
                    line_tok = strtok(NULL, "\n");
                }
                free(dup_resp);

                if (folder_count == 0) {
                    printf("(No other folders found. Move to Root?)\n");
                }

                int dest_choice;
                printf("Selection (0-%d): ", folder_count);
                if (scanf("%d", &dest_choice) != 1) { while(getchar() != '\n'); break; }
                while(getchar() != '\n');

                int target_parent = 0; // Default to Root
                if (dest_choice > 0 && dest_choice <= folder_count) {
                    target_parent = folder_ids[dest_choice - 1];
                } else if (dest_choice != 0) {
                    printf("Invalid selection. Cancelled.\n");
                    break;
                }
                
                snprintf(buffer, sizeof(buffer), "MOVE %d %d %d\r\n", group_id, item_id, target_parent);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            case 7: // Was Copy (7) - No change in logic block
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                int c_idx = select_item(current_items, item_count, 0, "Select file/folder to copy");
                if (c_idx == -1) break;
                item_id = current_items[c_idx].id;

                snprintf(buffer, sizeof(buffer), "COPY %d %d %d\r\n", group_id, item_id, current_folder_id);
                send_all(sockfd, buffer, strlen(buffer));
                recv_response(sockfd, resp, sizeof(resp));
                trim_newline(resp);
                printf("Server: %s\n", resp);
                break;
            case 8: // Was Delete (6)
                item_count = fetch_items(sockfd, group_id, current_folder_id, current_items, 100);
                int d_idx = select_item(current_items, item_count, 0, "Select file/folder to delete");
                if (d_idx == -1) break;
                item_id = current_items[d_idx].id;

                snprintf(buffer, sizeof(buffer), "DELETE %d %d\r\n", group_id, item_id);
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
