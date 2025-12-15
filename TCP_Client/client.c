#include "client.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include "../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define CHUNK_SIZE 4096

void handle_upload_client(int sockfd) {
    char filepath[256];
    printf("Enter file path to upload: ");
    scanf("%255s", filepath);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Cannot open file");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    msg_header_t header;
    header.cmd = CMD_UPLOAD;
    header.length = 0;
    send_all(sockfd, &header, sizeof(header));

    char filename_only[256];
    char *p = strrchr(filepath, '/');
    if (p != NULL) {
        strcpy(filename_only, p + 1);
    } else {
        strcpy(filename_only, filepath);
    }

    int name_len = strlen(filename_only);
    send_all(sockfd, &name_len, sizeof(int));
    send_all(sockfd, filename_only, name_len);
    send_all(sockfd, &filesize, sizeof(long));

    printf("Uploading %s (%ld bytes)...\n", filename_only, filesize);

    char buffer[CHUNK_SIZE];
    size_t n;
    long total_sent = 0;
    while ((n = fread(buffer, 1, CHUNK_SIZE, fp)) > 0) {
        if (send_all(sockfd, buffer, n) < 0) {
            printf("Error sending data.\n");
            break;
        }
        total_sent += n;
    }

    printf("\nUpload complete.\n");
    fclose(fp);
}

void run_client() {
    int sockfd;
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return;
    }

    printf("Connected to server %s:%d\n", SERVER_IP, SERVER_PORT);

    int choice;
    while (1) {
        printf("\n--- MENU ---\n");
        printf("1. Upload File\n");
        printf("0. Exit\n");
        printf("Select: ");

        if (scanf("%d", &choice) != 1) break;

        switch (choice) {
            case 1:
                handle_upload_client(sockfd);
                break;
            case 0:
                close(sockfd);
                return;
            default:
                printf("Unknown option.\n");
        }
    }

    close(sockfd);
}

int main() {
    run_client();
    return 0;
}
