#ifndef GROUP_HANDLERS_H
#define GROUP_HANDLERS_H

#include <mysql.h>

// Handle JOIN_REQ <group_id>
void handle_join_req(MYSQL *db, int current_user_id, const char *arg1, char *response);

// Handle LEAVE_GROUP <group_id>
void handle_leave_group(MYSQL *db, int current_user_id, const char *arg1, char *response);

// Handle LIST_REQ <group_id>
void handle_list_pending_req(MYSQL *db, int current_user_id, const char *arg1, char *response);

// Handle APPROVE <req_id> <A/R>
void handle_approve_req(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response);

// Handle KICK <username> <group_id>
void handle_kick_member(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response);

// Handle INVITE <username> <group_id>
void handle_invite_user(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response);

#endif
