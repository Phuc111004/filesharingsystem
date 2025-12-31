#ifndef FILE_HANDLERS_H
#define FILE_HANDLERS_H

#include <mysql.h>
#include <stddef.h>

void handle_list_files(MYSQL *db, int current_user_id, const char *group_id, const char *path, char *response, size_t maxlen);
void handle_list_all_folders(MYSQL *db, int current_user_id, const char *group_id, char *response, size_t maxlen);
void handle_create_folder(MYSQL *db, int current_user_id, const char *group_id, const char *path, const char *arg3, char *response, size_t maxlen);
void handle_rename_file(MYSQL *db, int current_user_id, const char *group_id, const char *old_path, const char *new_path, char *response, size_t maxlen);
void handle_delete_file(MYSQL *db, int current_user_id, const char *group_id, const char *path, char *response, size_t maxlen);
void handle_copy_file(MYSQL *db, int current_user_id, const char *group_id, const char *src_path, const char *dest_path, char *response, size_t maxlen);
void handle_move_file(MYSQL *db, int current_user_id, const char *group_id, const char *src_path, const char *dest_path, char *response, size_t maxlen);

#endif
