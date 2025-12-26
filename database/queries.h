#ifndef QUERIES_H
#define QUERIES_H


#include <mysql/mysql.h>
#include "database.h"


// Example prototypes
int db_create_user(MYSQL* conn, const char* username, const char* password_hash);
// Returns 1 if user exists, 0 if not, -1 on error
int db_user_exists(MYSQL* conn, const char* username);

// Verify username/password: returns 0 on success, 1 on invalid credentials, -1 on error
int db_verify_user(MYSQL* conn, const char* username, const char* password);

// Get user_id by username: returns 0 on success (out_user_id filled), 1 if not found, -1 on error
int db_get_user_id(MYSQL* conn, const char* username, int* out_user_id);

// Returns 1 if group name exists for this owner, 0 if not, -1 on error
int db_group_exists_for_owner(MYSQL* conn, const char* group_name, int owner_user_id);

// Create group with owner, create root directory record, and set owner as admin member.
// Returns 0 on success, 1 if group exists for owner, -1 on error.
int db_create_group(MYSQL* conn, const char* group_name, int owner_user_id, const char* root_path, int* out_group_id);

// Access control helpers
// Returns 1 nếu user là admin của group, 0 nếu không phải, -1 nếu lỗi
int db_is_group_admin(MYSQL* conn, int user_id, int group_id);
// Returns 1 nếu user là member (vai trò 'member'), 0 nếu không, -1 nếu lỗi
int db_is_group_member(MYSQL* conn, int user_id, int group_id);


// Add more prototypes for groups, requests, directories, logs...


#endif // QUERIES_H
