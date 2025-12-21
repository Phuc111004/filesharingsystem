#include "handlers.h"
#include "../../common/file_utils.h"
#include "client_utils.h"
#include "../../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// Handler for Create Group (Case 4)
void handle_create_group(int sockfd) {
    char buffer[4096];
    char group_name[128];

    printf("Group name: ");
    if (scanf("%127s", group_name) != 1) {
        printf("Invalid group name.\n");
        while (getchar() != '\n' && getchar() != EOF) {} // clear stdin
        return;
    }

    snprintf(buffer, sizeof(buffer), "CREATE_GROUP %s\n", group_name);
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    recv_line(sockfd, buffer, sizeof(buffer));
    printf("Server: %s", buffer);
}

// Handler for Join Group (Case 7)
void handle_join_group(int sockfd) {
    char buffer[16384];
    
    // Request list of groups
    snprintf(buffer, sizeof(buffer), "LIST_JOINABLE_GROUPS\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    ssize_t n = recv_line(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    
    // Display groups
    printf("\nAvailable Groups:\n%s\n", buffer);

    if (strstr(buffer, "No joinable groups available.") != NULL) {
        return;
    }
    
    // Parse group IDs
    int group_ids[100];
    int group_count = parse_ids_from_response(buffer, group_ids, 100, "[ID: ");

    if (group_count <= 0) {
        return;
    }
    
    // Prompt user selection
    int selection;
    printf("\nEnter number to join (1-%d): ", group_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return;
    }
    
    // Send JOIN_REQ
    int selected_group_id = group_ids[selection - 1];
    snprintf(buffer, sizeof(buffer), "JOIN_REQ %d\n", selected_group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer)-1, 0);
    printf("Server: %s", buffer);
}

// Handler for Leave Group (Case 12)
void handle_leave_group(int sockfd) {
    char buffer[4096], group_id[16];
    
    printf("Group ID: "); scanf("%s", group_id);
    snprintf(buffer, sizeof(buffer), "LEAVE_GROUP %s\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer)-1, 0);
    printf("Server: %s", buffer);
}
