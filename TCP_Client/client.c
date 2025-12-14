#include "client.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include "../common/utils.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 5500
#define CHUNK_SIZE 4096

// Gửi file theo từng chunk tới server
static void handle_upload_client(int sockfd) {
    char filepath[256];
    printf("Enter file path to upload: ");
    if (scanf("%255s", filepath) != 1) return;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Cannot open file");
        return;
    }

    // Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Gửi header lệnh UPLOAD
    msg_header_t header;
    header.cmd = CMD_UPLOAD;
    header.length = 0; // payload riêng cho metadata
    if (send_all(sockfd, &header, sizeof(header)) < 0) {
        perror("send header");
        fclose(fp);
        return;
    }

    // Metadata: tên file + kích thước
    char filename_only[256];
    char *p = strrchr(filepath, '/');
    strcpy(filename_only, p ? p + 1 : filepath);

    int name_len = (int)strlen(filename_only);
    if (send_all(sockfd, &name_len, sizeof(int)) < 0 ||
        send_all(sockfd, filename_only, name_len) < 0 ||
        send_all(sockfd, &filesize, sizeof(long)) < 0) {
        perror("send metadata");
        fclose(fp);
        return;
    }

    printf("Uploading %s (%ld bytes)...\n", filename_only, filesize);

    // Gửi nội dung file theo chunk
    char buffer[CHUNK_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, CHUNK_SIZE, fp)) > 0) {
        if (send_all(sockfd, buffer, n) < 0) {
            printf("Error sending data.\n");
            break;
        }
    }

    printf("Upload complete.\n");
    fclose(fp);
    // TODO: nhận phản hồi từ server (OK/FAIL)
}

void run_client(const char *host, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;

    const char *target_host = (host && host[0]) ? host : DEFAULT_SERVER_IP;
    int target_port = (port > 0) ? port : DEFAULT_SERVER_PORT;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_host, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        return;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sockfd);
        return;
    }

    printf("Connected to server %s:%d\n", target_host, target_port);

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

int main(int argc, char **argv) {
    const char *host = DEFAULT_SERVER_IP;
    int port = DEFAULT_SERVER_PORT;
    if (argc >= 2) host = argv[1];
    if (argc >= 3) {
        int p = atoi(argv[2]);
        if (p > 0) port = p;
    }

    run_client(host, port);
    return 0;
}
