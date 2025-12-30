#include "request_dispatcher.h"
#include "group_handlers.h"
#include "file_handlers.h"
#include "../../database/database.h"
#include "../../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dispatch_request(MYSQL *db, int current_user_id, char *buffer, char *response, size_t maxlen) {
    char cmd[32] = {0};
    char arg1[256] = {0};
    char arg2[256] = {0};
    char arg3[256] = {0};
    
    // Parse command
    int n = sscanf(buffer, "%s %255s %255s %255s", cmd, arg1, arg2, arg3);
    if (n < 1) return;

    if (strcmp(cmd, "JOIN_REQ") == 0) {
        handle_join_req(db, current_user_id, arg1, response, maxlen);
    } 
    else if (strcmp(cmd, "LEAVE_GROUP") == 0) {
        handle_leave_group(db, current_user_id, arg1, response, maxlen);
    } 
    else if (strcmp(cmd, "LIST_REQ") == 0) {
        handle_list_pending_req(db, current_user_id, arg1, response, maxlen);
    } 
    else if (strcmp(cmd, "APPROVE") == 0) {
        handle_approve_req(db, current_user_id, arg1, arg2, response, maxlen);
    } 
    else if (strcmp(cmd, "KICK") == 0) {
        handle_kick_member(db, current_user_id, arg1, arg2, response, maxlen);
    } 
    else if (strcmp(cmd, "INVITE") == 0) {
        handle_invite_user(db, current_user_id, arg1, arg2, response, maxlen);
    } 
    else if (strcmp(cmd, "LIST_GROUPS") == 0) {
        db_list_all_groups(db, response, maxlen);
    } 
    else if (strcmp(cmd, "LIST_ADMIN_GROUPS") == 0) {
        db_list_admin_groups(db, current_user_id, response, maxlen);
    }
    else if (strcmp(cmd, "LIST_NON_MEMBERS") == 0) {
        int group_id = atoi(arg1);
        db_list_non_members(db, group_id, response, maxlen);
    }
    else if (strcmp(cmd, STR_LIST_MEMBERS) == 0) {
        handle_list_group_members(db, current_user_id, arg1, response, maxlen);
    }
    else if (strcmp(cmd, "LIST_JOIN_REQUESTS") == 0) {
        db_list_join_requests_for_admin(db, current_user_id, response, maxlen);
    }
    else if (strcmp(cmd, "LIST_MY_INVITATIONS") == 0) {
        db_list_invitations_for_user(db, current_user_id, response, maxlen);
    }
    else if (strcmp(cmd, "LIST_JOINABLE_GROUPS") == 0) {
        db_list_joinable_groups(db, current_user_id, response, maxlen);
    }
    else if (strcmp(cmd, "LIST_MY_GROUPS") == 0) {
        db_list_my_groups(db, current_user_id, response, maxlen);
    }
    else if (strcmp(cmd, "GET_GROUP_NAME") == 0) {
        int group_id = atoi(arg1);
        db_get_group_name(db, group_id, response, maxlen);
    }
    // --- File Management ---
    else if (strcmp(cmd, "LIST_FILE") == 0) {
        handle_list_files(db, current_user_id, arg1, arg2, response, maxlen);
    }
    else if (strcmp(cmd, "MKDIR") == 0) {
        handle_create_folder(db, current_user_id, arg1, arg2, arg3, response, maxlen);
    }
    else if (strcmp(cmd, "COPY") == 0) {
        handle_copy_file(db, current_user_id, arg1, arg2, arg3, response, maxlen);
    }
    else if (strcmp(cmd, "DELETE") == 0) {
        handle_delete_file(db, current_user_id, arg1, arg2, response, maxlen);
    }
    else if (strcmp(cmd, "RENAME") == 0) {
        handle_rename_file(db, current_user_id, arg1, arg2, arg3, response, maxlen);
    }
    else if (strcmp(cmd, "MOVE") == 0) {
        handle_move_file(db, current_user_id, arg1, arg2, arg3, response, maxlen);
    }
    else if (strcmp(cmd, "DOWNLOAD") == 0) {
        // Handled in connection_handler.c loop for binary streaming, 
        // but dispatcher might need to handle errors or headers.
        // For now, we'll let connection_handler handle the STR_DOWNLOAD.
        snprintf(response, maxlen, "501 Use Stream Protocol");
    }
    else if (strcmp(cmd, "LIST_USER_GROUPS") == 0) {
        db_list_user_groups(db, current_user_id, response, 4096);
    }
    else {
        snprintf(response, maxlen, "400 Unknown Command");
    }
}
