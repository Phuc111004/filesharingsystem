#include "handlers.h"
#include "../../common/file_utils.h"
#include "client_utils.h"
#include "../../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// Handler for Invite User (Case 8)
void handle_invite_user(int sockfd) {
    char buffer[4096], user[128];
    
    // Get admin groups
    snprintf(buffer, sizeof(buffer), "LIST_ADMIN_GROUPS\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    
    printf("\nYour Admin Groups:\n%s\n", buffer);
    
    // Parse group IDs
    int group_ids[100];
    int group_count = parse_ids_from_response(buffer, group_ids, 100, "[ID: ");
    
    if (group_count == 0) {
        return;
    }
    
    // Select group
    int selection;
    printf("Select group number (1-%d): ", group_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return;
    }
    int selected_group_id = group_ids[selection - 1];
    
    // Get non-members list
    snprintf(buffer, sizeof(buffer), "LIST_NON_MEMBERS %d\n", selected_group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) return;
    buffer[n] = '\0';
    
    printf("\nAvailable Users to Invite:\n%s\n", buffer);
    
    if (strstr(buffer, "All users are already members") != NULL) {
        return;
    }
    
    // Enter username to invite
    printf("Enter username to invite: ");
    scanf("%s", user);
    
    snprintf(buffer, sizeof(buffer), "INVITE %s %d\n", user, selected_group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer)-1, 0);
    printf("Server: %s", buffer);
}

// Handler for Kick Member (Case 11)
void handle_kick_member(int sockfd) {
    char buffer[4096], user[128];
    
    // Get admin groups
    snprintf(buffer, sizeof(buffer), "LIST_ADMIN_GROUPS\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    
    printf("\nYour Admin Groups:\n%s\n", buffer);
    
    // Parse group IDs
    int group_ids[100];
    int group_count = parse_ids_from_response(buffer, group_ids, 100, "[ID: ");
    
    if (group_count == 0) {
        return;
    }
    
    // Select group
    int selection;
    printf("Select group number (1-%d): ", group_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return;
    }
    int selected_group_id = group_ids[selection - 1];
    
    // Get members list
    snprintf(buffer, sizeof(buffer), "LIST_MEMBERS %d\n", selected_group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) return;
    buffer[n] = '\0';
    
    printf("\nGroup Members:\n%s\n", buffer);

    if (strstr(buffer, "No members") != NULL) {
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
