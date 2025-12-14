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

#define CHUNK_SIZE 4096
#define AUTH_BUF 512

// --- LOGGED USER LIST MANAGEMENT (Giữ nguyên logic bạn bè) ---

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
 // tạo thư mục để người dùng upload file lên
void mkdir_p(const char *path) {
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", path);
    //ghi path vào mảng temp dưới dạng chuỗi, giới hạn kích thước không vượt quá kích thước mảng temp
    
    size_t len = strlen(temp);

    // Duyệt qua từng ký tự của đường dẫn
    for (int i = 0; i < len; i++) {
        // Nếu gặp dấu gạch chéo '/', tức là đã đi hết một cấp thư mục
        if (temp[i] == '/') {
            temp[i] = '\0';         // Cắt chuỗi tại đây (thay '/' bằng NULL)
            mkdir(temp, 0700);      // Tạo thư mục cha này (nếu chưa có)
            temp[i] = '/';          // Trả lại dấu '/' để đi tiếp
        }
    }
    
    mkdir(temp, 0700);
}

/**
 * @function handle_upload_request
 * Processes the file upload logic on server side.
 * * @param client_sock Socket connected to client
 * @param folder Target folder name
 * @param filename Name of the file
 * @param size_str File size as string
 */
 //Hàm này là hàm phía server, dùng để xử lý một lệnh UPLOAD từ client:
void handle_upload_request(int client_sock, const char *folder, const char *filename, const char *size_str) {
    long long filesize = atoll(size_str);
    if (filesize < 0) {
        perform_send_and_log(client_sock, "UPLOAD", "400 Invalid size\r\n");
        return;
    }

    char log_info[1024];
    snprintf(log_info, sizeof(log_info), "UPLOAD %s %s %lld", folder, filename, filesize);
    // (?) tại sao cần ghi log_info ở đây

    // 1. Prepare Storage Path
    char storage_path[1024];
    //tạo đường dẫn thư mục lưu file 
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

    // 2. Notify Client: Ready
    perform_send_and_log(client_sock, log_info, "150 Ready\r\n");

    // 3. Receive Data Loop
    char buffer[CHUNK_SIZE];
    //đọc từng khối dữ liệu trong file để upload 
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

    // 4. Send Final Response
    if (!error && received == filesize) {
        perform_send_and_log(client_sock, "UPLOAD_DATA", "200 Success\r\n");
    } else {
        remove(full_path); // Delete incomplete file
        perform_send_and_log(client_sock, "UPLOAD_DATA", "500 Upload incomplete\r\n");
    }
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
        char cmd[32] = {0}, arg1[256] = {0}, arg2[256] = {0}, arg3[64] = {0};
        // Simple parsing: CMD ARG1 ARG2 ARG3
        sscanf(line, "%31s %255s %255s %63s", cmd, arg1, arg2, arg3);

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
