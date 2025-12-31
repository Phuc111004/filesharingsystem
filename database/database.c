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
        if (!first) strncat(buffer, "\n", size - strlen(buffer) - 1);
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
    
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
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

int db_get_group_id_for_user_by_name(MYSQL* conn, int user_id, const char* group_name) {
    if (!conn || !group_name || user_id <= 0) return -1;
    char esc_name[512];
    unsigned long esc_len = mysql_real_escape_string(conn, esc_name, group_name, strlen(group_name));
    esc_name[esc_len] = '\0';

    char query[1024];
    // Chỉ trả về group mà user đang tham gia (thành viên hoặc admin/creator)
    snprintf(query, sizeof(query),
        "SELECT g.group_id FROM `groups` g "
        "LEFT JOIN user_groups ug ON g.group_id = ug.group_id AND ug.user_id = %d "
        "WHERE g.group_name='%s' AND (g.created_by=%d OR ug.user_id IS NOT NULL) "
        "LIMIT 1",
        user_id, esc_name, user_id);

    if (mysql_query(conn, query)) return -1;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return -1;

    MYSQL_ROW row = mysql_fetch_row(res);
    int group_id = 0;
    if (row && row[0]) group_id = atoi(row[0]);
    mysql_free_result(res);
    return group_id; // 0 => not found
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
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
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
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
    
    mysql_free_result(res);
}

void db_get_group_name(MYSQL* conn, int group_id, char* buffer, size_t size) {
    char query[512];
    snprintf(query, sizeof(query), "SELECT group_name FROM `groups` WHERE group_id = %d", group_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Group Unknown");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(res);
    
    if (row) {
        strncpy(buffer, row[0], size - 1);
        buffer[size - 1] = '\0';
    } else {
        snprintf(buffer, size, "Group Unknown");
    }
    
    mysql_free_result(res);
}

// Enhanced group management functions

void db_list_admin_groups(MYSQL* conn, int user_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT g.group_id, g.group_name "
        "FROM `groups` g "
        "LEFT JOIN user_groups ug ON g.group_id = ug.group_id AND ug.user_id = %d "
        "WHERE g.created_by = %d OR (ug.role = 'admin') "
        "GROUP BY g.group_id "
        "ORDER BY g.group_id", user_id, user_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying admin groups.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] %s [ID: %s]", count, row[1], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "You are not admin of any groups.");
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
    mysql_free_result(res);
}
void db_list_my_groups(MYSQL* conn, int user_id, char* buffer, size_t size) {
    char query[1024];
    // List groups where user is member OR admin OR creator
    snprintf(query, sizeof(query),
        "SELECT g.group_id, g.group_name "
        "FROM `groups` g "
        "LEFT JOIN user_groups ug ON g.group_id = ug.group_id AND ug.user_id = %d "
        "WHERE g.created_by = %d OR ug.user_id IS NOT NULL "
        "GROUP BY g.group_id "
        "ORDER BY g.group_id", user_id, user_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying your groups.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] %s [ID: %s]", count, row[1], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "You are not a member of any groups.");
    strncat(buffer, "\n", size - strlen(buffer) - 1);
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
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[128];
        count++;
        snprintf(line, sizeof(line), "[%d] %s", count, row[1]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "All users are already members.");
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
    
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
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[128];
        count++;
        snprintf(line, sizeof(line), "[%d] %s", count, row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "No members to kick (only admins in group).");
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
    mysql_free_result(res);
}

void db_list_group_members_with_roles(MYSQL* conn, int group_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT u.username, ug.role "
        "FROM user_groups ug "
        "JOIN users u ON ug.user_id = u.user_id "
        "WHERE ug.group_id = %d "
        "ORDER BY (ug.role='admin') DESC, u.username",
        group_id);

    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "500 Error querying members.");
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) {
        snprintf(buffer, size, "500 Error querying members.");
        return;
    }

    strcpy(buffer, "");
    int count = 0;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] %s %s", count, row[0], row[1]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }

    if (count == 0) snprintf(buffer, size, "No members in this group.");
    else strncat(buffer, "\n", size - strlen(buffer) - 1);

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
        "  AND (g.created_by = %d OR g.group_id IN (SELECT group_id FROM user_groups WHERE user_id = %d AND role = 'admin')) "
        "ORDER BY r.created_at DESC", user_id, user_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying join requests.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] Group: %s | User: %s [ReqID: %s]", 
                 count, row[1], row[2], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "No pending join requests.");
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
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
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] Group: %s | Admin: %s [InvID: %s]", 
                 count, row[1], row[2], row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "No pending invitations.");
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
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
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
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
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
    
    mysql_free_result(res);
}

void db_list_files(MYSQL* conn, int group_id, int parent_id, char* buffer, size_t size) {
    char query[1024];
    char parent_condition[64];
    
    if (parent_id <= 0) {
        strcpy(parent_condition, "parent_id IS NULL");
    } else {
        snprintf(parent_condition, sizeof(parent_condition), "parent_id = %d", parent_id);
    }

    snprintf(query, sizeof(query),
        "SELECT id, name, path, size, is_folder, parent_id "
        "FROM root_directory "
        "WHERE group_id = %d AND %s AND is_deleted = FALSE "
        "ORDER BY is_folder DESC, name", group_id, parent_condition);
    
    if (mysql_query(conn, query)) {
        // Fallback: return empty directory instead of error
        strcpy(buffer, "No files or folders found in this directory.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) {
        strcpy(buffer, "No files or folders found in this directory.");
        return;
    }
    MYSQL_ROW row;
    
    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
        if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[512];
        count++;
        int is_folder = atoi(row[4]);
        const char *type = is_folder ? "FOLDER" : "FILE";
        const char *size_val = is_folder ? "0" : row[3];
        snprintf(line, sizeof(line), "%s %s %s [ID: %s]", type, row[1], size_val, row[0]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (count == 0) strcpy(buffer, "No files or folders found in this directory.");
    mysql_free_result(res);
}

// Helper: Tái tạo đường dẫn đầy đủ từ ID thư mục
// Trả về: [Group Name]/Folder A/Folder B
static void get_folder_full_path(MYSQL* conn, int folder_id, char* full_path, size_t size) {
    char temp_path[1024] = "";
    int current_id = folder_id;
    char query[512];
    MYSQL_RES *res;
    MYSQL_ROW row;
    
    // 1. Traverse up to root
    while (current_id > 0) {
        snprintf(query, sizeof(query), "SELECT name, parent_id, group_id FROM root_directory WHERE id = %d", current_id);
        if (mysql_query(conn, query)) break;
        
        res = mysql_store_result(conn);
        if (!res) break;
        
        if ((row = mysql_fetch_row(res))) {
            char segment[256];
            snprintf(segment, sizeof(segment), "/%s", row[0]);
            
            // Prepend segment: temp_path = segment + temp_path
            char new_temp[2048];
            snprintf(new_temp, sizeof(new_temp), "%s%s", segment, temp_path);
            strncpy(temp_path, new_temp, sizeof(temp_path) - 1);
            temp_path[sizeof(temp_path) - 1] = '\0';
            
            // Move up
            int parent_id = row[1] ? atoi(row[1]) : 0;
            int group_id = atoi(row[2]);
            
            current_id = parent_id;
            
            // If checking fails or reached root, get group name
            if (parent_id == 0) {
                 char group_name[256];
                 db_get_group_name(conn, group_id, group_name, sizeof(group_name));
                 snprintf(new_temp, sizeof(new_temp), "[%s]%s", group_name, temp_path);
                 strncpy(full_path, new_temp, size - 1);
                 full_path[size - 1] = '\0';
                 mysql_free_result(res);
                 return;
            }
        } else {
            // ID not found (deleted?)
            mysql_free_result(res);
            break;
        }
        mysql_free_result(res);
    }
    // Fallback if loop breaks unexpectedly
    strncpy(full_path, temp_path, size);
}

void db_list_all_folders(MYSQL* conn, int group_id, char* buffer, size_t size) {
    char query[1024];

    // Query all folders in the group, excluding deleted ones
    snprintf(query, sizeof(query),
        "SELECT id, name, parent_id "
        "FROM root_directory "
        "WHERE group_id = %d AND is_folder = TRUE AND is_deleted = FALSE "
        "ORDER BY name", group_id);
    
    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying folders.");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) {
        snprintf(buffer, size, "No folders found.");
        return;
    }
    
    // Store rows manually because we need to run queries inside the loop (which would break the active result set)
    typedef struct {
        int id;
        char name[256];
    } FolderInfo;
    
    FolderInfo folders[100]; // Limit for demo
    int folder_count = 0;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) && folder_count < 100) {
        folders[folder_count].id = atoi(row[0]);
        strncpy(folders[folder_count].name, row[1], 255);
        folder_count++;
    }
    mysql_free_result(res);

    strcpy(buffer, "");
    for (int i=0; i < folder_count; i++) {
        if (i > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
        char line[2048];
        char full_path[1024];
        
        get_folder_full_path(conn, folders[i].id, full_path, sizeof(full_path));
        
        // Format: [FOLDER] Full/Path (ID: <id>)
        snprintf(line, sizeof(line), "[FOLDER] %s (ID: %d)", full_path, folders[i].id);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }
    
    if (folder_count == 0) strcpy(buffer, "No folders available.");
    // Append trailing newline for multi-line protocol
    strncat(buffer, "\n", size - strlen(buffer) - 1);
}

int db_create_folder(MYSQL* conn, int group_id, const char* name, const char* path, int uploaded_by, int parent_id) {
    char query[2048];
    if (parent_id <= 0) {
        snprintf(query, sizeof(query), 
            "INSERT INTO root_directory (group_id, name, path, size, uploaded_by, is_folder, parent_id) "
            "VALUES (%d, '%s', '%s', 0, %d, TRUE, NULL)", 
            group_id, name, path, uploaded_by);
    } else {
        snprintf(query, sizeof(query), 
            "INSERT INTO root_directory (group_id, name, path, size, uploaded_by, is_folder, parent_id) "
            "VALUES (%d, '%s', '%s', 0, %d, TRUE, %d)", 
            group_id, name, path, uploaded_by, parent_id);
    }
    return db_execute(conn, query);
}

int db_rename_item(MYSQL* conn, int item_id, const char* new_name) {
    char query[1024];
    snprintf(query, sizeof(query), "UPDATE root_directory SET name='%s' WHERE id=%d", new_name, item_id);
    return db_execute(conn, query);
}

int db_delete_item(MYSQL* conn, int item_id) {
    char query[512];
    
    // 1. Tìm và xóa đệ quy các con
    snprintf(query, sizeof(query), "SELECT id FROM root_directory WHERE parent_id = %d AND is_deleted = FALSE", item_id);
    if (mysql_query(conn, query)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res) {
        // Lưu ID các con vào mảng tạm để tránh xung đột cursor MySQL
        int child_ids[256];
        int child_count = 0;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) && child_count < 256) {
            child_ids[child_count++] = atoi(row[0]);
        }
        mysql_free_result(res);

        for (int i = 0; i < child_count; i++) {
            db_delete_item(conn, child_ids[i]);
        }
    }

    // 2. Đánh dấu xóa chính mục này
    snprintf(query, sizeof(query), "UPDATE root_directory SET is_deleted=TRUE WHERE id=%d", item_id);
    return db_execute(conn, query);
}

// Helper: Đệ quy copy item
static int copy_recursive(MYSQL* conn, int source_id, int dest_parent_id, int uploaded_by, int add_prefix) {
    char query[2048];
    MYSQL_RES *res;
    MYSQL_ROW row;

    // 1. Lấy thông tin source
    snprintf(query, sizeof(query), "SELECT group_id, name, path, size, is_folder FROM root_directory WHERE id=%d", source_id);
    if (mysql_query(conn, query)) return -1;
    
    res = mysql_store_result(conn);
    if (!res || mysql_num_rows(res) == 0) {
        if (res) mysql_free_result(res);
        return -1;
    }
    row = mysql_fetch_row(res);
    
    int group_id = atoi(row[0]);
    char old_name[256];
    strncpy(old_name, row[1], 255);
    old_name[255] = '\0';
    char path[512];
    strncpy(path, row[2], 511);
    path[511] = '\0'; // Path có thể cần xử lý lại logic nếu hệ thống dùng path vật lý
    long long size = atoll(row[3]);
    int is_folder = atoi(row[4]);
    
    mysql_free_result(res);

    // 2. Xác định tên mới
    char new_name[512];
    if (add_prefix) {
        snprintf(new_name, sizeof(new_name), "Copy of %s", old_name);
    } else {
        strncpy(new_name, old_name, sizeof(new_name));
    }

    // 3. Insert bản ghi mới
    char parent_sql[32];
    if (dest_parent_id <= 0) strcpy(parent_sql, "NULL");
    else snprintf(parent_sql, sizeof(parent_sql), "%d", dest_parent_id);

    // Escape strings
    char esc_name[512], esc_path[1024];
    mysql_real_escape_string(conn, esc_name, new_name, strlen(new_name));
    mysql_real_escape_string(conn, esc_path, path, strlen(path));

    snprintf(query, sizeof(query),
        "INSERT INTO root_directory (group_id, name, path, size, uploaded_by, is_folder, parent_id) "
        "VALUES (%d, '%s', '%s', %lld, %d, %d, %s)",
        group_id, esc_name, esc_path, size, uploaded_by, is_folder, parent_sql);
    
    if (db_execute(conn, query) != 0) return -1;

    int new_id = (int)mysql_insert_id(conn);

    // 4. Nếu là folder, copy đệ quy các con
    if (is_folder) {
        snprintf(query, sizeof(query), "SELECT id FROM root_directory WHERE parent_id=%d AND is_deleted=FALSE", source_id);
        if (mysql_query(conn, query)) return 0; // Warning: Partial deletion logic check needed?
        
        res = mysql_store_result(conn);
        if (res) {
            int child_ids[100]; // Limit recursion batch
            int count = 0;
            while ((row = mysql_fetch_row(res)) && count < 100) {
                child_ids[count++] = atoi(row[0]);
            }
            mysql_free_result(res);

            for (int i = 0; i < count; i++) {
                copy_recursive(conn, child_ids[i], new_id, uploaded_by, 0); // 0 prefix for children
            }
        }
    }
    return 0;
}

int db_copy_item(MYSQL* conn, int item_id, int uploaded_by, int dest_parent_id) {
    return copy_recursive(conn, item_id, dest_parent_id, uploaded_by, 1);
}

int db_move_item(MYSQL* conn, int item_id, int new_group_id, int new_parent_id) {
    char query[512];
    if (new_parent_id <= 0) {
        snprintf(query, sizeof(query), "UPDATE root_directory SET group_id=%d, parent_id=NULL WHERE id=%d", new_group_id, item_id);
    } else {
        snprintf(query, sizeof(query), "UPDATE root_directory SET group_id=%d, parent_id=%d WHERE id=%d", new_group_id, new_parent_id, item_id);
    }
    return db_execute(conn, query);
}

void db_list_user_groups(MYSQL* conn, int user_id, char* buffer, size_t size) {
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT g.group_id, g.group_name, u.username "
        "FROM `groups` g "
        "JOIN user_groups ug ON g.group_id = ug.group_id "
        "JOIN users u ON g.created_by = u.user_id "
        "WHERE ug.user_id = %d "
        "ORDER BY g.group_id", user_id);

    if (mysql_query(conn, query)) {
        snprintf(buffer, size, "Error querying your groups.");
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;

    strcpy(buffer, "");
    int count = 0;
    while ((row = mysql_fetch_row(res))) {
          if (count > 0) strncat(buffer, "\n", size - strlen(buffer) - 1);
          char line[256];
        count++;
        snprintf(line, sizeof(line), "[%d] %s (Admin: %s)",
                  count, row[1], row[2]);
        strncat(buffer, line, size - strlen(buffer) - 1);
    }

    if (count == 0) strcpy(buffer, "You are not in any groups.");
    mysql_free_result(res);
}


int db_add_file(MYSQL* conn, int group_id, const char* name, const char* path, long long size, int uploaded_by, int parent_id) {
    char query[2048];
    char parent_val[32];
    if (parent_id <= 0) strcpy(parent_val, "NULL");
    else snprintf(parent_val, sizeof(parent_val), "%d", parent_id);

    char esc_name[256], esc_path[512];
    mysql_real_escape_string(conn, esc_name, name, strlen(name));
    mysql_real_escape_string(conn, esc_path, path, strlen(path));

    snprintf(query, sizeof(query), 
        "INSERT INTO root_directory (group_id, name, path, size, uploaded_by, is_folder, parent_id) "
        "VALUES (%d, '%s', '%s', %lld, %d, FALSE, %s)", 
        group_id, esc_name, esc_path, size, uploaded_by, parent_val);

    return db_execute(conn, query);
}

int db_resolve_path(MYSQL* conn, const char* full_path, int return_type, int *out_group_id) {
    if (!full_path || strlen(full_path) == 0) return -1;

    char path_copy[1024];
    strncpy(path_copy, full_path, sizeof(path_copy));
    
    char *token = strtok(path_copy, "/");
    if (!token) return -1;

    // Simple lookup: find group by name (simplified implementation)
    char query[512];
    snprintf(query, sizeof(query), "SELECT group_id FROM groups WHERE group_name = '%s'", token);
    
    if (mysql_query(conn, query)) return -1;
    
    MYSQL_RES *result = mysql_store_result(conn);
    if (!result) return -1;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int group_id = -1;
    if (row && row[0]) {
        group_id = atoi(row[0]);
    }
    mysql_free_result(result);
    
    if (group_id == -1) return -1;
    if (out_group_id) *out_group_id = group_id;

    int current_parent_id = 0;
    int last_id = 0;
    
    while ((token = strtok(NULL, "/")) != NULL) {
        char query[1024];
        char esc_name[256];
        mysql_real_escape_string(conn, esc_name, token, strlen(token));

        char parent_cond[64];
        if (current_parent_id == 0) strcpy(parent_cond, "parent_id IS NULL");
        else snprintf(parent_cond, sizeof(parent_cond), "parent_id = %d", current_parent_id);

        snprintf(query, sizeof(query), 
            "SELECT id, is_folder FROM root_directory WHERE group_id=%d AND name='%s' AND %s AND is_deleted=FALSE",
            group_id, esc_name, parent_cond);

        if (mysql_query(conn, query)) return -1;
        MYSQL_RES *res = mysql_store_result(conn);
        if (mysql_num_rows(res) == 0) {
            mysql_free_result(res);
            return -1;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        int id = atoi(row[0]);
        int is_folder = atoi(row[1]);
        mysql_free_result(res);

        last_id = id;
        if (is_folder) {
            current_parent_id = id; // Đi sâu xuống
        } else {
            // Nếu gặp file mà vẫn còn token phía sau -> Lỗi (File không thể chứa File)
            // Trừ khi đây là token cuối cùng
        }
    }

    // Xử lý logic trả về
    if (return_type == 0) {
        // Nếu path chỉ có "GroupA" -> parent là Root (0)
        // Nếu path "GroupA/FolderB" -> parent là ID của FolderB
        return current_parent_id; 
    } else {
        return last_id;
    }
}

int db_get_group_root_id(MYSQL* conn, int group_id) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT root_dir_id FROM `groups` WHERE group_id = %d", group_id);
    
    if (mysql_query(conn, query)) return 0;
    
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return 0;
    
    MYSQL_ROW row = mysql_fetch_row(res);
    int root_id = 0;
    if (row && row[0]) {
        root_id = atoi(row[0]);
    }
    mysql_free_result(res);
    return root_id;
}