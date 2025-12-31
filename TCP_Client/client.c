#include "client.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include "../common/utils.h"
#include "handlers/handlers.h"
#include "handlers/file_management.h"
#include "handlers/client_utils.h"
#include "ui.h"
#include "helpers/group_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5500

// Import các tiện ích client (chứa hàm recv_response, trim_newline)
#include "handlers/client_utils.h"

/**
 * @function get_filename
 * Extracts the filename from a full path.
 * (GIỮ NGUYÊN THEO YÊU CẦU)
 */
const char *get_filename(const char *path)
{
    const char *p = strrchr(path, '/');
    if (p != NULL) {
        return p + 1;
    }
    return path;
}

/**
 * @function handle_upload_client
 * Handles the file upload process from client to server.
 * (GIỮ NGUYÊN VỊ TRÍ, SỬA NỘI DUNG BÊN TRONG)
 */
void handle_upload_client(int sockfd) {
    // Xóa bộ đệm bàn phím
    int c; while ((c = getchar()) != '\n' && c != EOF) {}

    char filepath[512];
    printf("Enter LOCAL file path: ");
    if (!fgets(filepath, sizeof(filepath), stdin)) return;
    trim_newline(filepath);
    if (strlen(filepath) == 0) return;

    // Check file existence and size
    struct stat st;
    if (stat(filepath, &st) != 0) {
        perror("Error accessing file");
        return;
    }
    long long filesize = (long long)st.st_size;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Cannot open file");
        return;
    }

    // NEW: Select group and folder using improved UI
    int group_id = get_selected_group_id(sockfd);
    if (group_id == -1) {
        printf("No group selected.\n");
        fclose(fp);
        return;
    }

    int folder_id = get_selected_folder_id(sockfd, group_id, "Select destination folder for upload:");
    if (folder_id == -1) {
        printf("No folder selected.\n");
        fclose(fp);
        return;
    }

    // 1. Gửi lệnh UPLOAD với format mới: group_id/parent_id
    char header[1024];
    snprintf(header, sizeof(header), "UPLOAD %d/%d \"%s\" %lld\r\n", 
             group_id, folder_id, get_filename(filepath), filesize);
    
    if (send_all(sockfd, header, strlen(header)) < 0) {
        perror("Failed to send header");
        fclose(fp);
        return;
    }

    // 2. Nhận phản hồi "150 Ready"
    char response[256];
    if (recv_response(sockfd, response, sizeof(response)) <= 0) {
        printf("Server disconnected.\n");
        fclose(fp);
        return;
    }

    printf("Uploading to Group ID %d, Folder ID %d...\n", group_id, folder_id);

    // 3. CẤP PHÁT ĐỘNG
    char *buffer = (char *)malloc(CHUNK_SIZE);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        fclose(fp);
        return;
    }

    // Vòng lặp gửi dữ liệu
    while (1) {
        size_t n = fread(buffer, 1, CHUNK_SIZE, fp);
        if (n == 0) break;

        if (send_all(sockfd, buffer, n) < 0) {
            perror("send_all error");
            break;
        }
    }

    // 4. GIẢI PHÓNG BỘ NHỚ
    free(buffer);
    fclose(fp);

    // 5. Nhận kết quả cuối cùng
    memset(response, 0, sizeof(response));
    recv_response(sockfd, response, sizeof(response));
    printf("\nResult: %s\n", response);
}

void handle_download_client(int sockfd) {
    int c; while ((c = getchar()) != '\n' && c != EOF) {}

    char local_folder[512];
    printf("Enter LOCAL save folder path: ");
    if (!fgets(local_folder, sizeof(local_folder), stdin)) return;
    trim_newline(local_folder);
    if (strlen(local_folder) == 0) strcpy(local_folder, ".");

    // NEW: Select group and browse files
    int group_id = get_selected_group_id(sockfd);
    if (group_id == -1) {
        printf("No group selected.\n");
        return;
    }

    // Browse and select file to download
    printf("\nBrowse files to download:\n");
    int current_folder_id = 0;
    char selected_filename[256] = {0};
    
    // Attempt to get real root ID
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "GET_GROUP_ROOT_ID %d\r\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    char root_id_str[64];
    recv_response(sockfd, root_id_str, sizeof(root_id_str));
    trim_newline(root_id_str);
    int real_root_id = atoi(root_id_str);
    if (real_root_id > 0) current_folder_id = real_root_id;

    int history[100];
    int history_top = -1;

    while (1) {
        char buffer[32768];
        snprintf(buffer, sizeof(buffer), "LIST_FILE %d %d\r\n", group_id, current_folder_id);
        send_all(sockfd, buffer, strlen(buffer));
        
        memset(buffer, 0, sizeof(buffer));
        recv_response(sockfd, buffer, sizeof(buffer));
        
        // Use clean header
        // printf("Contents:\n%s\n", buffer); // REMOVED raw dump
        
        typedef struct { int id; char name[256]; int is_folder; } Item;
        Item items[100];
        int item_count = 0;
        
        char *saveptr;
        char *dup = strdup(buffer);
        char *line = strtok_r(dup, "\n", &saveptr);
        
        printf("\nContents:\n");

        while (line && item_count < 100) {
            if (strstr(line, "List:") || strlen(line) < 5) {
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }
            if (strstr(line, "End")) break;
            
            char type_str[16], id_str[16], name[256];
            char *id_ptr = strstr(line, "[ID: ");
            
            // Clean parsing
            int id = 0;
            int is_folder = 0;
            
            if (id_ptr) {
                sscanf(id_ptr, "[ID: %15[^]]", id_str);
                id = atoi(id_str);
                
                sscanf(line, "%15s", type_str); 
                is_folder = (strcmp(type_str, "FOLDER") == 0);

                // Name parsing
                char *start_of_name = strchr(line, ' ');
                if (start_of_name) {
                    start_of_name++;
                    char *space_before_id = id_ptr - 1;
                    while (space_before_id > start_of_name && *space_before_id == ' ') space_before_id--;
                    char *start_of_size = space_before_id;
                    while (start_of_size > start_of_name && *start_of_size != ' ') start_of_size--;
                    
                    if (start_of_size > start_of_name) {
                        size_t name_len = start_of_size - start_of_name;
                        if (name_len > 255) name_len = 255;
                        strncpy(name, start_of_name, name_len);
                        name[name_len] = '\0';
                    } else {
                         sscanf(start_of_name, "%255s", name);
                    }
                }
                
                // Add to list
                items[item_count].id = id;
                strncpy(items[item_count].name, name, 255);
                items[item_count].is_folder = is_folder;
                
                // Print Clean
                if (is_folder) {
                    printf("  [FOLDER] %s\n", name);
                } else {
                    printf("  [FILE]   %s\n", name);
                }
                item_count++;
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
        free(dup);
        
        // Show navigation options
        printf("\nNavigation:\n");
        printf("0. Back to Main Menu\n"); // Exit download flow
        if (history_top >= 0) {
            printf("1. Go Back to Parent\n");
        }
        
        int option_idx = 2; 
        for (int i = 0; i < item_count; i++) {
            if (items[i].is_folder) {
                printf("%d. Enter folder: %s\n", option_idx, items[i].name);
            } else {
                printf("%d. Download file: %s\n", option_idx, items[i].name);
            }
            option_idx++;
        }
        
        int choice;
        printf("Choice: ");
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }
        while(getchar() != '\n');
        
        if (choice == 0) {
            return; // Exit function
        } else if (choice == 1 && history_top >= 0) {
            current_folder_id = history[history_top];
            history_top--;
        } else if (choice >= 2 && choice < option_idx) {
            int selected_idx = choice - 2;
            if (items[selected_idx].is_folder) {
                // Enter folder
                if (history_top < 99) {
                    history_top++;
                    history[history_top] = current_folder_id;
                }
                current_folder_id = items[selected_idx].id;
            } else {
                // Download file
                strncpy(selected_filename, items[selected_idx].name, sizeof(selected_filename) - 1);
                break; // Exit loop to proceed with download
            }
        } else {
            printf("Invalid choice.\n");
        }
    }
    
    if (strlen(selected_filename) == 0) return;

    // Build full local path
    char full_local_path[1024];
    snprintf(full_local_path, sizeof(full_local_path), "%s/%s", local_folder, selected_filename);

    // Send download command with new format
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "DOWNLOAD %d/%d \"%s\"\r\n", group_id, current_folder_id, selected_filename);

    if (send_all(sockfd, cmd, strlen(cmd)) < 0) {
        perror("send_all DOWNLOAD command failed");
        return;
    }

    char header[256];
    memset(header, 0, sizeof(header));

    if (recv_response(sockfd, header, sizeof(header)) <= 0) {
        printf("Server disconnected.\n");
        return;
    }

    if (strncmp(header, "102 ", 4) != 0) {
        printf("Server error: %s\n", header);
        return;
    }

    long long filesize = atoll(header + 4);
    printf("Downloading file: %s (%lld bytes)...\n", selected_filename, filesize);

    FILE *fp = fopen(full_local_path, "wb");
    if (!fp) {
        perror("Cannot create local file");
        return;
    }

    char *file_buf = (char *)malloc(CHUNK_SIZE);
    if (!file_buf) {
        perror("Memory allocation failed");
        fclose(fp);
        return;
    }

    long long received = 0;
    while (received < filesize) {
        size_t to_read = CHUNK_SIZE;
        if (filesize - received < CHUNK_SIZE) to_read = (size_t)(filesize - received);

        ssize_t n = recv(sockfd, file_buf, to_read, 0);
        if (n <= 0) {
            printf("Connection error during download.\n");
            break;
        }
        fwrite(file_buf, 1, n, fp);
        received += n;
    }

    free(file_buf);
    fclose(fp);

    if (received == filesize) {
        printf("Download completed: %s\n", full_local_path);
    } else {
        printf("Download incomplete (%lld/%lld bytes)\n", received, filesize);
        remove(full_local_path);
    }

    // NEW: Consume the final "200 File sent successfully" message
    char final_resp[1024];
    memset(final_resp, 0, sizeof(final_resp));
    recv_response(sockfd, final_resp, sizeof(final_resp));
}

void handle_list_directory(int sockfd) {
    // Select group and browse directories
    int group_id = get_selected_group_id(sockfd);
    if (group_id == -1) {
        printf("No group selected.\n");
        return;
    }

    int current_folder_id = 0; // Assuming 0 is root or use get_group_root_id
    char current_folder_name[256] = "Root";
    
    // Attempt to get real root ID, similar to file management
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "GET_GROUP_ROOT_ID %d\r\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    char root_id_str[64];
    recv_response(sockfd, root_id_str, sizeof(root_id_str));
    trim_newline(root_id_str);
    int real_root_id = atoi(root_id_str);
    if (real_root_id > 0) current_folder_id = real_root_id;

    // Helper stack for navigation (simple depth version)
    // For full path support we'd need a stack of IDs.
    // Let's keep it simple: "Go back" goes to parent (which we might need to fetch or track).
    // The previous implementation used 0 as parent for everything which is wrong if we have depth.
    // For now, let's just stick to the requested "clean display" and "exit" fix.
    // To properly support "Back", we should use a history stack like in file_management.c.
    
    int history[100];
    int history_top = -1;

    while (1) {
        char buffer[32768];
        snprintf(buffer, sizeof(buffer), "LIST_FILE %d %d\r\n", group_id, current_folder_id);
        send_all(sockfd, buffer, strlen(buffer));
        
        memset(buffer, 0, sizeof(buffer));
        recv_response(sockfd, buffer, sizeof(buffer));
        
        printf("\n=== Browsing Group: %d | Folder: %s ===\n", group_id, current_folder_name);
        
        // Parse items
        typedef struct { int id; char name[256]; int is_folder; } Item;
        Item items[100];
        int item_count = 0;
        
        char *saveptr;
        char *dup = strdup(buffer);
        char *line = strtok_r(dup, "\n", &saveptr);
        
        // Print header for file list
        printf("Contents:\n");

        while (line && item_count < 100) {
            if (strstr(line, "List:") || strlen(line) < 5) {
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }
            if (strstr(line, "End")) break;
            
            char type_str[16], id_str[16];
            char *id_ptr = strstr(line, "[ID: ");
            
            // Clean parsing similar to fetch_items
            char name[256] = {0};
            int is_folder = 0;
            int id = 0;

            if (id_ptr) {
                sscanf(id_ptr, "[ID: %15[^]]", id_str);
                id = atoi(id_str);
                
                sscanf(line, "%15s", type_str);
                is_folder = (strcmp(type_str, "FOLDER") == 0);

                // Parse Name (robust handling of spaces)
                char *start_of_name = strchr(line, ' ');
                if (start_of_name) {
                    start_of_name++; // skip space after type
                    char *space_before_id = id_ptr - 1;
                    while (space_before_id > start_of_name && *space_before_id == ' ') space_before_id--;
                    char *start_of_size = space_before_id;
                    while (start_of_size > start_of_name && *start_of_size != ' ') start_of_size--;
                    
                    if (start_of_size > start_of_name) {
                        size_t name_len = start_of_size - start_of_name;
                        if (name_len > 255) name_len = 255;
                        strncpy(name, start_of_name, name_len);
                        name[name_len] = '\0';
                    } else {
                         sscanf(start_of_name, "%255s", name);
                    }
                }
            } else {
                 // Fallback if ID invalid
                 line = strtok_r(NULL, "\n", &saveptr);
                 continue;
            }

            // Print Clean Entry
            if (is_folder) {
                printf("  [FOLDER] %s\n", name);
                items[item_count].id = id;
                strncpy(items[item_count].name, name, 255);
                items[item_count].is_folder = 1;
                item_count++;
            } else {
                printf("  [FILE]   %s\n", name);
            }

            line = strtok_r(NULL, "\n", &saveptr);
        }
        free(dup);
        
        if (item_count == 0) {
            // Check if there were any files printed (since we only stored folders in items for navigation)
            // But verify logic: loop printed both. items array only needs folders?
            // "Enter folder" only makes sense for folders.
            // Let's store text in items regardless? No, navigation only for folders.
            // Wait, previous code only stored folders in `items`.
            // Let's keep that behavior.
        }

        // Show navigation options
        printf("\nNavigation:\n");
        printf("0. Back to main menu\n");
        if (history_top >= 0) {
            printf("1. Go Back to Parent\n");
        }
        
        int option_idx = 2; // Start after 0 and 1
        for (int i = 0; i < item_count; i++) {
            if (items[i].is_folder) {
                printf("%d. Enter folder: %s\n", option_idx, items[i].name);
                 // We re-use item_count for folders only? 
                 // My parsing loop added only if is_folder? 
                 // Ah, previous code: if (id_ptr && strncmp(line, "FOLDER", 6) == 0) -> add to items.
                 // My new loop prints files but needs to add folders to items array.
                 // Correct logic:
                 // Loop: parse. Print. If folder -> items[item_count++] = ...
            }
            option_idx++;
        }
        
        int choice;
        printf("Choice: ");
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }
        while(getchar() != '\n');
        
        if (choice == 0) {
            break; // Back to main menu
        } else if (choice == 1 && history_top >= 0) {
            // Pop history
            current_folder_id = history[history_top];
            history_top--;
            strcpy(current_folder_name, "Parent"); // Name lost unless we stack it too. simplified.
        } else if (choice >= 2 && choice < option_idx) {
            // Find which folder corresponds to choice
            // Since we iterated and printed, we need to map choice back to items index.
            // Since I incremented option_idx for every item? No, only for folders?
            // Wait, the loop above: `for (int i = 0; i < item_count; i++) ... printf`
            // implies that `items` ONLY contains folders.
            // Let's ensure parsing loop only adds folders to `items`.
            
            int folder_idx = choice - 2;
             if (folder_idx >= 0 && folder_idx < item_count) {
                // Push current to history
                if (history_top < 99) {
                    history_top++;
                    history[history_top] = current_folder_id;
                }
                
                current_folder_id = items[folder_idx].id;
                strncpy(current_folder_name, items[folder_idx].name, 255);
            } else {
                printf("Invalid choice.\n");
            }
        } else {
            printf("Invalid choice.\n");
        }
    }
}

/**
 * @function run_client
 * Main loop for the client application.
 */
void run_client(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sockfd);
        return;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return;
    }

    printf("Connected to %s:%d\n", ip, port);

    int choice;
    while (1) {
        printf("\n--- FILE SHARING CLIENT ---\n");
        print_menu();
        printf("Select: ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // Clear invalid input
            continue;
        }

        switch (choice) {
            case 1:  // Login
                handle_login(sockfd);
                break;

            case 2:  // Register
                handle_register(sockfd);
                break;

            case 3:  // Upload
                handle_upload_client(sockfd);
                break;
            
            case 4:  // Download
                handle_download_client(sockfd);
                break;

            case 5:  // List Directory
                handle_list_directory(sockfd);
                break;

            case 6:  // Create Group
                handle_create_group(sockfd);
                break;

            case 7:  // List Groups
                handle_list_groups(sockfd);
                break;

            case 8:  // List Members
                handle_list_group_members(sockfd);
                break;

            case 9:  // Join Group
                handle_join_group(sockfd);
                break;

            case 10: // Invite User
                handle_invite_user(sockfd);
                break;

            case 11: // Approve Request
                handle_approve_request(sockfd);
                break;

            case 12: // Accept Invitation
                handle_accept_invitation(sockfd);
                break;

            case 13: // Kick Member
                handle_kick_member(sockfd);
                break;

            case 14: // Leave Group
                handle_leave_group(sockfd);
                break;

            case 15: // File Management
                handle_file_management(sockfd);
                break;

            case 16: // Logout
                handle_logout(sockfd);
                break;

            case 17: // Exit
                close(sockfd);
                return;

            default:
                printf("Unknown option.\n");
        }
    }

    close(sockfd);
}

/**
 * @function main
 * Program entry point.
 */
int main(int argc, char **argv) {
    const char *ip = SERVER_IP;
    int port = SERVER_PORT;

    // Usage: ./client [server_ip] [port]; one argument is treated as port.
    if (argc >= 3) {
        ip = argv[1];
        port = atoi(argv[2]);
    } else if (argc == 2) {
        port = atoi(argv[1]);
    }
    if (port <= 0) port = SERVER_PORT;

    run_client(ip, port);
    return 0;
}