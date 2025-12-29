#include "queries.h"
#include <stdio.h>
#include <string.h>
#include <mysql/mysql.h>
#include "../common/utils.h"

// Check if a username already exists
int db_user_exists(MYSQL* conn, const char* username) {
    if (!conn || !username) return -1;
    // Use a simple query with escaping to check existence to avoid stmt binding compatibility issues
    char esc[512];
    mysql_real_escape_string(conn, esc, username, strlen(username)); // Quan: biến esc_len được khai báo để nhận giá trị trả về từ mysql_real_escape_string nhưng không được sử dụng ở các dòng 12 và 77, dẫn đến cảnh báo từ trình biên dịch.

    char sql[1024];
    snprintf(sql, sizeof(sql), "SELECT user_id FROM users WHERE username='%s' LIMIT 1", esc);

    if (mysql_query(conn, sql) != 0) return -1;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return -1; // error or no result

    my_ulonglong num_rows = mysql_num_rows(res);
    int exists = (num_rows > 0) ? 1 : 0;
    mysql_free_result(res);
    return exists;
}


// Create user with username and password_hash. Returns 0 on success, 1 if user exists, -1 on error.
int db_create_user(MYSQL* conn, const char* username, const char* password_hash) {
    if (!conn || !username || !password_hash) return -1;

    int exists = db_user_exists(conn, username);
    if (exists == 1) return 1; // already exists
    if (exists == -1) return -1; // error

    const char *sql = "INSERT INTO users (username, password) VALUES (?, ?);";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return -1;

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)username;
    bind[0].buffer_length = strlen(username);

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)password_hash;
    bind[1].buffer_length = strlen(password_hash);

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    mysql_stmt_close(stmt);
    return 0;
}


// Verify username/password: returns 0 on success, 1 on invalid credentials, -1 on error
int db_verify_user(MYSQL* conn, const char* username, const char* password) {
    if (!conn || !username || !password) return -1;

    // escape username
    char esc[512];
    mysql_real_escape_string(conn, esc, username, strlen(username));

    char sql[1024];
    snprintf(sql, sizeof(sql), "SELECT password FROM users WHERE username='%s' LIMIT 1", esc);

    if (mysql_query(conn, sql) != 0) return -1;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return -1;

    my_ulonglong num_rows = mysql_num_rows(res);
    if (num_rows == 0) {
        mysql_free_result(res);
        return 1; // user not found -> invalid credentials
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return -1;
    }

    const char *stored_hash = row[0] ? row[0] : "";

    // compute hash of provided password
    char provided_hash[512];
    if (utils_hash_password(password, provided_hash, sizeof(provided_hash)) != 0) {
        mysql_free_result(res);
        return -1;
    }

    int result = (strcmp(stored_hash, provided_hash) == 0) ? 0 : 1;
    mysql_free_result(res);
    return result;
}


// Get user_id by username
int db_get_user_id(MYSQL* conn, const char* username, int* out_user_id) {
    if (!conn || !username || !out_user_id) return -1;
    char esc[512];
    unsigned long esc_len = mysql_real_escape_string(conn, esc, username, strlen(username));
    esc[esc_len] = '\0';

    char sql[1024];
    snprintf(sql, sizeof(sql), "SELECT user_id FROM users WHERE username='%s' LIMIT 1", esc);

    if (mysql_query(conn, sql) != 0) return -1;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return -1;

    if (mysql_num_rows(res) == 0) {
        mysql_free_result(res);
        return 1; // not found
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row || !row[0]) {
        mysql_free_result(res);
        return -1;
    }
    *out_user_id = atoi(row[0]);
    mysql_free_result(res);
    return 0;
}


// Check if group name exists for owner
int db_group_exists_for_owner(MYSQL* conn, const char* group_name, int owner_user_id) {
    if (!conn || !group_name) return -1;
    char esc_name[512];
    unsigned long esc_len = mysql_real_escape_string(conn, esc_name, group_name, strlen(group_name));
    esc_name[esc_len] = '\0';

    char sql[1024];
    snprintf(sql, sizeof(sql), "SELECT group_id FROM `groups` WHERE group_name='%s' AND created_by=%d LIMIT 1", esc_name, owner_user_id);

    if (mysql_query(conn, sql) != 0) return -1;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return -1;

    int exists = (mysql_num_rows(res) > 0) ? 1 : 0;
    mysql_free_result(res);
    return exists;
}


// Create group, root directory, and owner membership
int db_create_group(MYSQL* conn, const char* group_name, int owner_user_id, const char* root_path, int* out_group_id) {
    if (!conn || !group_name || !root_path) return -1;

    int exists = db_group_exists_for_owner(conn, group_name, owner_user_id);
    if (exists == 1) return 1;
    if (exists == -1) return -1;

    // Insert into groups
    const char *sql_group = "INSERT INTO `groups` (group_name, created_by) VALUES (?, ?);";
    MYSQL_STMT *stmt_group = mysql_stmt_init(conn);
    if (!stmt_group) return -1;
    if (mysql_stmt_prepare(stmt_group, sql_group, strlen(sql_group)) != 0) {
        mysql_stmt_close(stmt_group);
        return -1;
    }

    MYSQL_BIND bind_group[2];
    memset(bind_group, 0, sizeof(bind_group));

    bind_group[0].buffer_type = MYSQL_TYPE_STRING;
    bind_group[0].buffer = (char*)group_name;
    bind_group[0].buffer_length = strlen(group_name);

    bind_group[1].buffer_type = MYSQL_TYPE_LONG;
    bind_group[1].buffer = (char*)&owner_user_id;
    bind_group[1].is_unsigned = 0;

    if (mysql_stmt_bind_param(stmt_group, bind_group) != 0 || mysql_stmt_execute(stmt_group) != 0) {
        mysql_stmt_close(stmt_group);
        return -1;
    }
    mysql_stmt_close(stmt_group);

    int group_id = (int)mysql_insert_id(conn);
    if (out_group_id) *out_group_id = group_id;

    // Insert root directory record
    const char *sql_root = "INSERT INTO root_directory (group_id, name, path, size, uploaded_by, is_folder, parent_id) VALUES (?, ?, ?, 0, ?, TRUE, NULL);";
    MYSQL_STMT *stmt_root = mysql_stmt_init(conn);
    if (!stmt_root) return -1;
    if (mysql_stmt_prepare(stmt_root, sql_root, strlen(sql_root)) != 0) {
        mysql_stmt_close(stmt_root);
        return -1;
    }

    MYSQL_BIND bind_root[4];
    memset(bind_root, 0, sizeof(bind_root));

    bind_root[0].buffer_type = MYSQL_TYPE_LONG;
    bind_root[0].buffer = (char*)&group_id;
    bind_root[1].buffer_type = MYSQL_TYPE_STRING;
    bind_root[1].buffer = (char*)group_name;
    bind_root[1].buffer_length = strlen(group_name);
    bind_root[2].buffer_type = MYSQL_TYPE_STRING;
    bind_root[2].buffer = (char*)root_path;
    bind_root[2].buffer_length = strlen(root_path);
    bind_root[3].buffer_type = MYSQL_TYPE_LONG;
    bind_root[3].buffer = (char*)&owner_user_id;

    if (mysql_stmt_bind_param(stmt_root, bind_root) != 0 || mysql_stmt_execute(stmt_root) != 0) {
        mysql_stmt_close(stmt_root);
        return -1;
    }
    mysql_stmt_close(stmt_root);

    int root_id = (int)mysql_insert_id(conn);

    // Update groups.root_dir_id
    const char *sql_update = "UPDATE `groups` SET root_dir_id = ? WHERE group_id = ?";
    MYSQL_STMT *stmt_upd = mysql_stmt_init(conn);
    if (!stmt_upd) return -1;
    if (mysql_stmt_prepare(stmt_upd, sql_update, strlen(sql_update)) != 0) {
        mysql_stmt_close(stmt_upd);
        return -1;
    }

    MYSQL_BIND bind_upd[2];
    memset(bind_upd, 0, sizeof(bind_upd));
    bind_upd[0].buffer_type = MYSQL_TYPE_LONG;
    bind_upd[0].buffer = (char*)&root_id;
    bind_upd[1].buffer_type = MYSQL_TYPE_LONG;
    bind_upd[1].buffer = (char*)&group_id;

    if (mysql_stmt_bind_param(stmt_upd, bind_upd) != 0 || mysql_stmt_execute(stmt_upd) != 0) {
        mysql_stmt_close(stmt_upd);
        return -1;
    }
    mysql_stmt_close(stmt_upd);

    // Insert owner membership as admin
    const char *sql_member = "INSERT INTO user_groups (user_id, group_id, role) VALUES (?, ?, 'admin');";
    MYSQL_STMT *stmt_mem = mysql_stmt_init(conn);
    if (!stmt_mem) return -1;
    if (mysql_stmt_prepare(stmt_mem, sql_member, strlen(sql_member)) != 0) {
        mysql_stmt_close(stmt_mem);
        return -1;
    }

    MYSQL_BIND bind_mem[2];
    memset(bind_mem, 0, sizeof(bind_mem));
    bind_mem[0].buffer_type = MYSQL_TYPE_LONG;
    bind_mem[0].buffer = (char*)&owner_user_id;
    bind_mem[1].buffer_type = MYSQL_TYPE_LONG;
    bind_mem[1].buffer = (char*)&group_id;

    if (mysql_stmt_bind_param(stmt_mem, bind_mem) != 0 || mysql_stmt_execute(stmt_mem) != 0) {
        mysql_stmt_close(stmt_mem);
        return -1;
    }
    mysql_stmt_close(stmt_mem);

    return 0;
}

// Trả về 1 nếu user có vai trò 'member' trong group, 0 nếu không, -1 nếu lỗi.
int db_is_group_member(MYSQL* conn, int user_id, int group_id) {
    if (!conn || user_id <= 0 || group_id <= 0) return -1;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT role FROM user_groups WHERE user_id=%d AND group_id=%d LIMIT 1",
             user_id, group_id);

    if (mysql_query(conn, sql) != 0) return -1;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return -1;

    int is_member = 0;
    if (mysql_num_rows(res) > 0) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0] && strcmp(row[0], "member") == 0) is_member = 1;
    }

    mysql_free_result(res);
    return is_member;
}
