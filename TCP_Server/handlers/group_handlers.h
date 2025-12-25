#ifndef GROUP_HANDLERS_H
#define GROUP_HANDLERS_H

#include <mysql.h>

// Handle JOIN_REQ <group_id>
// Handle JOIN_REQ <group_id>
void handle_join_req(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen);

// Handle LEAVE_GROUP <group_id>
// Handle LEAVE_GROUP <group_id>
void handle_leave_group(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen);

// Handle LIST_REQ <group_id>
// Handle LIST_REQ <group_id>
void handle_list_pending_req(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen);

// Handle APPROVE <req_id> <A/R>
// Handle APPROVE <req_id> <A/R>
void handle_approve_req(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen);

// Handle KICK <username> <group_id>
// Handle KICK <username> <group_id>
void handle_kick_member(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen);

// Handle INVITE <username> <group_id>
// Handle INVITE <username> <group_id>
void handle_invite_user(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen);

#endif
