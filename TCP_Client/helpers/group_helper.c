#include "group_helper.h"
#include "../../common/file_utils.h"
#include "../../common/utils.h"
#include "../handlers/client_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

int get_selected_admin_group(int sockfd) {
    char buffer[32768];
    
    // [SỬA] \n -> \r\n
    snprintf(buffer, sizeof(buffer), "LIST_ADMIN_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    // [SỬA] recv_line -> recv_response
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return -1;
    }
    
    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nYour Admin Groups:\n%s\n", buffer);
    }
    
    int group_ids[100];
    int group_count = parse_ids_from_response(buffer, group_ids, 100, "[ID: ");
    
    if (group_count == 0) return -1;
    
    int selection;
    printf("Select group number (1-%d): ", group_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return -1;
    }
    
    return group_ids[selection - 1];
}

int get_selected_group_id(int sockfd) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_MY_GROUPS\r\n");
    send_all(sockfd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    int n = recv_response(sockfd, buffer, sizeof(buffer));
    if (n <= 0) {
        printf("Server disconnected.\n");
        return -1;
    }

    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
        return -1;
    } else {
        printf("\nYour Groups:\n%s\n", buffer);
    }

    int group_ids[100];
    int group_count = parse_ids_from_response(buffer, group_ids, 100, "[ID: ");

    if (group_count == 0) return -1;

    int selection;
    printf("Select group number (1-%d): ", group_count);
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
        printf("Invalid selection.\n");
        while(getchar() != '\n');
        return -1;
    }
    while(getchar() != '\n'); // Consume newline

    return group_ids[selection - 1];
}

// Reuse logic from get_selected_folder but generic
int get_selected_folder_id(int sockfd, int group_id, const char* prompt) {
    // For now, let's just return Root (database root ID for group)
    // To implement folder selection properly, we need to list folders similar to Moving File
    // But user asked to fix Upload workflow.
    // The simplest fix is to fetch the group root ID.
    
    char buffer[1024];
    // Fetch Group Root ID
    int root_id = 0;
    snprintf(buffer, sizeof(buffer), "GET_GROUP_ROOT_ID %d\r\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    char root_id_str[64];
    recv_response(sockfd, root_id_str, sizeof(root_id_str));
    trim_newline(root_id_str);
    root_id = atoi(root_id_str);
    
    // TODO: Ideally show a folder picker. For now return Root ID.
    return root_id;
}