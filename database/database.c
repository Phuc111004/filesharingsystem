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
    
    strcpy(buffer, "");
    int first = 1;
    while ((row = mysql_fetch_row(res))) {
        if (!first) strncat(buffer, " | ", size - strlen(buffer) - 1);
        char line[256];
        // request_id | group_id | group_name | username | type
        if (strcmp(row[4], "join_request") == 0) {
            snprintf(line, sizeof(line), "[ID: %s] User '%s' wants to join Group '%s' (ID: %s)", 
                     row[0], row[3], row[2], row[1]);
        } else {
            snprintf(line, sizeof(line), "[ID: %s] You are invited to Group '%s' (ID: %s)", 
                     row[0], row[2], row[1]);
        }
        strncat(buffer, line, size - strlen(buffer) - 1);
        first = 0;
    }
    if (first) strcpy(buffer, "No pending requests.");
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

void db_list_all_groups(MYSQL* conn, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT g.group_id, g.group_name, u.username "
        "FROM `groups` g "
        "JOIN users u ON g.created_by = u.user_id "
        "ORDER BY g.group_id");
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying groups.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, " | ", size - strlen(buffer) - 1);
        char line[256];
        count++;
        // Format: [num] GroupName (Admin: username) [ID: group_id]
        snprintf(line, sizeof(line), "[%d] %s (Admin: %s) [ID: %s]", 
                 count, row[1], row[2], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) {
        strcpy(buffer, "No groups available.");
    }
    
    mysql_free_result(res);
}

// Enhanced group management functions

void db_list_admin_groups(MYSQL* conn, int user_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT g.group_id, g.group_name "
        "FROM `groups` g "
        "JOIN user_groups ug ON g.group_id = ug.group_id "
        "WHERE ug.user_id = %d AND ug.role = 'admin' "
        "ORDER BY g.group_id", user_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying admin groups.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, " | ", size - strlen(buffer) - 1);
        char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] %s [ID: %s]", count, row[1], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "You are not admin of any groups.");
    mysql_free_result(res);
}

void db_list_non_members(MYSQL* conn, int group_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT u.user_id, u.username "
        "FROM users u "
        "WHERE u.user_id NOT IN ( "
        "   SELECT user_id FROM user_groups WHERE group_id = %d "
        ") "
        "ORDER BY u.username", group_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying non-members.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, " | ", size - strlen(buffer) - 1);
        char line[128];
        count++;
        snprintf(line, sizeof(line), "[%d] %s", count, row[1]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "All users are already members.");
    mysql_free_result(res);
}

void db_list_group_members(MYSQL* conn, int group_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT u.username, ug.role "
        "FROM user_groups ug "
        "JOIN users u ON ug.user_id = u.user_id "
        "WHERE ug.group_id = %d AND ug.role = 'member' "
        "ORDER BY u.username", group_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying members.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, " | ", size - strlen(buffer) - 1);
        char line[128];
        count++;
        snprintf(line, sizeof(line), "[%d] %s", count, row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "No members to kick (only admins in group).");
    mysql_free_result(res);
}

void db_list_join_requests_for_admin(MYSQL* conn, int user_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT r.request_id, g.group_name, u.username, r.created_at "
        "FROM group_requests r "
        "JOIN `groups` g ON r.group_id = g.group_id "
        "JOIN users u ON r.user_id = u.user_id "
        "WHERE r.request_type = 'join_request' "
        "  AND r.status = 'pending' "
        "  AND r.group_id IN (SELECT group_id FROM user_groups WHERE user_id = %d AND role = 'admin') "
        "ORDER BY r.created_at DESC", user_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying join requests.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, " | ", size - strlen(buffer) - 1);
        char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] Group: %s | User: %s [ReqID: %s]", 
                 count, row[1], row[2], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "No pending join requests.");
    mysql_free_result(res);
}

void db_list_invitations_for_user(MYSQL* conn, int user_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT r.request_id, g.group_name, u.username, r.created_at "
        "FROM group_requests r "
        "JOIN `groups` g ON r.group_id = g.group_id "
        "JOIN users u ON g.created_by = u.user_id "
        "WHERE r.request_type = 'invitation' "
        "  AND r.status = 'pending' "
        "  AND r.user_id = %d "
        "ORDER BY r.created_at DESC", user_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying invitations.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, " | ", size - strlen(buffer) - 1);
        char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] Group: %s | Admin: %s [InvID: %s]", 
                 count, row[1], row[2], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "No pending invitations.");
    mysql_free_result(res);
}

void db_list_joinable_groups(MYSQL* conn, int user_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT g.group_id, g.group_name, u.username "
        "FROM `groups` g "
        "JOIN users u ON g.created_by = u.user_id "
        "WHERE g.group_id NOT IN (SELECT group_id FROM user_groups WHERE user_id = %d) "
        "ORDER BY g.group_id", user_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying joinable groups.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, " | ", size - strlen(buffer) - 1);
        char line[256];
        count++;
        // Format: [num] GroupName (Admin: username) [ID: group_id]
        snprintf(line, sizeof(line), "[%d] %s (Admin: %s) [ID: %s]", 
                 count, row[1], row[2], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) {
        strcpy(buffer, "No joinable groups available.");
    }
    
    mysql_free_result(res);
}
