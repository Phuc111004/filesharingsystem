#include "request_dispatcher.h"dispatch_request
#include "group_handlers.h"
#include "../../database/database.h"
#include <stdio.h>
#include <string.h>

void dispatch_request(MYSQL *db, int current_user_id, char *buffer, char *response) {
    char cmd[32] = {0};
    char arg1[32] = {0};
    char arg2[32] = {0};
    
    // Parse command
    int n = sscanf(buffer, "%s %s %s", cmd, arg1, arg2);
    if (n < 1) return; // Should be handled by caller if empty, but safe check here

    if (strcmp(cmd, "JOIN_REQ") == 0) {
        handle_join_req(db, current_user_id, arg1, response);
    } 
    else if (strcmp(cmd, "LEAVE_GROUP") == 0) {
        handle_leave_group(db, current_user_id, arg1, response);
    } 
    else if (strcmp(cmd, "LIST_REQ") == 0) {
        handle_list_pending_req(db, current_user_id, arg1, response);
    } 
    else if (strcmp(cmd, "APPROVE") == 0) {
        handle_approve_req(db, current_user_id, arg1, arg2, response);
    } 
    else if (strcmp(cmd, "KICK") == 0) {
        handle_kick_member(db, current_user_id, arg1, arg2, response);
    } 
    else if (strcmp(cmd, "INVITE") == 0) {
        handle_invite_user(db, current_user_id, arg1, arg2, response);
    } 
    else if (strcmp(cmd, "LIST_GROUPS") == 0) {
        db_list_all_groups(db, response, 4096);
    } 
    else if (strcmp(cmd, "LIST_ADMIN_GROUPS") == 0) {
        db_list_admin_groups(db, current_user_id, response, 4096);
    }
    else if (strcmp(cmd, "LIST_NON_MEMBERS") == 0) {
        int group_id = atoi(arg1);
        db_list_non_members(db, group_id, response, 4096);
    }
    else if (strcmp(cmd, "LIST_MEMBERS") == 0) {
        int group_id = atoi(arg1);
        db_list_group_members(db, group_id, response, 4096);
    }
    else if (strcmp(cmd, "LIST_JOIN_REQUESTS") == 0) {
        db_list_join_requests_for_admin(db, current_user_id, response, 4096);
    }
    else if (strcmp(cmd, "LIST_MY_INVITATIONS") == 0) {
        db_list_invitations_for_user(db, current_user_id, response, 4096);
    }
    else if (strcmp(cmd, "LIST_JOINABLE_GROUPS") == 0) {
        db_list_joinable_groups(db, current_user_id, response, 4096);
    }
    else {
        strcpy(response, "400 Unknown Command");
    }
}
