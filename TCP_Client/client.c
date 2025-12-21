#include "client.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include "../common/utils.h"
#include "handlers/handlers.h"
#include "ui.h"

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
#define CHUNK_SIZE 4096

#include "handlers/client_utils.h"

/**
 * @function get_filename
 * Extracts the filename from a full path.
 *
 * @param path The full file path
 * @return const char* Pointer to the filename part
 */
const char* get_filename(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\'); // Check Windows slash
    return slash ? (slash + 1) : path;
}

/**
 * @function handle_upload_client
 * Handles the file upload process from client to server.
 *
 * @param sockfd Socket file descriptor connected to server
 */
void handle_upload_client(int sockfd) {
    // Clear input buffer
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

    char remote_folder[256];
    printf("Enter destination folder (ENTER for root): ");
    if (!fgets(remote_folder, sizeof(remote_folder), stdin)) strcpy(remote_folder, ".");
    trim_newline(remote_folder);
    if (strlen(remote_folder) == 0) strcpy(remote_folder, ".");

    // 1. Send Upload Header
    char header[1024];
    snprintf(header, sizeof(header), "UPLOAD %s %s %lld\r\n", 
             remote_folder, get_filename(filepath), filesize);
    
    if (send(sockfd, header, strlen(header), 0) < 0) {
        perror("Failed to send header");
        fclose(fp);
        return;
    }

    // 2. Wait for Server Readiness
    char response[256];
    if (recv_line(sockfd, response, sizeof(response)) <= 0) {
        printf("Server disconnected.\n");
        fclose(fp);
        return;
    }

    int code = 0;
    sscanf(response, "%d", &code);
    if (code != RES_UPLOAD_READY) {
        printf("Server rejected upload: %s\n", response);
        fclose(fp);
        return;
    }

    printf("Uploading... ");
    fflush(stdout);

    // 3. Send File Data
    char buffer[CHUNK_SIZE];
    while (1) {
        size_t n = fread(buffer, 1, sizeof(buffer), fp);
        if (n <= 0) break;
        if (send(sockfd, buffer, n, 0) < 0) {
            perror("Send error");
            break;
        }
    }
    fclose(fp);

    // 4. Wait for Final Result
    recv_line(sockfd, response, sizeof(response));
    printf("\nResult: %s\n", response);
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

            case 4:  // Create Group
                printf("Feature under development\n");
                break;

            case 5:  // List Groups
                printf("Feature under development\n");
                break;

            case 6:  // List Members
                printf("Feature under development\n");
                break;

            case 7:  // Join Group
                handle_join_group(sockfd);
                break;

            case 8:  // Invite User
                handle_invite_user(sockfd);
                break;

            case 9:  // Approve Request
                handle_approve_request(sockfd);
                break;

            case 10: // Accept Invitation
                handle_accept_invitation(sockfd);
                break;

            case 11: // Kick Member
                handle_kick_member(sockfd);
                break;

            case 12: // Leave Group
                handle_leave_group(sockfd);
                break;

            case 13: // Logout
                handle_logout(sockfd);
                break;

            case 0:  // Exit
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