#include "client.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include "../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../common/utils.h"
#include "ui.h"

#define SERVER_PORT 8080 // Bạn có thể chuyển vào config nếu muốn
#define SERVER_IP "127.0.0.1"
#define CHUNK_SIZE 4096

// Hàm phụ trợ để gửi file (Streaming)
void handle_upload_client(int sockfd) {
    char filepath[256];
    printf("Enter file path to upload: ");
    scanf("%255s", filepath);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Cannot open file");
        return;
    }

    // 1. Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 2. Gửi Header báo hiệu lệnh UPLOAD
    msg_header_t header;
    header.cmd = CMD_UPLOAD;
    header.length = 0; // Payload length ban đầu = 0, ta sẽ gửi metadata riêng
    send_all(sockfd, &header, sizeof(header));

    // 3. Gửi Metadata (Tên file và Kích thước)
    // Cấu trúc tạm để gửi thông tin file
    char filename_only[256];
    // (Logic tách tên file từ đường dẫn đơn giản)
    char *p = strrchr(filepath, '/');
    strcpy(filename_only, p ? p + 1 : filepath);

    // Gửi độ dài tên file
    int name_len = strlen(filename_only);
    send_all(sockfd, &name_len, sizeof(int));
    // Gửi tên file
    send_all(sockfd, filename_only, name_len);
    // Gửi kích thước file
    send_all(sockfd, &filesize, sizeof(long));

    printf("Uploading %s (%ld bytes)...\n", filename_only, filesize);

    // 4. Gửi nội dung file theo Chunk (Streaming)
    char buffer[CHUNK_SIZE];
    size_t n;
    long total_sent = 0;
    while ((n = fread(buffer, 1, CHUNK_SIZE, fp)) > 0) {
        if (send_all(sockfd, buffer, n) < 0) {
            printf("Error sending data.\n");
            break;
        }
        total_sent += n;
        // Hiển thị tiến độ đơn giản
        // printf("\rSent: %ld%%", (total_sent * 100) / filesize);
    }

    printf("\nUpload complete.\n");
    fclose(fp);
    
    // 5. Đợi Server phản hồi kết quả (OK/FAIL)
    // (Tùy chọn: Implement recv response từ server ở đây)
}

void run_client() {
    int sockfd;
    struct sockaddr_in serv_addr;

    // 1. Tạo socket
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

    // 2. Kết nối
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return;
    }

    printf("Connected to server %s:%d\n", SERVER_IP, SERVER_PORT);

    // 3. Vòng lặp chính
    int choice;
    while (1) {
        // Gọi hàm in menu từ ui.c (bạn cần include prototype hoặc extern)
        // print_menu(); 
        printf("\n--- MENU ---\n");
        printf("1. Upload File\n");
        printf("0. Exit\n");
        printf("Select: ");
        
        if (scanf("%d", &choice) != 1) break;

        switch (choice) {
            case 1: // Upload
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