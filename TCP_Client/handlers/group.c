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

    // [SỬA] Đổi \n thành \r\n
    snprintf(buffer, sizeof(buffer), "CREATE_GROUP %s\r\n", group_name);
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n > 0) buffer[n] = '\0';
    
    printf("Server: %s", buffer);
}

// Handler for Join Group (Case 7)
void handle_join_group(int sockfd) {
    char buffer[32768];
    
    // Request list of groups
    // [SỬA] Đổi \n thành \r\n
    snprintf(buffer, sizeof(buffer), "LIST_JOINABLE_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    
    // Display groups
    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nAvailable Groups:\n%s\n", buffer);
    }

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
    
    // [SỬA] Đổi \n thành \r\n
    snprintf(buffer, sizeof(buffer), "JOIN_REQ %d\r\n", selected_group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv_response(sockfd, buffer, sizeof(buffer));
    printf("Server: %s\n", buffer);
}

// Handler for List Groups (Case 5)
void handle_list_groups(int sockfd) {
    char buffer[16384];

    snprintf(buffer, sizeof(buffer), "LIST_USER_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }

    printf("\nYour Groups:\n%s\n", buffer);
}

// Handler for List Group Members (Case 6)
void handle_list_group_members(int sockfd) {
    char buffer[32768];
    char group_name[256];

    // B1: List user's groups
    snprintf(buffer, sizeof(buffer), "LIST_USER_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    printf("\nYour Groups:\n%s\n", buffer);

    // B2: Prompt for group name
    int c; while ((c = getchar()) != '\n' && c != EOF) {} // clear newline
    printf("Enter group name: ");
    if (!fgets(group_name, sizeof(group_name), stdin)) return;
    trim_newline(group_name);
    if (strlen(group_name) == 0) return;

    // B3: Send LIST_MEM <group_name>
    snprintf(buffer, sizeof(buffer), "LIST_MEM %s\r\n", group_name);
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    printf("\n%s\n", buffer);
}

// Handler for Leave Group (Case 12)
void handle_leave_group(int sockfd) {
    char buffer[32768];
    char group_name[256];

    // B1: List user's groups
    snprintf(buffer, sizeof(buffer), "LIST_USER_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    printf("\nYour Groups:\n%s\n", buffer);

    // B2: Prompt for group name
    int c; while ((c = getchar()) != '\n' && c != EOF) {} // clear newline
    printf("Enter group name to leave: ");
    if (!fgets(group_name, sizeof(group_name), stdin)) return;
    trim_newline(group_name);
    if (strlen(group_name) == 0) return;

    // B3: Send LEAVE_GROUP <group_name>
    snprintf(buffer, sizeof(buffer), "LEAVE_GROUP %s\r\n", group_name);
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return;
    }
    buffer[n] = '\0';
    printf("Server: %s\n", buffer);
}