#include "connection_handler.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define PORT 5500
#define BACKLOG 20

void start_server() {
// TODO: implement socket(), bind(), listen(), accept() loop
// For each accepted client create a pthread running client_thread
    int listen_sock, *conn_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    pthread_t tid;

    // 1. Tạo socket (Lec03 slide 7)
    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Tùy chọn: Cho phép dùng lại cổng ngay lập tức sau khi tắt server
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Cấu hình địa chỉ server (Lec03 slide 18)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Lắng nghe mọi IP
    server_addr.sin_port = htons(PORT);              // Cổng 8080

    // 3. Bind (Gắn địa chỉ vào socket)
    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    // 4. Listen (Chuyển sang trạng thái chờ kết nối)
    if (listen(listen_sock, BACKLOG) == -1) {
        perror("Listen failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    printf("[server] Server started on port %d. Waiting for connections...\n", PORT);

    // 5. Vòng lặp chấp nhận kết nối (Accept Loop)
    while (1) {
        client_addr_len = sizeof(client_addr);
        conn_sock = malloc(sizeof(int)); // Cấp phát vùng nhớ mới cho socket ID để truyền vào thread

        // Chấp nhận kết nối (Block until connect)
        *conn_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (*conn_sock == -1) {
            perror("Accept failed");
            free(conn_sock);
            continue;
        }

        // In thông tin client (Lec03 slide 20 - inet_ntop)
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("[server] Accepted connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // 6. Tạo luồng mới xử lý client (Multi-thread Server)
        if (pthread_create(&tid, NULL, client_thread, conn_sock) != 0) {
            perror("Pthread create failed");
            free(conn_sock);
        }
    }

    close(listen_sock);
}


void* client_thread(void* arg) {
// TODO: receive requests, parse protocol, call DB functions, send responses
    int client_sock = *(int*)arg;
    free(arg); // Giải phóng bộ nhớ đã malloc bên start_server
    
    // Detach luồng để tự thu hồi tài nguyên khi kết thúc (L05_06_ltm.pdf trang 17)
    pthread_detach(pthread_self());

    printf("[server] Client connected on socket %d\n", client_sock);

    // TODO: Gọi các hàm xử lý login, upload, download ở đây
    // Ví dụ demo: Gửi lời chào
    char *msg = "Hello from Server!\n";
    send(client_sock, msg, strlen(msg), 0);

    // Đóng socket khi xong việc
    close(client_sock);
    printf("[server] Client on socket %d disconnected.\n", client_sock);
    return NULL;
}
