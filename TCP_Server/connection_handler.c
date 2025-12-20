#include "connection_handler.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include "../common/utils.h"
#include "../database/database.h"
#include "../database/queries.h"
#include "handlers/request_dispatcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define BACKLOG 20
#define CHUNK_SIZE 4096

// --- In-memory Logged User Registry ---
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
            return 0; // Already present
        }
        it = it->next;
    }
    logged_user_t *node = malloc(sizeof(logged_user_t));
    if (node) {
        strncpy(node->username, username, 127);
        node->username[127] = '\0';
        node->next = logged_head;
        logged_head = node;
        pthread_mutex_unlock(&logged_mutex);
        return 1;
    }
    pthread_mutex_unlock(&logged_mutex);
    return 0;
}

static void remove_logged_user(const char *username) {
    if (!username || username[0] == '\0') return;
    pthread_mutex_lock(&logged_mutex);
    logged_user_t **curr = &logged_head;
    while (*curr) {
        if (strcmp((*curr)->username, username) == 0) {
            logged_user_t *temp = *curr;
            *curr = (*curr)->next;
            free(temp);
            break;
        }
        curr = &(*curr)->next;
    }
    pthread_mutex_unlock(&logged_mutex);
}
void perform_send_and_log(int sock, const char* raw_cmd, const char* resp) {
    if (!resp) return;

    // Timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    // Clean cmd for logging (remove newline)
    char cmd_clean[256];
    if (raw_cmd) {
        strncpy(cmd_clean, raw_cmd, 255);
        cmd_clean[255] = '\0';
        char *nl = strchr(cmd_clean, '\n');
        if (nl) *nl = '\0';
        nl = strchr(cmd_clean, '\r');
        if (nl) *nl = '\0';
    } else {
        strcpy(cmd_clean, "(Internal/NoCmd)");
    }

    // Clean resp for logging
    char resp_clean[256];
    strncpy(resp_clean, resp, 255);
    resp_clean[255] = '\0';
    char *nl = strchr(resp_clean, '\n'); 
    if (nl) *nl = '\0'; 

    printf("[%s] CMD: %-20s | RESP: %s\n", time_str, cmd_clean, resp_clean);

    send_all(sock, resp, strlen(resp));
}

// --- Upload Handler ---
void handle_upload_request(int client_sock, const char *arg_filename, const char *arg_filesize) {
    long long filesize = atoll(arg_filesize);
    char filename[256];
    strncpy(filename, arg_filename, 255);
    filename[255] = '\0';
    
    char log_info[512];
    snprintf(log_info, sizeof(log_info), "UPLOAD %s %lld", filename, filesize);

    // 1. Send Ready
    char ready_msg[64];
    snprintf(ready_msg, sizeof(ready_msg), "%d Ready\n", RES_UPLOAD_READY);
    perform_send_and_log(client_sock, log_info, ready_msg);

    // 2. Prepare Storage
    struct stat st = {0};
    if (stat("storage", &st) == -1) {
        mkdir("storage", 0700);
    }
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "storage/%s", filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[Server] File open error");
        return; 
    }

    // 3. Receive Loop
    char buffer[CHUNK_SIZE];
    long long remaining = filesize;
    long long total_received = 0;
    while (remaining > 0) {
        size_t to_read = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        ssize_t n = recv(client_sock, buffer, to_read, 0);
        if (n <= 0) break; // Error or disconnect
        fwrite(buffer, 1, n, fp);
        remaining -= n;
        total_received += n;
    }
    fclose(fp);

    // 4. Send Success/Fail
    if (total_received == filesize) {
        char succ_msg[64];
        snprintf(succ_msg, sizeof(succ_msg), "%d Upload success\n", RES_SUCCESS);
        perform_send_and_log(client_sock, "UPLOAD_DATA_STREAM", succ_msg);
    } else {
        char fail_msg[64];
        snprintf(fail_msg, sizeof(fail_msg), "%d Upload failed\n", RES_UPLOAD_FAILED);
        perform_send_and_log(client_sock, "UPLOAD_DATA_STREAM", fail_msg);
    }
}

// --- Client Thread ---
void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    pthread_detach(pthread_self());

    printf("[Server] Client connected: socket %d\n", client_sock);

    MYSQL *conn = db_connect();
    if (!conn) {
        const char *msg = "500 DB Error\n";
        send_all(client_sock, msg, strlen(msg));
        close(client_sock);
        return NULL;
    }

    char current_user[128] = {0};
    int current_user_id = -1;
    char buffer[4096];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = recv(client_sock, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) break;

        char raw_cmd_log[4096];
        strncpy(raw_cmd_log, buffer, sizeof(raw_cmd_log)-1);
        raw_cmd_log[sizeof(raw_cmd_log)-1] = '\0';

        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        if (buffer[strlen(buffer)-1] == '\r') buffer[strlen(buffer)-1] = '\0';

        char cmd[32] = {0};
        char arg1[128] = {0};
        char arg2[128] = {0};
        
        sscanf(buffer, "%s", cmd);

        if (strcmp(cmd, STR_REGISTER) == 0) {
             sscanf(buffer, "%*s %s %s", arg1, arg2);
             if (current_user_id != -1) {
                 perform_send_and_log(client_sock, raw_cmd_log, "409 Already logged in\n");
             } else {
                 char hash[512];
                 utils_hash_password(arg2, hash, sizeof(hash));
                 int r = db_create_user(conn, arg1, hash);
                 if (r == 0) perform_send_and_log(client_sock, raw_cmd_log, "201 Register success\n");
                 else if (r == 1) perform_send_and_log(client_sock, raw_cmd_log, "409 User exists\n");
                 else perform_send_and_log(client_sock, raw_cmd_log, "500 Register error\n");
             }
        } 
        else if (strcmp(cmd, STR_LOGIN) == 0) {
            sscanf(buffer, "%*s %s %s", arg1, arg2);
            if (current_user_id != -1) {
                perform_send_and_log(client_sock, raw_cmd_log, "409 Already logged in\n");
            } else {
                int res = db_verify_user(conn, arg1, arg2);
                if (res == 0) { 
                     if (is_logged_in(arg1)) {
                         perform_send_and_log(client_sock, raw_cmd_log, "409 Already logged in elsewhere\n");
                     } else {
                         add_logged_user(arg1);
                         strncpy(current_user, arg1, 127);
                         current_user_id = db_get_user_id_by_name(conn, arg1);
                         perform_send_and_log(client_sock, raw_cmd_log, "110 Login success\n");
                     }
                } else {
                    perform_send_and_log(client_sock, raw_cmd_log, "401 Login failed\n");
                }
            }
        }
        else if (strcmp(cmd, STR_LOGOUT) == 0) {
            if (current_user_id != -1) {
                remove_logged_user(current_user);
                current_user_id = -1;
                memset(current_user, 0, sizeof(current_user));
            }
            perform_send_and_log(client_sock, raw_cmd_log, "202 Logout success\n");
        }
        else if (strcmp(cmd, STR_UPLOAD) == 0) {
            char group_id_str[32], path[256], filename[256], size_str[32];
            int parsed = sscanf(buffer, "%*s %s %s %s %s", group_id_str, path, filename, size_str);
            if (parsed == 4) {
                 handle_upload_request(client_sock, filename, size_str);
            } else {
                 perform_send_and_log(client_sock, raw_cmd_log, "400 Bad Request\n");
            }
        }
        else {
            char response[4096] = {0};
            if (current_user_id == -1) {
                perform_send_and_log(client_sock, raw_cmd_log, "403 Login required\n");
            } else {
                dispatch_request(conn, current_user_id, buffer, response);
                strcat(response, "\n"); 
                perform_send_and_log(client_sock, raw_cmd_log, response);
            }
        }
    }

    if (current_user_id != -1) {
        remove_logged_user(current_user);
    }
    db_close(conn);
    close(client_sock);
    printf("[Server] Client disconnected: socket %d\n", client_sock);
    return NULL;
}

void start_server(int port) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

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
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Ready to accept connections on port %d...\n", port);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, pclient) != 0) {
             perror("pthread_create");
             free(pclient);
             close(new_socket);
        }
    }
}