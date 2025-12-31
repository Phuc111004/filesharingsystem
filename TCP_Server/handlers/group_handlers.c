#include "group_handlers.h"
#define RESPONSE_SIZE 4096
#include "../../database/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_join_req(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen) {
    // JOIN_REQ <group_id>
    int group_id = atoi(arg1);
    if (!db_check_group_exists(db, group_id)) {
        snprintf(response, maxlen, "404 Group not found");
    } else {
        int res = db_create_group_request(db, current_user_id, group_id, "join_request");
        if (res == 0) snprintf(response, maxlen, "200 Request sent");
        else if (res == -2) snprintf(response, maxlen, "409 Already joined/requested");
        else snprintf(response, maxlen, "500 Internal Server Error");
    }
}

void handle_leave_group(MYSQL *db, int current_user_id, const char *arg, char *response, size_t maxlen) {
    // LEAVE_GROUP <group_name/id>
    if (!arg || strlen(arg) == 0) {
        snprintf(response, maxlen, "400 Bad request");
        return;
    }

    int group_id = 0;
    int is_id = 1;
    for (int i = 0; arg[i]; i++) {
        if (arg[i] < '0' || arg[i] > '9') { is_id = 0; break; }
    }

    if (is_id) {
        group_id = atoi(arg);
    } else {
        group_id = db_get_group_id_for_user_by_name(db, current_user_id, arg);
    }
    if (group_id == -1) {
        snprintf(response, maxlen, "500 Internal Server Error");
        return;
    }
    if (group_id == 0) {
        snprintf(response, maxlen, "404 Group not found");
        return;
    }

    // Leader cannot leave the group
    if (db_is_group_admin(db, current_user_id, group_id)) {
        snprintf(response, maxlen, "403 Forbidden (Admin cannot leave)");
        return;
    }

    db_remove_group_member(db, current_user_id, group_id);
    snprintf(response, maxlen, "200 Left group");
}

void handle_list_pending_req(MYSQL *db, int current_user_id, const char *arg1, char *response, size_t maxlen) {
    // LIST_REQ <group_id>
    int group_id = atoi(arg1);
    if (!db_is_group_admin(db, current_user_id, group_id)) {
        snprintf(response, maxlen, "403 Forbidden");
    } else {
        // Use a somewhat large buffer for temp, but bounded by maxlen
        // Or better, pass just reference? logic is db_list writes to buffer.
        // We can write to response directly if we manage it?
        // But response is expected to have "100 Requests: " prefix here.
        // Let's alloc temp.
        char *temp = malloc(maxlen); 
        if (!temp) {
             snprintf(response, maxlen, "500 Memory Error");
             return;
        }
        db_list_pending_requests(db, current_user_id, temp, maxlen);
        snprintf(response, maxlen, "Requests: %s", temp);
        free(temp);
    }
}

void handle_approve_req(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen) {
    // APPROVE <req_id> <A/R>
    int req_id = atoi(arg1);
    char type = arg2[0]; // 'A' or 'R'
    
    // Fetch user_id and group_id from the request
    char query[512];
    snprintf(query, sizeof(query),
        "SELECT user_id, group_id FROM group_requests WHERE request_id = %d", req_id);
    
    if (mysql_query(db, query)) {
        snprintf(response, maxlen, "500 Database error");
        return;
    }
    
    MYSQL_RES *res = mysql_store_result(db);
    MYSQL_ROW row = mysql_fetch_row(res);
    
    if (!row) {
        snprintf(response, maxlen, "404 Request not found");
        mysql_free_result(res);
        return;
    }
    
    int user_id = atoi(row[0]);
    int group_id = atoi(row[1]);
    mysql_free_result(res);
    
    if (type == 'A') {
        // Update request status to accepted
        db_update_request_status(db, req_id, "accepted");
        
        // Add user to group
        int result = db_add_group_member(db, user_id, group_id, "member");
        if (result == 0) {
            snprintf(response, maxlen, "200 Request accepted, user added to group");
        } else {
            snprintf(response, maxlen, "200 Request accepted but failed to add user to group");
        }
    } else if (type == 'R') {
        db_update_request_status(db, req_id, "rejected");
        snprintf(response, maxlen, "200 Request rejected");
    } else {
        snprintf(response, maxlen, "400 Bad Request");
    }
}

void handle_kick_member(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen) {
    // KICK <username> <group_id>
    const char *target_username = arg1;
    int group_id = atoi(arg2);

    if (!db_is_group_admin(db, current_user_id, group_id)) {
        snprintf(response, maxlen, "403 Forbidden");
    } else {
        int target_id = db_get_user_id_by_name(db, target_username);
        if (target_id == -1) {
            snprintf(response, maxlen, "404 User not found");
        } else {
            // Check if target is also an admin
            if (db_is_group_admin(db, target_id, group_id)) {
                 snprintf(response, maxlen, "403 Cannot kick admin");
            } else {
                db_remove_group_member(db, target_id, group_id);
                snprintf(response, maxlen, "200 User kicked");
            }
        }
    }
}

void handle_invite_user(MYSQL *db, int current_user_id, const char *arg1, const char *arg2, char *response, size_t maxlen) {
    // INVITE <username> <group_id>
    const char *target_username = arg1;
    int group_id = atoi(arg2);

    if (!db_check_group_exists(db, group_id)) {
        snprintf(response, maxlen, "404 Group not found");
    } else {
        int target_id = db_get_user_id_by_name(db, target_username);
        if (target_id == -1) {
            snprintf(response, maxlen, "404 User not found");
        } else {
            int res = db_create_group_request(db, target_id, group_id, "invitation");
            if (res == 0) snprintf(response, maxlen, "200 Invitation sent");
            else if (res == -2) snprintf(response, maxlen, "409 Already member/invited");
            else snprintf(response, maxlen, "500 Error");
        }
    }
}

void handle_list_group_members(MYSQL *db, int current_user_id, const char *arg, char *response, size_t maxlen) {
    if (!arg || strlen(arg) == 0) {
        snprintf(response, maxlen, "400 Bad request");
        return;
    }

    int group_id = 0;
    int is_id = 1;
    for (int i = 0; arg[i]; i++) {
        if (arg[i] < '0' || arg[i] > '9') { is_id = 0; break; }
    }

    if (is_id) {
        group_id = atoi(arg);
        // Optional: verify membership? db_list_group_members_with_roles will only return data if id exists.
    } else {
        group_id = db_get_group_id_for_user_by_name(db, current_user_id, arg);
    }
    if (group_id == -1) {
        snprintf(response, maxlen, "500 Internal Server Error");
        return;
    }
    if (group_id == 0) {
        snprintf(response, maxlen, "404 Group not found");
        return;
    }

    char list_buf[3500];
    db_list_group_members_with_roles(db, group_id, list_buf, sizeof(list_buf));
    snprintf(response, maxlen, "100 Member List:\n%s", list_buf);
}