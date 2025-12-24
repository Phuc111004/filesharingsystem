#ifndef FILE_HANDLERS_H
#define FILE_HANDLERS_H

#include <mysql.h>
#include <stddef.h>

void handle_list_files(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen);
void handle_rename_file(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen);
void handle_delete_file(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen);
void handle_copy_file(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen);
void handle_move_file(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen);

#endif
