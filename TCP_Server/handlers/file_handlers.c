#include "file_handlers.h"
#include "../../database/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/utils.h"
#include <sys/stat.h>

// Internal helper to check if user has leader rights (admin or creator)
static int is_leader(MYSQL *db, int user_id, int group_id) {
    return db_is_group_admin(db, user_id, group_id);
}

// Wrapper for db_list_files to add protocol headers/footers
static void list_files_protocol(MYSQL *db, int group_id, int parent_id, char *response, size_t maxlen) {
    char data_buf[maxlen - 32];
    db_list_files(db, group_id, parent_id, data_buf, sizeof(data_buf));
    snprintf(response, maxlen, "100 List:\n%s\n203 End", data_buf);
}

void handle_list_files(MYSQL *db, int current_user_id, const char *group_id_str, const char *path_str, char *response, size_t maxlen) {
    int group_id = atoi(group_id_str);
    int parent_id = atoi(path_str); // Protocol uses <path> which we treat as ID for now
    
    // Check if user is in group
    if (db_get_user_id_by_name(db, "") == -1) {} // Placeholder for session check if needed

    list_files_protocol(db, group_id, parent_id, response, maxlen);
}

void handle_create_folder(MYSQL *db, int current_user_id, const char *group_id_str, const char *path_str, const char *arg3, char *response, size_t maxlen) {
    int group_id = atoi(group_id_str);
    const char *folder_name = path_str;
    int parent_id = atoi(arg3);

    // Determine path
    char group_name[128];
    db_get_group_name(db, group_id, group_name, sizeof(group_name));
    char folder_path[1024];
    snprintf(folder_path, sizeof(folder_path), "storage/%s/%s", group_name, folder_name);

    // Create physical directory
    mkdir_p(folder_path);

    if (db_create_folder(db, group_id, folder_name, folder_path, current_user_id, parent_id) == 0) {
        snprintf(response, maxlen, "200 OK");
    } else {
        // Return specific error for debugging
        snprintf(response, maxlen, "500 Server error: %s", mysql_error(db));
    }
}

void handle_rename_file(MYSQL *db, int current_user_id, const char *group_id_str, const char *old_path, const char *new_path, char *response, size_t maxlen) {
    int group_id = atoi(group_id_str);
    int item_id = atoi(old_path);
    const char *new_name = new_path;

    // Protocol: CHỈ TRƯỞNG NHÓM
    if (!is_leader(db, current_user_id, group_id)) {
        snprintf(response, maxlen, "403 Forbidden");
        return;
    }

    if (db_rename_item(db, item_id, new_name) == 0) {
        snprintf(response, maxlen, "200 OK");
    } else {
        snprintf(response, maxlen, "404 Not found");
    }
}

void handle_delete_file(MYSQL *db, int current_user_id, const char *group_id_str, const char *path, char *response, size_t maxlen) {
    int group_id = atoi(group_id_str);
    int item_id = atoi(path);

    // Protocol: CHỈ TRƯỞNG NHÓM
    if (!is_leader(db, current_user_id, group_id)) {
        snprintf(response, maxlen, "403 Forbidden");
        return;
    }

    if (db_delete_item(db, item_id) == 0) {
        snprintf(response, maxlen, "200 OK");
    } else {
        snprintf(response, maxlen, "404 Not found");
    }
}

void handle_copy_file(MYSQL *db, int current_user_id, const char *group_id_str, const char *src_path, const char *dest_path, char *response, size_t maxlen) {
    int group_id = atoi(group_id_str);
    int src_id = atoi(src_path);
    int dest_parent_id = atoi(dest_path); 

    (void)group_id; // Silencing warning as it's provided in protocol but permission is checked implicitly by group membership in future

    // Protocol: Tất cả member
    if (db_copy_item(db, src_id, current_user_id, dest_parent_id) == 0) {
        snprintf(response, maxlen, "200 OK");
    } else {
        snprintf(response, maxlen, "404 Not found");
    }
}

void handle_move_file(MYSQL *db, int current_user_id, const char *group_id_str, const char *src_path, const char *dest_path, char *response, size_t maxlen) {
    int group_id = atoi(group_id_str);
    int item_id = atoi(src_path);
    int target_parent_id = atoi(dest_path);

    // Protocol: CHỈ TRƯỞNG NHÓM
    if (!is_leader(db, current_user_id, group_id)) {
        snprintf(response, maxlen, "403 Forbidden");
        return;
    }

    if (db_move_item(db, item_id, group_id, target_parent_id) == 0) {
        snprintf(response, maxlen, "200 OK");
    } else {
        snprintf(response, maxlen, "404 Not found");
    }
}
