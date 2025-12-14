#include "connection_handler.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "../database/database.h"
#include <stdio.h>
#include <sys/stat.h> 
#include <errno.h>
#include "handlers/request_dispatcher.h"
#include <string.h>
#include <unistd.h>
#include <pthread.h>
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

    // Buffer for accumulating received data and process line-by-line
    char inbuf[4096];
    size_t in_len = 0;
    ssize_t n;

    while (1) {
        n = recv(client_sock, inbuf + in_len, sizeof(inbuf) - in_len - 1, 0);
        if (n <= 0) break; // client closed or error
        in_len += (size_t)n;
        inbuf[in_len] = '\0';

        // n -> cmd, arg1, arg2
        

        // process all complete lines
        char *line_start = inbuf;
        char *newline;
        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';
            char *line = line_start;

            // Trim CR if present
            size_t L = strlen(line);
            if (L > 0 && line[L-1] == '\r') line[L-1] = '\0';

            // Handle command
            if (strncmp(line, "REGISTER ", 9) == 0) {
                // If this connection already logged in, disallow creating a new account
                if (current_user[0] != '\0') {
                    const char *resp = "409 Already logged in\n";
                    send(client_sock, resp, strlen(resp), 0);
                } else {
                    char *p = line + 9;
                    char username[128];
                    char password[128];
                    if (sscanf(p, "%127s %127s", username, password) == 2) {
                        char hash_out[512];
                        if (utils_hash_password(password, hash_out, sizeof(hash_out)) != 0) {
                            const char *resp = "500 Hash error\n";
                            send(client_sock, resp, strlen(resp), 0);
                        } else {
                            int r = db_create_user(conn, username, hash_out);
                            if (r == 0) {
                                const char *resp = "201 Register success\n";
                                send(client_sock, resp, strlen(resp), 0);
                            } else if (r == 1) {
                                const char *resp = "409 User exists\n";
                                send(client_sock, resp, strlen(resp), 0);
                            } else {
                                const char *resp = "500 Register error\n";
                                send(client_sock, resp, strlen(resp), 0);
                            }
                        }
                    } else {
                        const char *resp = "400 Bad request\n";
                        send(client_sock, resp, strlen(resp), 0);
                    }
                }
            } else if (strncmp(line, "LOGIN ", 6) == 0) {
                char *p = line + 6;
                char username[128];
                char password[128];
                if (sscanf(p, "%127s %127s", username, password) == 2) {
                    // check credentials
                    int v = db_verify_user(conn, username, password);
                    if (v == 0) {
                        // valid credentials, check already logged in
                        if (is_logged_in(username)) {
                            const char *resp = "409 Already logged in\n";
                            send(client_sock, resp, strlen(resp), 0);
                        } else {
                            if (!add_logged_user(username)) {
                                const char *resp = "500 Login error\n";
                                send(client_sock, resp, strlen(resp), 0);
                            } else {
                                // mark current
                                strncpy(current_user, username, sizeof(current_user)-1);
                                const char *resp = "110 Login success\n";
                                send(client_sock, resp, strlen(resp), 0);
                            }
                        }
                    } else if (v == 1) {
                        const char *resp = "401 Login failed\n";
                        send(client_sock, resp, strlen(resp), 0);
                    } else {
                        const char *resp = "500 Login error\n";
                        send(client_sock, resp, strlen(resp), 0);
                    }
                } else {
                    const char *resp = "400 Bad request\n";
                    send(client_sock, resp, strlen(resp), 0);
                }
            } else {
                const char *resp = "400 Unknown command\n";
                send(client_sock, resp, strlen(resp), 0);
            }

            // move to next line
            line_start = newline + 1;
        }

        // move leftover to beginning of buffer
        size_t remaining = strlen(line_start);
        if (remaining > 0 && line_start != inbuf) {
            memmove(inbuf, line_start, remaining + 1);
            in_len = remaining;
        } else if (line_start == inbuf) {
            // no complete line found yet
            if (in_len == sizeof(inbuf)-1) {
                // buffer full without newline, reset to avoid overflow
                in_len = 0;
                inbuf[0] = '\0';
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
            // no leftover
            in_len = 0;
            inbuf[0] = '\0';
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

void handle_upload(int client_sock) {
    file_data meta;
    
    if (recv_all(client_sock, &meta, sizeof(meta)) <= 0) {
        printf("[Server] Failed to receive metadata.\n");
        return;
    }

    printf("[Server] Receiving file: %s (%lld bytes)\n", meta.filename, meta.filesize);

    struct stat st = {0};
    if (stat("storage", &st) == -1) {
        mkdir("storage", 0700);
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "storage/%s", meta.filename);

}