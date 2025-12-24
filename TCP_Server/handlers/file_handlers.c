#include "file_handlers.h"
#include "../../database/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal helper to check if user has leader rights (admin or creator)
static int is_leader(MYSQL *db, int user_id, int group_id) {
    return db_is_group_admin(db, user_id, group_id);
}

// Internal helper to get group_id from file_id
static int get_group_from_file(MYSQL *db, int file_id) {
    char query[256];
    sprintf(query, "SELECT group_id FROM root_directory WHERE id = %d", file_id);
    if (mysql_query(db, query)) return -1;
    MYSQL_RES *res = mysql_store_result(db);
    MYSQL_ROW row = mysql_fetch_row(res);
    int group_id = row ? atoi(row[0]) : -1;
    mysql_free_result(res);
    return group_id;
}

void handle_list_files(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen) {
    int group_id = atoi(arg1);
    // Any group member can list files. For simplicity, we just list.
    db_list_files(db, group_id, response, maxlen);
}

void handle_rename_file(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen) {
    int file_id = atoi(arg1);
    const char *new_name = arg2;
    int group_id = get_group_from_file(db, file_id);
    
    if (group_id == -1) {
        snprintf(response, maxlen, "404 File not found");
        return;
    }

    if (!is_leader(db, current_user_id, group_id)) {
        snprintf(response, maxlen, "403 Forbidden");
        return;
    }

    if (db_rename_item(db, file_id, new_name) == 0) {
        snprintf(response, maxlen, "200 File renamed");
    } else {
        snprintf(response, maxlen, "500 Rename failed");
    }
}

void handle_delete_file(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen) {
    int file_id = atoi(arg1);
    int group_id = get_group_from_file(db, file_id);
    
    if (group_id == -1) {
        snprintf(response, maxlen, "404 File not found");
        return;
    }

    if (!is_leader(db, current_user_id, group_id)) {
        snprintf(response, maxlen, "403 Forbidden");
        return;
    }

    if (db_delete_item(db, file_id) == 0) {
        snprintf(response, maxlen, "200 File deleted");
    } else {
        snprintf(response, maxlen, "500 Delete failed");
    }
}

void handle_copy_file(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen) {
    int file_id = atoi(arg1);
    // Members can copy
    if (db_copy_item(db, file_id, current_user_id) == 0) {
        snprintf(response, maxlen, "200 File copied");
    } else {
        snprintf(response, maxlen, "500 Copy failed");
    }
}

void handle_move_file(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen) {
    int file_id = atoi(arg1);
    int target_group_id = atoi(arg2);
    int source_group_id = get_group_from_file(db, file_id);

    if (source_group_id == -1) {
        snprintf(response, maxlen, "404 File not found");
        return;
    }

    // Must be leader of source group to move out
    if (!is_leader(db, current_user_id, source_group_id)) {
        snprintf(response, maxlen, "403 Forbidden");
        return;
    }

    if (db_move_item(db, file_id, target_group_id) == 0) {
        snprintf(response, maxlen, "200 File moved");
    } else {
        snprintf(response, maxlen, "500 Move failed");
    }
}
