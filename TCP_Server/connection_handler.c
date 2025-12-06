#include "connection_handler.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
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

#define PORT 5500
#define BACKLOG 20

void start_server() {
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
    server_addr.sin_port = htons(PORT);

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

    printf("[server] Server started on port %d. Waiting for connections...\n", PORT);

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

    // Buffer for accumulating received data and process line-by-line
    char inbuf[4096];
    size_t in_len = 0;
    ssize_t n;

    while (1) {
        n = recv(client_sock, inbuf + in_len, sizeof(inbuf) - in_len - 1, 0);
        if (n <= 0) break; // client closed or error
        in_len += (size_t)n;
        inbuf[in_len] = '\0';

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

    db_close(conn);
    close(client_sock);
    printf("[server] Client on socket %d disconnected.\n", client_sock);
    return NULL;
}
