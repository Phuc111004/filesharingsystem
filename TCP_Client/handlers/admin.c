#include "handlers.h"
#include "../helpers/group_helper.h"
#include "../../common/file_utils.h"
#include "client_utils.h"
#include "../../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// Handler for Invite User (Case 8)
void handle_invite_user(int sockfd) {
    char buffer[32768], user[128];
    
    int selected_group_id = get_selected_admin_group(sockfd);
    if (selected_group_id == -1) return;
    
    // Get non-members list
    snprintf(buffer, sizeof(buffer), "LIST_NON_MEMBERS %d\n", selected_group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    int n = recv_multiline(sockfd, buffer, sizeof(buffer));
    
    if (n <= 0) return;
    buffer[n] = '\0';
    
    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nAvailable Users to Invite:\n%s\n", buffer);
    }
    
    if (strstr(buffer, "All users are already members") != NULL) {
        return;
    }

    if (is_error_response(buffer)) {
        return;
    }
    
    // Enter username to invite
    printf("Enter username to invite: ");
    scanf("%s", user);
    
    snprintf(buffer, sizeof(buffer), "INVITE %s %d\n", user, selected_group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv_line(sockfd, buffer, sizeof(buffer));
    printf("Server: %s\n", buffer);
}

// Handler for Kick Member (Case 11)
void handle_kick_member(int sockfd) {
    char buffer[32768], user[128];
    
    int selected_group_id = get_selected_admin_group(sockfd);
    if (selected_group_id == -1) return;
    
    // Get members list
    snprintf(buffer, sizeof(buffer), "LIST_MEMBERS %d\n", selected_group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    int n = recv_multiline(sockfd, buffer, sizeof(buffer));
    if (n <= 0) return;
    buffer[n] = '\0';
    
    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nGroup Members:\n%s\n", buffer);
    }

    if (strstr(buffer, "No members") != NULL) {
        return;
    }

    if (is_error_response(buffer)) {
        return;
    }
    
    // Enter username to kick
    printf("Enter username to kick: ");
    scanf("%s", user);
    
    // Confirm
    char confirm[10];
    printf("Are you sure? (Yes/No): ");
    scanf("%s", confirm);
    
    if (strcmp(confirm, "Yes") == 0 || strcmp(confirm, "yes") == 0) {
        snprintf(buffer, sizeof(buffer), "KICK %s %d\n", user, selected_group_id);
        send_all(sockfd, buffer, strlen(buffer));
        
        memset(buffer, 0, sizeof(buffer));
        recv(sockfd, buffer, sizeof(buffer)-1, 0);
        printf("Server: %s", buffer);
    } else {
        printf("Kick cancelled.\n");
    }
}
