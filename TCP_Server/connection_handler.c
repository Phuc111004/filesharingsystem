#include "connection_handler.h"
#include "../common/protocol.h"
#include "../common/utils.h"
#include "../common/file_utils.h"
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

#define AUTH_BUF 512
#define GROUP_BUF 512
#define LINE_MAX 8192
#define BUFFER_SIZE 4096
#define STORAGE_ROOT "./storage"

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
 * @param username The username of the client (for logging)
 */
void perform_send_and_log(int sock, const char* raw_cmd, const char* resp, const char* username) {
    if (!resp) return;
    //không nhận được response từ server thì không log
    time_t now = time(NULL);
    //giá trị của time_t là số giây kể từ epoch
    //vì sao truyền NULL?
    //
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    //time_str[]: buffer dùng để lưu chuỗi thời gian sau khi format
    //Chuyển thời gian từ struct tm t sang chuỗi ký tự theo định dạng mong muốn
    char cmd_clean[256];

    if (raw_cmd != NULL) {
        // Sao chép command từ client
        strncpy(cmd_clean, raw_cmd, sizeof(cmd_clean) - 1);
        cmd_clean[sizeof(cmd_clean) - 1] = '\0';

        // Loại bỏ ký tự xuống dòng nếu có
        char *pos;
        pos = strchr(cmd_clean, '\n');
        if (pos) *pos = '\0';

        pos = strchr(cmd_clean, '\r');
        if (pos) *pos = '\0';
    } else {
        strcpy(cmd_clean, "Error: NULL command");
    }

    char resp_clean[256];

    strncpy(resp_clean, resp, sizeof(resp_clean) - 1);
    resp_clean[sizeof(resp_clean) - 1] = '\0';

    char *pos = strchr(resp_clean, '\n');
    if (pos) *pos = '\0';

    printf("[%s] CMD: %-20s | RESP: %s\n",
           time_str, cmd_clean, resp_clean);

    if (send_all(sock, resp, strlen(resp)) <= 0) {
        perror("send_all response failed");
    }

    const char *logfile = "server_log.txt";

    FILE *f = fopen(logfile, "a");
    if (f != NULL) {
        const char *user = (username && username[0] != '\0') ? username : "-";

        fprintf(f, "[%s] user=%s $ %s $ %s\n",
                time_str, user, cmd_clean, resp_clean);
        fclose(f);
    } else {
        perror("Cannot open server log file");
    }
}

// --- HELPER FUNCTIONS ---

void parse_command_line(const char *line, char *cmd, size_t cmd_sz, char *arg1, size_t arg1_sz, char *arg2, size_t arg2_sz, char *arg3, size_t arg3_sz) {
    if (!line) { cmd[0]=arg1[0]=arg2[0]=arg3[0]='\0'; return; }
    sscanf(line, "%s %s %s %s", cmd, arg1, arg2, arg3);
}

// mkdir_p is now in common/utils.c

/**
 * @function handle_upload_request
 * Xử lý Upload file an toàn với Stream Processing
 * (Đã cập nhật để nhận residue_data)
 */
 /**
 * @function handle_upload_request
 * Processes the file upload logic on server side.
 * * @param client_sock Socket connected to client
 * @param folder Target folder name
 * @param filename Name of the file
 * @param size_str File size as string
 */

void handle_upload_request(int client_sock, const char *folder, const char *filename, const char *size_str, const char *residue_data, size_t residue_len, const char *username, MYSQL *db_conn, int user_id) {
    long long filesize = atoll(size_str);
    if (filesize < 0) {
        perform_send_and_log(client_sock, "UPLOAD", "400 Invalid size\r\n", username);
        return;
    }

    char log_info[1024];
    snprintf(log_info, sizeof(log_info), "UPLOAD %s %s %lld", folder, filename, filesize);

    // Parse folder as "group_id/parent_id" or fallback to legacy folder name
    int group_id = -1, parent_id = 0;
    char group_name[256] = {0};
    
    if (folder && strchr(folder, '/')) {
        // New format: "group_id/parent_id"
        sscanf(folder, "%d/%d", &group_id, &parent_id);
    } else {
        // Legacy format: folder name - try to resolve to group
        if (!folder || strcmp(folder, ".") == 0 || strlen(folder) == 0) {
            perform_send_and_log(client_sock, log_info, "400 Group required\r\n", username);
            return;
        }
        strncpy(group_name, folder, sizeof(group_name) - 1);
        // For now, assume folder name is group name and parent_id = 0 (root)
        // In a real implementation, you'd resolve this properly
    }

    // Determine storage path
    char storage_path[1024];
    if (group_id > 0) {
        db_get_group_name(db_conn, group_id, group_name, sizeof(group_name));
        snprintf(storage_path, sizeof(storage_path), "storage/%s", group_name);
    } else {
        snprintf(storage_path, sizeof(storage_path), "storage/%s", group_name);
    }
    mkdir_p(storage_path);

    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s/%s", storage_path, filename);

    FILE *fp = fopen(full_path, "wb");
    if (!fp) {
        perform_send_and_log(client_sock, log_info, "500 Create file failed\r\n", username);
        return;
    }

    // Gửi phản hồi sẵn sàng (dùng mã số define trong protocol.h hoặc hardcode tạm)
    // Giả sử RES_UPLOAD_READY là 150
    perform_send_and_log(client_sock, log_info, "150 Ready\r\n", username);

    long long received = 0;
    int error = 0;

    // 1. Ghi dữ liệu thừa (Residue) từ buffer trước (nếu có)
    if (residue_len > 0) {
        size_t to_write = residue_len;
        if (received + to_write > filesize) {
            to_write = filesize - received;
        }
        fwrite(residue_data, 1, to_write, fp);
        received += to_write;
    }

    // 2. Vòng lặp nhận tiếp dữ liệu còn thiếu (Cấp phát động)
    char *buffer = (char *)malloc(CHUNK_SIZE);
    if (!buffer) {
        perror("Malloc failed");
        fclose(fp);
        return;
    }

    while (received < filesize) {
        size_t to_read = CHUNK_SIZE;
        if (filesize - received < CHUNK_SIZE) to_read = (size_t)(filesize - received);
        
        ssize_t n = recv(client_sock, buffer, to_read, 0);
        if (n <= 0) {
            error = 1;
            break;
        }
        fwrite(buffer, 1, n, fp);
        received += n;
    }
    free(buffer);
    fclose(fp);

    if (!error && received == filesize) {
        // SUCCESS: Save file metadata to database
        if (group_id > 0) {
            printf("[DEBUG] Saving to DB: group_id=%d, filename=%s, size=%lld, user_id=%d, parent_id=%d\n", 
                   group_id, filename, filesize, user_id, parent_id);
            int db_result = db_add_file(db_conn, group_id, filename, full_path, filesize, user_id, parent_id);
            if (db_result != 0) {
                printf("[WARNING] File uploaded but database save failed: %s\n", mysql_error(db_conn));
            } else {
                printf("[SUCCESS] File metadata saved to database\n");
            }
        } else {
            printf("[WARNING] group_id <= 0, not saving to database\n");
        }
        perform_send_and_log(client_sock, "UPLOAD_DATA", "200 Upload success\r\n", username);
    } else {
        remove(full_path);
        perform_send_and_log(client_sock, "UPLOAD_DATA", "501 Upload incomplete\r\n", username);
    }
}

/**
 * @function handle_download_request
 * Xử lý Download file với Database integration
 */
void handle_download_request(int client_sock, const char *folder, const char *filename, const char *username, MYSQL *db_conn, int user_id) {
    // Parse folder as "group_id/parent_id" or fallback to legacy folder name
    int group_id = -1, parent_id = 0;
    char group_name[256] = {0};
    
    if (folder && strchr(folder, '/')) {
        // New format: "group_id/parent_id"
        sscanf(folder, "%d/%d", &group_id, &parent_id);
        db_get_group_name(db_conn, group_id, group_name, sizeof(group_name));
    } else {
        // Legacy format: folder name
        if (!folder || strcmp(folder, ".") == 0 || strlen(folder) == 0) {
            perform_send_and_log(client_sock, "DOWNLOAD", "400 Group required\r\n", username);
            return;
        }
        strncpy(group_name, folder, sizeof(group_name) - 1);
    }

    // Check if user has access to the group
    if (group_id > 0 && !db_is_group_member(db_conn, user_id, group_id) && !db_is_group_admin(db_conn, user_id, group_id)) {
        perform_send_and_log(client_sock, "DOWNLOAD", "403 Access denied\r\n", username);
        return;
    }

    // Build file path
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s", STORAGE_ROOT, group_name, filename);

    // Verify file exists in database (optional but recommended)
    // For now, we'll just check filesystem existence

    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "404 File not found: %s\r\n", filename);
        perform_send_and_log(client_sock, "DOWNLOAD", error_msg, username);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char header[256];
    snprintf(header, sizeof(header), "102 %lld\r\n", filesize);
    if (send_all(client_sock, header, strlen(header)) < 0) {
        perror("send_all header failed");
        fclose(fp);
        return;
    }

    printf("[Server] Sending file: %s (%lld bytes)\n", filename, filesize);

    char *buffer = (char *)malloc(CHUNK_SIZE);
    if (!buffer) {
        perror("malloc failed");
        fclose(fp);
        return;
    }

    size_t n;
    while ((n = fread(buffer, 1, CHUNK_SIZE, fp)) > 0) {
        if (send_all(client_sock, buffer, n) < 0) {
            perror("send_all failed");
            break;
        }
    }

    free(buffer);
    fclose(fp);

    perform_send_and_log(client_sock, "DOWNLOAD", "200 File sent successfully\r\n", username);
    printf("[Server] File sent successfully.\n");
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
        send(sock, "500 DB Error\r\n", 14, 0); //
        close(sock);
        return NULL;
    }

    char recvbuf[BUFFER_SIZE];
    char acc[LINE_MAX];

    size_t acc_len = 0; //số byte hiện có trong acc
    acc[0] = '\0';

    char current_user[128] = {0};
    int user_id = -1;

    while (1) {
        ssize_t received = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);
        if (received <= 0) break;

        if (acc_len + received >= sizeof(acc) - 1) {
            acc_len = 0; acc[0] = '\0';
            perform_send_and_log(sock, "UNKNOWN", "300 Command too long\r\n", current_user);
            continue;
        }

        memcpy(acc + acc_len, recvbuf, received);
        acc_len += received;
        acc[acc_len] = '\0';

        char *line_start = acc;
        char *eol;

        while ((eol = strstr(line_start, "\r\n")) != NULL) {
            *eol = '\0';
            char *line = line_start;

            if (strlen(line) > 0) {
                char cmd[32] = {0}, arg1[256] = {0}, arg2[256] = {0}, arg3[256] = {0};
                int parsed = sscanf(line, "%31s %255s %255s %255s", cmd, arg1, arg2, arg3);

                //sscanf trả về số lượng tham số đọc được
                if (parsed <= 0) {
                    line_start = eol + 2;
                    continue;
                }

                // [AUTH COMMANDS]
                if (strcmp(cmd, STR_LOGIN) == 0) {
                    if (parsed < 3) {
                        perform_send_and_log(sock, line, "400 Bad request\r\n", current_user);
                    } else if (user_id != -1) {
                        perform_send_and_log(sock, line, "409 Already logged in\r\n", current_user);
                    } else if (db_verify_user(conn, arg1, arg2) == 0) {
                        if (is_logged_in(arg1)) perform_send_and_log(sock, line, "409 User logged in elsewhere\r\n", current_user);
                        else {
                            add_logged_user(arg1);
                            strncpy(current_user, arg1, 127);
                            user_id = db_get_user_id_by_name(conn, arg1);
                            perform_send_and_log(sock, line, "110 Login success\r\n", current_user);
                        }
                    } else {
                        perform_send_and_log(sock, line, "401 Login failed\r\n", current_user);
                    }
                }
                else if (strcmp(cmd, STR_REGISTER) == 0) {
                    if (parsed < 3) {
                        perform_send_and_log(sock, line, "400 Bad request\r\n", current_user);
                    } else {
                        char hash[512];
                        utils_hash_password(arg2, hash, sizeof(hash));
                        int res = db_create_user(conn, arg1, hash);
                        if (res == 0) perform_send_and_log(sock, line, "201 Register success\r\n", current_user);
                        else if (res == 1) perform_send_and_log(sock, line, "409 User exists\r\n", current_user);
                        else perform_send_and_log(sock, line, "500 Register error\r\n", current_user);
                    }
                }
                else if (strcmp(cmd, STR_LOGOUT) == 0) {
                    if (user_id != -1) {
                        remove_logged_user(current_user);
                        user_id = -1;
                        memset(current_user, 0, sizeof(current_user));
                    }
                    perform_send_and_log(sock, line, "202 Logout success\r\n", current_user);
                }
                // [GROUP COMMANDS]
                else if (strcmp(cmd, STR_CREATE_GROUP) == 0) {
                    if (parsed < 2) perform_send_and_log(sock, line, "400 Bad request\r\n", current_user);
                    else if (user_id == -1) perform_send_and_log(sock, line, "403 Login required\r\n", current_user);
                    else {
                        struct stat st = {0};
                        if (stat("storage", &st) == -1) mkdir("storage", 0700);
                        char group_path[512];
                        snprintf(group_path, sizeof(group_path), "storage/%s", arg1);
                        if (stat(group_path, &st) == -1) mkdir(group_path, 0700);

                        int group_id_out = 0;
                        int cres = db_create_group(conn, arg1, user_id, group_path, &group_id_out);
                        if (cres == 0) perform_send_and_log(sock, line, "200 Group created\r\n", current_user);
                        else if (cres == 1) perform_send_and_log(sock, line, "409 Group exists\r\n", current_user);
                        else perform_send_and_log(sock, line, "500 Group create error\r\n", current_user);
                    }
                }

                // [UPLOAD COMMAND]
                else if (strcmp(cmd, STR_UPLOAD) == 0) {
                    if (user_id == -1) {
                        perform_send_and_log(sock, line, "403 Login required\r\n", current_user);
                    } else {
                        char *residue_ptr = eol + 2;
                        size_t residue_len = (size_t)((acc + acc_len) - residue_ptr);

                        // Gọi hàm upload (8 tham số)
                        handle_upload_request(sock, arg1, arg2, arg3, residue_ptr, residue_len, current_user, conn, user_id);

                        // Reset buffer
                        acc_len = 0; acc[0] = '\0';
                        break;
                    }
                }
                // [DOWNLOAD COMMAND]
                else if (strcmp(cmd, STR_DOWNLOAD) == 0) {
                    if (user_id == -1) {
                        perform_send_and_log(sock, line, "403 Login required\r\n", current_user);
                    } else {
                        handle_download_request(sock, arg1, arg2, current_user, conn, user_id);

                        acc_len = 0;
                        acc[0] = '\0';
                        break;
                    }
                }
                // [OTHER COMMANDS]
                else {
                    if (user_id == -1) perform_send_and_log(sock, line, "403 Login required\r\n", current_user);
                    else {
                        char resp[4096] = {0};
                        dispatch_request(conn, user_id, line, resp, sizeof(resp));
                        strcat(resp, "\r\n");
                        perform_send_and_log(sock, line, resp, current_user);
                    }
                }
            }
            line_start = eol + 2;
        }

        if (acc_len == 0) continue;

        size_t processed = line_start - acc;
        size_t remaining = acc_len - processed;
        memmove(acc, line_start, remaining);
        
        acc_len = remaining;
        acc[acc_len] = '\0';
    }
    
    if (user_id != -1) remove_logged_user(current_user);
    db_close(conn);
    close(sock);
    return NULL;
}

/**
 * @function start_server
 * Sets up the TCP server socket and accepts incoming connections.
 * * @param port The port number to listen on
 */
void start_server(int port) {
    int server_fd, new_socket;
    struct sockaddr_in address; // Biến này tên là address
    int opt = 1;
    int addrlen = sizeof(address);
    int listen_port = (port > 0) ? port : 8080;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(listen_port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Listening on port %d...\n", listen_port);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        // [FIX 3] Sửa client_addr -> address
        inet_ntop(AF_INET, &address.sin_addr, client_ip, sizeof(client_ip));
        printf("New connection: %s:%d\n", client_ip, ntohs(address.sin_port));

        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        if (!pclient) {
            close(new_socket);
            continue;
        }
        *pclient = new_socket;

        if (pthread_create(&tid, NULL, client_thread, pclient) != 0) {
            perror("pthread_create");
            free(pclient);
            close(new_socket);
        } else {
            pthread_detach(tid);
        }
    }
}

