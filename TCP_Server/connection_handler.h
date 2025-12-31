#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include <netinet/in.h>
#include <mysql/mysql.h>

void start_server(int port);
void* client_thread(void* arg);
void handle_upload_request(int client_sock, const char *folder, const char *filename, const char *size_str, const char *residue_data, size_t residue_len, const char *username, MYSQL *db_conn, int user_id);
void handle_download_request(int client_sock, const char *folder, const char *filename, const char *username, MYSQL *db_conn, int user_id);

#endif // CONNECTION_HANDLER_H
