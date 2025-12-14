#include "connection_handler.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mysql/mysql.h>
#include "../database/database.h"
#include "../database/queries.h"
#include "../common/utils.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"

#define BACKLOG 20

// Simple in-memory registry of logged-in usernames to prevent multiple simultaneous logins
typedef struct logged_user {
    char username[128];
    struct logged_user *next;
} logged_user_t;

static logged_user_t *logged_head = NULL;
static pthread_mutex_t logged_mutex = PTHREAD_MUTEX_INITIALIZER;

static int is_logged_in(const char *username) {
    if (!username || username[0] == '\0') return 0;
    pthread_mutex_lock(&logged_mutex);
    logged_user_t *it = logged_head;
    while (it) {
        if (strcmp(it->username, username) == 0) {
            pthread_mutex_unlock(&logged_mutex);
            return 1;
        }
        it = it->next;
    }
    pthread_mutex_unlock(&logged_mutex);
    return 0;
}

static int add_logged_user(const char *username) {
    if (!username || username[0] == '\0') return 0;
    pthread_mutex_lock(&logged_mutex);
    logged_user_t *it = logged_head;
    while (it) {
        if (strcmp(it->username, username) == 0) {
            pthread_mutex_unlock(&logged_mutex);
            return 0; // already present
        }
        it = it->next;
    }
    logged_user_t *node = malloc(sizeof(logged_user_t));
    if (!node) {
        pthread_mutex_unlock(&logged_mutex);
        return 0;
    }
    strncpy(node->username, username, sizeof(node->username));
    node->username[sizeof(node->username)-1] = '\0';
    node->next = logged_head;
    logged_head = node;
    pthread_mutex_unlock(&logged_mutex);
    return 1;
}

static void remove_logged_user(const char *username) {
    if (!username || username[0] == '\0') return;
    pthread_mutex_lock(&logged_mutex);
    logged_user_t **p = &logged_head;
    while (*p) {
        if (strcmp((*p)->username, username) == 0) {
            logged_user_t *to_free = *p;
            *p = to_free->next;
            free(to_free);
            break;
        }
        p = &((*p)->next);
    }
    pthread_mutex_unlock(&logged_mutex);
}

void start_server(int port) {
    int listen_sock, *conn_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    pthread_t tid;

    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(listen_sock, BACKLOG) == -1) {
        perror("Listen failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    printf("[server] Server started on port %d. Waiting for connections...\n", port);

    while (1) {
        client_addr_len = sizeof(client_addr);
        conn_sock = malloc(sizeof(int));
        *conn_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (*conn_sock == -1) {
            perror("Accept failed");
            free(conn_sock);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("[server] Accepted connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        if (pthread_create(&tid, NULL, client_thread, conn_sock) != 0) {
            perror("Pthread create failed");
            free(conn_sock);
        }
    }

    close(listen_sock);
}


void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    pthread_detach(pthread_self());

    printf("[server] Client connected on socket %d\n", client_sock);

    // Open a DB connection for this thread
    MYSQL *conn = db_connect();
    if (!conn) {
        const char *resp = "500 DB connection error\n";
        send(client_sock, resp, strlen(resp), 0);
        close(client_sock);
        printf("[server] Client on socket %d disconnected (db error).\n", client_sock);
        return NULL;
    }

    // track current logged-in username for this connection (if any)
    char current_user[128] = {0};

    msg_header_t header;

    while (1) {
        // 1. Luôn đọc Header (8 bytes) trước
        // Đây là điểm chờ (blocking) an toàn mới thay cho recv text cũ
        if (recv_all(client_sock, &header, sizeof(header)) <= 0) {
            break; // Client ngắt kết nối
        }

        // Chuyển đổi byte order từ Mạng -> Máy
        int cmd = ntohl(header.cmd);
        int len = ntohl(header.length);

        // 2. Xử lý từng loại lệnh
        switch (cmd) {
            case CMD_UPLOAD:
                // Logic mới: Gọi hàm upload xử lý luồng
                handle_upload(client_sock, len);
                break;

            case CMD_REGISTER:
            case CMD_LOGIN:
                // --- BẢO TỒN LOGIC CŨ Ở ĐÂY ---
                // Client sẽ gửi body là chuỗi text "username password"
                // Ta cần đọc hết body này vào buffer rồi mới xử lý
                if (len > 0) {
                    char *body = malloc(len + 1);
                    if (!body) break; // Lỗi bộ nhớ

                    // Đọc chính xác phần text "user pass"
                    if (recv_all(client_sock, body, len) > 0) {
                        body[len] = '\0'; // Đảm bảo thành chuỗi hợp lệ

                        char username[128];
                        char password[128];

                        // Parse dữ liệu (Logic cũ của bạn)
                        if (sscanf(body, "%127s %127s", username, password) == 2) {
                            
                            if (cmd == CMD_REGISTER) {
                                // === LOGIC REGISTER CŨ ===
                                if (current_user[0] != '\0') {
                                    const char *resp = "409 Already logged in\n";
                                    send_all(client_sock, resp, strlen(resp));
                                } else {
                                    char hash_out[512];
                                    if (utils_hash_password(password, hash_out, sizeof(hash_out)) != 0) {
                                        const char *resp = "500 Hash error\n";
                                        send_all(client_sock, resp, strlen(resp));
                                    } else {
                                        int r = db_create_user(conn, username, hash_out);
                                        if (r == 0) {
                                            const char *resp = "201 Register success\n";
                                            send_all(client_sock, resp, strlen(resp));
                                        } else if (r == 1) {
                                            const char *resp = "409 User exists\n";
                                            send_all(client_sock, resp, strlen(resp));
                                        } else {
                                            const char *resp = "500 Register error\n";
                                            send_all(client_sock, resp, strlen(resp));
                                        }
                                    }
                                }
                            } 
                            else if (cmd == CMD_LOGIN) {
                                // === LOGIC LOGIN CŨ ===
                                int v = db_verify_user(conn, username, password);
                                if (v == 0) {
                                    if (is_logged_in(username)) {
                                        const char *resp = "409 Already logged in\n";
                                        send_all(client_sock, resp, strlen(resp));
                                    } else {
                                        if (!add_logged_user(username)) {
                                            const char *resp = "500 Login error\n";
                                            send_all(client_sock, resp, strlen(resp));
                                        } else {
                                            strncpy(current_user, username, sizeof(current_user)-1);
                                            const char *resp = "110 Login success\n";
                                            send_all(client_sock, resp, strlen(resp));
                                        }
                                    }
                                } else if (v == 1) {
                                    const char *resp = "401 Login failed\n";
                                    send_all(client_sock, resp, strlen(resp));
                                } else {
                                    const char *resp = "500 Login error\n";
                                    send_all(client_sock, resp, strlen(resp));
                                }
                            }
                        } else {
                            const char *resp = "400 Bad request format\n";
                            send_all(client_sock, resp, strlen(resp));
                        }
                    }
                    free(body);
                }
                break;

            default:
                printf("[Server] Unknown CMD: %d. Skipping %d bytes.\n", cmd, len);
                // Rất quan trọng: Nếu gặp lệnh lạ, vẫn phải đọc bỏ phần body 
                // để không bị lỗi lệnh tiếp theo
                if (len > 0) {
                    char *junk = malloc(len);
                    recv_all(client_sock, junk, len);
                    free(junk);
                }
                break;
        }
    }

    // cleanup logged-in state if necessary
    if (current_user[0] != '\0') {
        remove_logged_user(current_user);
    }

    db_close(conn);
    close(client_sock);
    printf("[server] Client on socket %d disconnected.\n", client_sock);
    return NULL;
}

// Hàm xử lý upload chuẩn Streaming (Chunking)
void handle_upload(int client_sock, int payload_len) {
    file_data meta;
    int meta_len = sizeof(file_data);

    // 1. Nhận Metadata (Tên file, kích thước...)
    // Lưu ý: payload_len bao gồm cả [Metadata] + [Nội dung file]
    if (recv_all(client_sock, &meta, meta_len) <= 0) {
        printf("[Server] Failed to receive file metadata.\n");
        return;
    }

    printf("[Server] Uploading file: %s (%lld bytes)\n", meta.filename, meta.filesize);

    // Tạo thư mục storage nếu chưa có
    struct stat st = {0};
    if (stat("storage", &st) == -1) mkdir("storage", 0700);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "storage/%s", meta.filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[Server] Cannot open file for writing");
        // Thực tế cần đọc xả hết socket buffer để tránh lỗi protocol, nhưng tạm thời return
        return;
    }

    // 2. Nhận nội dung file (Binary Stream)
    long long left = meta.filesize;
    char buffer[4096];
    
    while (left > 0) {
        int to_read = (left > sizeof(buffer)) ? sizeof(buffer) : left;
        
        // Đọc chính xác to_read bytes
        if (recv_all(client_sock, buffer, to_read) <= 0) break;
        
        fwrite(buffer, 1, to_read, fp);
        left -= to_read;
    }

    fclose(fp);
    printf("[Server] Upload complete: %s\n", filepath);
}