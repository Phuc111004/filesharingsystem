#include "connection_handler.h"
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
    
    if (recv_exact(client_sock, &meta, sizeof(meta)) <= 0) {
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