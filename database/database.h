#ifndef DATABASE_H
#define DATABASE_H
#include <mysql/mysql.h>

MYSQL* db_connect();
void db_close(MYSQL* conn);

// helper: execute a query that does not return rows
int db_execute(MYSQL* conn, const char* sql);
// helper: check connection liveness
int db_ping(MYSQL* conn);

// Group Request Functions
int db_create_group_request(MYSQL* conn, int user_id, int group_id, const char* type);
int db_update_request_status(MYSQL* conn, int request_id, const char* status);
int db_add_group_member(MYSQL* conn, int user_id, int group_id, const char* role);
int db_remove_group_member(MYSQL* conn, int user_id, int group_id);
// Note: For simplicity in this C demo, we might just print results or return a string.
// In a real app, we would return a struct list.
void db_list_pending_requests(MYSQL* conn, int user_id, char* buffer, size_t size);

// Helpers for new features
int db_is_group_admin(MYSQL* conn, int user_id, int group_id);
int db_is_group_member(MYSQL* conn, int user_id, int group_id);
int db_get_group_id_for_user_by_name(MYSQL* conn, int user_id, const char* group_name);
int db_get_user_id_by_name(MYSQL* conn, const char* username);
int db_check_group_exists(MYSQL* conn, int group_id);
void db_list_all_groups(MYSQL* conn, char* buffer, size_t size);
void db_get_group_name(MYSQL* conn, int group_id, char* buffer, size_t size);

// Enhanced group management queries
void db_list_admin_groups(MYSQL* conn, int user_id, char* buffer, size_t size);
void db_list_non_members(MYSQL* conn, int group_id, char* buffer, size_t size);
void db_list_group_members(MYSQL* conn, int group_id, char* buffer, size_t size);
void db_list_group_members_with_roles(MYSQL* conn, int group_id, char* buffer, size_t size);
void db_list_join_requests_for_admin(MYSQL* conn, int user_id, char* buffer, size_t size);
void db_list_invitations_for_user(MYSQL* conn, int user_id, char* buffer, size_t size);
void db_list_joinable_groups(MYSQL* conn, int user_id, char* buffer, size_t size);
void db_list_my_groups(MYSQL* conn, int user_id, char* buffer, size_t size);

// File Management Functions
void db_list_files(MYSQL* conn, int group_id, int parent_id, char* buffer, size_t size);
int db_create_folder(MYSQL* conn, int group_id, const char* name, const char* path, int uploaded_by, int parent_id);
int db_rename_item(MYSQL* conn, int item_id, const char* new_name);
int db_delete_item(MYSQL* conn, int item_id);
int db_copy_item(MYSQL* conn, int item_id, int uploaded_by, int dest_parent_id);
int db_move_item(MYSQL* conn, int item_id, int new_group_id, int new_parent_id);
void db_list_user_groups(MYSQL* conn, int user_id, char* buffer, size_t size);
int db_add_file(MYSQL* conn, int group_id, const char* name, const char* path, long long size, int uploaded_by, int parent_id);

// Hàm quan trọng mới: Dịch đường dẫn "Group/Folder/..." thành ID
// return_type: 0 = lấy ID thư mục cuối (để upload), 1 = lấy ID file cuối (để download)
// Trả về: ID tìm được, hoặc -1 nếu lỗi/không tìm thấy.
// Output: Ghi group_id tìm được vào biến con trỏ *out_group_id
int db_resolve_path(MYSQL* conn, const char* full_path, int return_type, int *out_group_id);

#endif // DATABASE_H
