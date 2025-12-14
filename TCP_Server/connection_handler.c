#include "connection_handler.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h> // Để dùng mkdir

#define CHUNK_SIZE 4096

// Hàm xử lý nhận file
void handle_upload_server(int client_sock) {
    // 1. Nhận Metadata tương ứng với Client gửi
    int name_len;
    if (recv_all(client_sock, &name_len, sizeof(int)) <= 0) return;

    char filename[256];
    if (recv_all(client_sock, filename, name_len) <= 0) return;
    filename[name_len] = '\0'; // Null terminate

    long filesize;
    if (recv_all(client_sock, &filesize, sizeof(long)) <= 0) return;

    printf("[Server] Receiving file: %s (%ld bytes)\n", filename, filesize);

    // Tạo thư mục storage nếu chưa có
    struct stat st = {0};
    if (stat("storage", &st) == -1) {
        mkdir("storage", 0700);
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "storage/%s", filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[Server] Cannot create file");
        // Cần cơ chế báo lỗi về client, nhưng tạm thời return
        return;
    }

    // 2. Nhận Stream nội dung file
    char buffer[CHUNK_SIZE];
    long total_received = 0;
    long remaining = filesize;

    while (remaining > 0) {
        size_t to_read = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        ssize_t n = recv(client_sock, buffer, to_read, 0); // Dùng recv thường, không dùng recv_all ở đây để linh hoạt
        
        if (n <= 0) break; // Lỗi hoặc đóng kết nối

        fwrite(buffer, 1, n, fp);
        total_received += n;
        remaining -= n;
    }

    fclose(fp);
    printf("[Server] Saved file to %s. Total received: %ld bytes\n", filepath, total_received);
}

void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg); // Giải phóng vùng nhớ con trỏ arg đã malloc bên server.c (nếu có)

    printf("[Server] Client handler thread started for socket %d\n", client_sock);

    msg_header_t header;
    while (1) {
        // 1. Nhận Header
        ssize_t n = recv_all(client_sock, &header, sizeof(header));
        if (n <= 0) {
            printf("[Server] Client disconnected or error.\n");
            break;
        }

        // 2. Phân loại lệnh (Switch case)
        switch (header.cmd) {
            case CMD_UPLOAD:
                handle_upload_server(client_sock);
                break;
            
            case CMD_LOGIN:
                // TODO: Implement login logic (Nhận user/pass -> Query DB)
                printf("[Server] Login command received.\n");
                // Consume payload if any logic here
                break;

            case CMD_REGISTER:
                // TODO: Implement register logic
                printf("[Server] Register command received.\n");
                break;

            default:
                printf("[Server] Unknown command: %d\n", header.cmd);
                // Nếu header có length > 0, cần đọc bỏ payload để tránh lỗi protocol
                // skip_bytes(client_sock, header.length);
                break;
        }
    }

    close(client_sock);
    return NULL;
}

// Cần thêm hàm start_server implementation (socket, bind, listen) nếu server.c chưa làm kỹ
// Tuy nhiên server.c của bạn có vẻ đã gọi start_server(), hàm này nên nằm ở connection_handler.c
// Dưới đây là khung sườn cho start_server:
void start_server() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    int PORT = 8080; // Define port

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Listening on port %d...\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        printf("[Server] New connection accepted.\n");

        // Tạo thread cho client mới
        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;
        
        if (pthread_create(&tid, NULL, client_thread, pclient) != 0) {
            perror("pthread_create");
            free(pclient);
            close(new_socket);
        } else {
            pthread_detach(tid); // Tự thu hồi tài nguyên khi thread xong
        }
    }
}