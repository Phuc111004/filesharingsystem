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
    unsigned long esc_len = mysql_real_escape_string(conn, esc, username, strlen(username));

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
    unsigned long esc_len = mysql_real_escape_string(conn, esc, username, strlen(username));

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
