#include "client.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include "../common/utils.h"
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

/**
 * @function trim_newline
 * Removes trailing newline characters from a string.
 *
 * @param s The string to trim
 */
static void trim_newline(char *s) {
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
static int recv_line(int sockfd, char *buf, size_t maxlen) {
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

/**
 * @function get_filename
 * Extracts the filename from a full path.
 *
 * @param path The full file path
 * @return const char* Pointer to the filename part
 */
static const char* get_filename(const char *path) {
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
        printf("1. Upload File\n");
        printf("0. Exit\n");
        printf("Select: ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // Clear invalid input
            continue;
        }

        switch (choice) {
            case 1: handle_upload_client(sockfd); break;
            case 0: close(sockfd); return;
            default: printf("Invalid option.\n");
        }
    }
}

/**
 * @function main
 * Program entry point.
 */
int main() {
    run_client();
    return 0;
}