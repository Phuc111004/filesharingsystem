#include "handlers.h"
#include "../../common/file_utils.h"
#include "client_utils.h"
#include "../../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// Handler for Approve Request (Case 9)
void handle_approve_request(int sockfd) {
    char buffer[16384];
    
    // Get pending join requests
    snprintf(buffer, sizeof(buffer), "LIST_JOIN_REQUESTS\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    ssize_t n = recv_line(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    
    printf("\nPending Join Requests:\n%s\n", buffer);
    
    // Parse request IDs
    int req_ids[100];
    int req_count = parse_ids_from_response(buffer, req_ids, 100, "[ReqID: ");
    
    if (req_count == 0) {
        return;
    }
    
    // Select request
    int selection;
    printf("Select request number (1-%d): ", req_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > req_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return;
    }
    int selected_req_id = req_ids[selection - 1];
    
    // Accept or Reject
    char action[10];
    printf("Accept (A) or Reject (R)? ");
    scanf("%s", action);
    
    snprintf(buffer, sizeof(buffer), "APPROVE %d %s\n", selected_req_id, action);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer)-1, 0);
    printf("Server: %s", buffer);
}

// Handler for Accept Invitation (Case 10)
void handle_accept_invitation(int sockfd) {
    char buffer[16384];
    
    // Get pending invitations
    snprintf(buffer, sizeof(buffer), "LIST_MY_INVITATIONS\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    ssize_t n = recv_line(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    
    printf("\nYour Pending Invitations:\n%s\n", buffer);
    
    // Parse invitation IDs
    int inv_ids[100];
    int inv_count = parse_ids_from_response(buffer, inv_ids, 100, "[InvID: ");
    
    if (inv_count == 0) {
        return;
    }
    
    // Select invitation
    int selection;
    printf("Select invitation number (1-%d): ", inv_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > inv_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return;
    }
    int selected_inv_id = inv_ids[selection - 1];
    
    // Accept or Reject
    char action[10];
    printf("Accept (A) or Reject (R)? ");
    scanf("%s", action);
    
    snprintf(buffer, sizeof(buffer), "APPROVE %d %s\n", selected_inv_id, action);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer)-1, 0);
    printf("Server: %s", buffer);
}
