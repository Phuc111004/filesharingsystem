#include "database.h"
#include "../config/db_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


MYSQL* db_connect() {
    MYSQL *conn = mysql_init(NULL);
    if (!conn) {
        fprintf(stderr, "mysql_init() failed\n");
        return NULL;
    }

    // Dùng UNIX socket thay vì TCP
    if (!mysql_real_connect(conn,
                            DB_HOST,     // NULL → dùng socket
                            DB_USER,
                            DB_PASS,
                            DB_NAME,
                            DB_PORT,     // 0
                            DB_SOCKET,   // đường dẫn sock
                            0)) 
    {
        fprintf(stderr, "MySQL connect error: %s\n", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }

    mysql_set_character_set(conn, "utf8mb4");
    return conn;
}



void db_close(MYSQL* conn) {
if (conn) mysql_close(conn);
}


int db_execute(MYSQL* conn, const char* sql) {
if (mysql_query(conn, sql)) {
fprintf(stderr, "Query error: %s\nSQL: %s\n", mysql_error(conn), sql);
return -1;
}
return 0;
}


int db_ping(MYSQL* conn) {
	if (!conn) return -1;
	if (mysql_ping(conn) == 0) return 0;
	fprintf(stderr, "MySQL ping failed: %s\n", mysql_error(conn));
	return -1;
}

// ---------------------------------------------------------------------
// Group Request Implementation
// ---------------------------------------------------------------------

int db_create_group_request(MYSQL* conn, int user_id, int group_id, const char* type) {
    char query[512];
    // Check if request already exists
    sprintf(query, "SELECT request_id FROM group_requests WHERE user_id=%d AND group_id=%d AND status='pending'", user_id, group_id);
    if (mysql_query(conn, query)) return -1;
    MYSQL_RES *res = mysql_store_result(conn);
    if (mysql_num_rows(res) > 0) {
        mysql_free_result(res);
        return -2; // Already pending
    }
    mysql_free_result(res);

    sprintf(query, "INSERT INTO group_requests (user_id, group_id, request_type) VALUES (%d, %d, '%s')", 
            user_id, group_id, type);
    return db_execute(conn, query);
}

int db_update_request_status(MYSQL* conn, int request_id, const char* status) {
    char query[256];
    sprintf(query, "UPDATE group_requests SET status='%s' WHERE request_id=%d", status, request_id);
    return db_execute(conn, query);
}

int db_add_group_member(MYSQL* conn, int user_id, int group_id, const char* role) {
    char query[256];
    sprintf(query, "INSERT INTO user_groups (user_id, group_id, role) VALUES (%d, %d, '%s') ON DUPLICATE KEY UPDATE role='%s'", 
            user_id, group_id, role, role);
    return db_execute(conn, query);
}

int db_remove_group_member(MYSQL* conn, int user_id, int group_id) {
    char query[256];
    sprintf(query, "DELETE FROM user_groups WHERE user_id=%d AND group_id=%d", user_id, group_id);
    return db_execute(conn, query);
}

void db_list_pending_requests(MYSQL* conn, int user_id, char* buffer, size_t size) {
    // Logic:
    // 1. If user is admin of a group, show JOIN REQUESTS for that group.
    // 2. If user has INVITATIONS, show them.
    
    char query[1024];
    // Complex query to get both types relevant to the user
    // Part A: Join requests for groups where user is admin
    // Part B: Invitations for the user
    
    snprintf(query, sizeof(query), 
        "SELECT r.request_id, r.group_id, g.group_name, u.username, r.request_type "
        "FROM group_requests r "
        "JOIN `groups` g ON r.group_id = g.group_id "
        "JOIN users u ON r.user_id = u.user_id "
        "WHERE r.status = 'pending' "
        "AND ( "
        "  (r.request_type = 'join_request' AND r.group_id IN (SELECT group_id FROM user_groups WHERE user_id=%d AND role='admin')) "
        "  OR "
        "  (r.request_type = 'invitation' AND r.user_id = %d) "
        ")", user_id, user_id);

    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying requests.");
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "Pending Requests:\n");
    while ((row = mysql_fetch_row(res))) {
        char line[256];
        // request_id | group_id | group_name | username | type
        if (strcmp(row[4], "join_request") == 0) {
            snprintf(line, sizeof(line), "[ID: %s] User '%s' wants to join Group '%s' (ID: %s)\n", 
                     row[0], row[3], row[2], row[1]);
        } else {
            snprintf(line, sizeof(line), "[ID: %s] You are invited to Group '%s' (ID: %s)\n", 
                     row[0], row[2], row[1]);
        }
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    mysql_free_result(res);
}

int db_is_group_admin(MYSQL* conn, int user_id, int group_id) {
    char query[256];
    sprintf(query, "SELECT 1 FROM user_groups WHERE user_id=%d AND group_id=%d AND role='admin'", user_id, group_id);
    if (mysql_query(conn, query)) return 0;
    MYSQL_RES *res = mysql_store_result(conn);
    int is_admin = (mysql_num_rows(res) > 0);
    mysql_free_result(res);
    return is_admin;
}

int db_get_user_id_by_name(MYSQL* conn, const char* username) {
    char query[256];
    sprintf(query, "SELECT user_id FROM users WHERE username='%s'", username);
    if (mysql_query(conn, query)) return -1;
    MYSQL_RES *res = mysql_store_result(conn);
    if (mysql_num_rows(res) == 0) {
        mysql_free_result(res);
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    int id = atoi(row[0]);
    mysql_free_result(res);
    return id;
}

int db_check_group_exists(MYSQL* conn, int group_id) {
    char query[256];
    sprintf(query, "SELECT 1 FROM `groups` WHERE group_id=%d", group_id);
    if (mysql_query(conn, query)) return 0;
    MYSQL_RES *res = mysql_store_result(conn);
    int exists = (mysql_num_rows(res) > 0);
    mysql_free_result(res);
    return exists;
}
