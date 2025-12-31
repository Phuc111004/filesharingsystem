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
    snprintf(header, sizeof(header), "UPLOAD %d/%d %s %lld\r\n", 
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
    
    while (1) {
        // List current folder contents
        char buffer[32768];
        snprintf(buffer, sizeof(buffer), "LIST_FILE %d %d\r\n", group_id, current_folder_id);
        send_all(sockfd, buffer, strlen(buffer));
        
        memset(buffer, 0, sizeof(buffer));
        recv_response(sockfd, buffer, sizeof(buffer));
        
        printf("\nCurrent Folder ID: %d\n", current_folder_id);
        printf("Contents:\n%s\n", buffer);
        
        // Parse items
        typedef struct { int id; char name[256]; int is_folder; } Item;
        Item items[100];
        int item_count = 0;
        
        char *saveptr;
        char *dup = strdup(buffer);
        char *line = strtok_r(dup, "\n", &saveptr);
        
        while (line && item_count < 100) {
            if (strstr(line, "100 List:") || strlen(line) < 5) {
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }
            if (strstr(line, "203 End")) break;
            
            char type_str[16], name[256], id_str[16];
            char *id_ptr = strstr(line, "[ID: ");
            if (id_ptr) {
                sscanf(id_ptr, "[ID: %15[^]]", id_str);
                items[item_count].id = atoi(id_str);
                
                sscanf(line, "%15s %255s", type_str, name);
                strncpy(items[item_count].name, name, 255);
                items[item_count].is_folder = (strcmp(type_str, "FOLDER") == 0);
                item_count++;
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
        free(dup);
        
        // Show options
        printf("\nOptions:\n");
        if (current_folder_id != 0) {
            printf("0. Go back to parent\n");
        }
        for (int i = 0; i < item_count; i++) {
            printf("%d. %s %s\n", i + 1, 
                   items[i].is_folder ? "[FOLDER]" : "[FILE]", 
                   items[i].name);
        }
        
        int choice;
        printf("Choice (0 to go back, or select item): ");
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }
        while(getchar() != '\n');
        
        if (choice == 0 && current_folder_id != 0) {
            current_folder_id = 0; // Simplified: go to root
        } else if (choice >= 1 && choice <= item_count) {
            int idx = choice - 1;
            if (items[idx].is_folder) {
                current_folder_id = items[idx].id;
            } else {
                // Selected a file - download it
                strncpy(selected_filename, items[idx].name, sizeof(selected_filename) - 1);
                break;
            }
        } else {
            printf("Invalid choice.\n");
        }
    }
    
    if (strlen(selected_filename) == 0) {
        printf("No file selected.\n");
        return;
    }

    // Build full local path
    char full_local_path[1024];
    snprintf(full_local_path, sizeof(full_local_path), "%s/%s", local_folder, selected_filename);

    // Send download command with new format
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "DOWNLOAD %d/%d %s\r\n", group_id, current_folder_id, selected_filename);

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

    char *buffer = (char *)malloc(CHUNK_SIZE);
    if (!buffer) {
        perror("Memory allocation failed");
        fclose(fp);
        return;
    }

    long long received = 0;
    while (received < filesize) {
        size_t to_read = CHUNK_SIZE;
        if (filesize - received < CHUNK_SIZE) to_read = (size_t)(filesize - received);

        ssize_t n = recv(sockfd, buffer, to_read, 0);
        if (n <= 0) {
            printf("Connection error during download.\n");
            break;
        }
        fwrite(buffer, 1, n, fp);
        received += n;
    }

    free(buffer);
    fclose(fp);

    if (received == filesize) {
        printf("Download completed: %s\n", full_local_path);
    } else {
        printf("Download incomplete (%lld/%lld bytes)\n", received, filesize);
        remove(full_local_path);
    }
}

void handle_list_directory(int sockfd) {
    // Select group and browse directories
    int group_id = get_selected_group_id(sockfd);
    if (group_id == -1) {
        printf("No group selected.\n");
        return;
    }

    printf("\nBrowsing directory contents:\n");
    int current_folder_id = 0;
    char current_folder_name[256] = "Root";
    
    while (1) {
        // List current folder contents
        char buffer[32768];
        snprintf(buffer, sizeof(buffer), "LIST_FILE %d %d\r\n", group_id, current_folder_id);
        send_all(sockfd, buffer, strlen(buffer));
        
        memset(buffer, 0, sizeof(buffer));
        recv_response(sockfd, buffer, sizeof(buffer));
        
        printf("\n=== Group ID: %d, Folder: %s (ID: %d) ===\n", group_id, current_folder_name, current_folder_id);
        printf("%s\n", buffer);
        
        // Parse folders for navigation
        typedef struct { int id; char name[256]; int is_folder; } Item;
        Item items[100];
        int item_count = 0;
        
        char *saveptr;
        char *dup = strdup(buffer);
        char *line = strtok_r(dup, "\n", &saveptr);
        
        while (line && item_count < 100) {
            if (strstr(line, "100 List:") || strlen(line) < 5) {
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }
            if (strstr(line, "203 End")) break;
            
            char type_str[16], name[256], id_str[16];
            char *id_ptr = strstr(line, "[ID: ");
            if (id_ptr && strncmp(line, "FOLDER", 6) == 0) {
                sscanf(id_ptr, "[ID: %15[^]]", id_str);
                items[item_count].id = atoi(id_str);
                
                sscanf(line, "%15s %255s", type_str, name);
                strncpy(items[item_count].name, name, 255);
                items[item_count].is_folder = 1;
                item_count++;
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
        free(dup);
        
        // Show navigation options
        printf("\nNavigation:\n");
        printf("0. Back to main menu\n");
        if (current_folder_id != 0) {
            printf("1. Go back to root\n");
        }
        for (int i = 0; i < item_count; i++) {
            printf("%d. Enter folder: %s\n", i + 2, items[i].name);
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
        } else if (choice == 1 && current_folder_id != 0) {
            current_folder_id = 0;
            strcpy(current_folder_name, "Root");
        } else if (choice >= 2 && choice < item_count + 2) {
            int idx = choice - 2;
            current_folder_id = items[idx].id;
            strncpy(current_folder_name, items[idx].name, 255);
            current_folder_name[255] = '\0';
        } else {
            printf("Invalid choice.\n");
        }
    }
}

/**
 * @function run_client
 * Main loop for the client application.
 */
void run_client() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sockfd);
        return;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return;
    }

    printf("Connected to %s:%d\n", SERVER_IP, SERVER_PORT);

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

            case 15: // Logout
                handle_logout(sockfd);
                break;

            case 16: // File Management
                handle_file_management(sockfd);
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
int main() {
    run_client();
    return 0;
}