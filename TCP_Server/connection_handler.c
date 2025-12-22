#include "connection_handler.h"
#include "../common/protocol.h"
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
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>

#define CHUNK_SIZE 64000

typedef struct logged_user {
    char username[128];
    struct logged_user *next;
} logged_user_t;

logged_user_t *logged_head = NULL;
pthread_mutex_t logged_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @function is_logged_in
 * Checks if a user is currently logged in.
 *
 * @param username The username to check
 * @return int 1 if logged in, 0 otherwise
 */
int is_logged_in(const char *username) {
    if (!username || !*username) return 0;
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

/**
 * @function add_logged_user
 * Adds a user to the active logged-in list.
 * (Note: Logic kept as requested)
 *
 * @param username The username to add
 * @return int 1 on success, 0 if already exists
 */
static int add_logged_user(const char *username) {
    if (!username || !*username) return 0;
    pthread_mutex_lock(&logged_mutex);
    logged_user_t *it = logged_head;
    while (it) {
        if (strcmp(it->username, username) == 0) {
            pthread_mutex_unlock(&logged_mutex);
            return 0;
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

/**
 * @function remove_logged_user
 * Removes a user from the active logged-in list.
 * (Note: Logic kept as requested)
 * * @param username The username to remove
 */
static void remove_logged_user(const char *username) {
    if (!username || !*username) return;
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

/**
 * @function perform_send_and_log
 * Logs the command and response to console, then sends response to client.
 * (Note: Logic kept as requested)
 *
 * @param sock Client socket
 * @param raw_cmd The command received (for logging)
 * @param resp The response message to send
 */
void perform_send_and_log(int sock, const char* raw_cmd, const char* resp) {
    if (!resp) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    char cmd_clean[256];
    if (raw_cmd) {
        strncpy(cmd_clean, raw_cmd, 255);
        cmd_clean[255] = '\0';
        char *nl = strchr(cmd_clean, '\n'); if (nl) *nl = '\0';
        nl = strchr(cmd_clean, '\r'); if (nl) *nl = '\0';
    } else {
        strcpy(cmd_clean, "(Internal)");
    }

    char resp_clean[256];
    strncpy(resp_clean, resp, 255);
    resp_clean[255] = '\0';
    char *nl = strchr(resp_clean, '\n'); if (nl) *nl = '\0';

    printf("[%s] CMD: %-20s | RESP: %s\n", time_str, cmd_clean, resp_clean);
    send(sock, resp, strlen(resp), 0);
}

/**
 * @function client_thread
 * Main thread function to handle a single client connection.
 * * @param arg Pointer to the client socket descriptor
 * @return void*
 */
void* client_thread(void* arg) {
    int sock = *(int*)arg;
    free(arg);

    MYSQL *conn = db_connect();
    if (!conn) {
        send(sock, "500 DB Error\r\n", 14, 0);
        close(sock);
        return NULL;
    }

    char line[4096];
    char current_user[128] = {0};
    int user_id = -1;

    // Command Processing Loop
    while (recv_line(sock, line, sizeof(line)) > 0) {
        char cmd[32] = {0}, arg1[256] = {0}, arg2[256] = {0}, arg3[256] = {0};
        parse_command_line(line, cmd, sizeof(cmd), arg1, sizeof(arg1), arg2, sizeof(arg2), arg3, sizeof(arg3));

        // --- AUTH COMMANDS ---
        if (strcmp(cmd, STR_LOGIN) == 0) {
            if (user_id != -1) {
                perform_send_and_log(sock, line, "409 Already logged in\r\n");
            } else if (db_verify_user(conn, arg1, arg2) == 0) { // Verify pass
                if (is_logged_in(arg1)) {
                    perform_send_and_log(sock, line, "409 User logged in elsewhere\r\n");
                } else {
                    add_logged_user(arg1);
                    strncpy(current_user, arg1, 127);
                    user_id = db_get_user_id_by_name(conn, arg1);
                    perform_send_and_log(sock, line, "110 Login success\r\n");
                }
            } else {
                perform_send_and_log(sock, line, "401 Login failed\r\n");
            }
        } 
        else if (strcmp(cmd, STR_REGISTER) == 0) {
            char hash[512];
            utils_hash_password(arg2, hash, sizeof(hash));
            int res = db_create_user(conn, arg1, hash);
            if (res == 0) perform_send_and_log(sock, line, "201 Register success\r\n");
            else if (res == 1) perform_send_and_log(sock, line, "409 User exists\r\n");
            else perform_send_and_log(sock, line, "500 Register error\r\n");
        }
        else if (strcmp(cmd, STR_LOGOUT) == 0) {
            if (user_id != -1) {
                remove_logged_user(current_user);
                user_id = -1;
                memset(current_user, 0, sizeof(current_user));
            }
            perform_send_and_log(sock, line, "202 Logout success\r\n");
        }
        // --- FEATURE COMMANDS ---
        else if (strcmp(cmd, STR_UPLOAD) == 0) {
            handle_upload_request(sock, arg1, arg2, arg3);
        }
        else {
            // Commands requiring login
            if (user_id == -1) {
                perform_send_and_log(sock, line, "403 Login required\r\n");
            } else {
                char resp[4096] = {0};
                dispatch_request(conn, user_id, line, resp);
                strcat(resp, "\r\n");
                perform_send_and_log(sock, line, resp);
            }
        }
    }

    // Cleanup on disconnect
    if (user_id != -1) remove_logged_user(current_user);
    db_close(conn);
    close(sock);
    return NULL;
}

/**
 * @function recv_line
 * Helper to receive a line from socket.
 */
int recv_line(int sockfd, char *buf, size_t maxlen) {
    size_t i = 0;
    while (i < maxlen - 1) {
        char c;
        if (recv(sockfd, &c, 1, 0) <= 0) break;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (int)i;
}

/**
 * @function mkdir_p
 * Creates a directory path recursively.
 * Example: "storage/data/img" -> creates "storage", then "storage/data", then "storage/data/img"
 * * @param path The directory path to create
 */
void mkdir_p(const char *path) {
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", path);
    //ghi path vào mảng temp dưới dạng chuỗi, giới hạn kích thước không vượt quá kích thước mảng temp
    
    size_t len = strlen(temp);

    for (int i = 0; i < len; i++) {
        if (temp[i] == '/') {
            temp[i] = '\0';         
            mkdir(temp, 0700);      
            temp[i] = '/';          
        }
    }
    
    mkdir(temp, 0700);
}

void parse_command_line(const char *line,
                               char *cmd, size_t cmd_sz,
                               char *arg1, size_t arg1_sz,
                               char *arg2, size_t arg2_sz,
                               char *arg3, size_t arg3_sz)
{
    if (!line) { cmd[0]=arg1[0]=arg2[0]=arg3[0]='\0'; return; }

    sscanf(line, "%s %s %s %s", cmd, arg1, arg2, arg3);
}

/**
 * @function handle_upload_request
 * Processes the file upload logic on server side.
 * * @param client_sock Socket connected to client
 * @param folder Target folder name
 * @param filename Name of the file
 * @param size_str File size as string
 */
void handle_upload_request(int client_sock, const char *folder, const char *filename, const char *size_str) {
    long long filesize = atoll(size_str);
    if (filesize < 0) {
        perform_send_and_log(client_sock, "UPLOAD", "400 Invalid size\r\n");
        return;
    }

    char log_info[1024];
    snprintf(log_info, sizeof(log_info), "UPLOAD %s %s %lld", folder, filename, filesize);

    // khai báo mảng để lưu trữ đường dẫn thư mục
    char storage_path[1024];
    if (!folder || strcmp(folder, ".") == 0 || strlen(folder) == 0) {
        snprintf(storage_path, sizeof(storage_path), "storage");
    } else {
        snprintf(storage_path, sizeof(storage_path), "storage/%s", folder);
    }
    mkdir_p(storage_path);

    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s/%s", storage_path, filename);

    FILE *fp = fopen(full_path, "wb");
    if (!fp) {
        perform_send_and_log(client_sock, log_info, "500 Create file failed\r\n");
        return;
    }

    perform_send_and_log(client_sock, log_info, "150 Ready\r\n");

    // 3. Receive Data Loop
    char buffer[CHUNK_SIZE];
    long long received = 0;
    int error = 0;

    while (received < filesize) {
        size_t to_read = CHUNK_SIZE;

        if (filesize - received < CHUNK_SIZE) {
            to_read = (size_t)(filesize - received);
        }
        ssize_t n = recv(client_sock, buffer, to_read, 0);
        if (n <= 0) {
            error = 1;
            break;
        }
        fwrite(buffer, 1, n, fp);
        received += n;
    }
    fclose(fp);
    //xử lý lại recv_line() (truyền dòng)
    //upload file thì phải có ràng buộc, theo đúng giao thức - vào nhóm nào, thư mục nào
    //cấp phát động (CHUNK-SIZE) thay vì cấp phát tĩnh

    //có kiểu thông điệp cần login db, kiểu nào không cần
    // 4. Send Final Response
    if (!error && received == filesize) {
        perform_send_and_log(client_sock, "UPLOAD_DATA", "200 Success\r\n");
    } else {
        remove(full_path);
        perform_send_and_log(client_sock, "UPLOAD_DATA", "500 Upload incomplete\r\n");
    }
}

/**
 * @function start_server
 * Sets up the TCP server socket and accepts incoming connections.
 * * @param port The port number to listen on
 */
void start_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 20) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int new_sock = accept(server_fd, (struct sockaddr *)&client_addr, &len);
        if (new_sock < 0) {
            perror("Accept error");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("New connection: %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // Create thread
        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = new_sock;
        if (pthread_create(&tid, NULL, client_thread, pclient) != 0) {
            perror("Thread creation failed");
            free(pclient);
            close(new_sock);
        } else {
            pthread_detach(tid);
        }
    }
}