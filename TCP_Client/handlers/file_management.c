#include "file_management.h"
#include "client_utils.h"
#include "../../common/file_utils.h"
#include "../../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static void list_files_in_group(int sockfd, int group_id) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_FILES %d\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv_multiline(sockfd, buffer, sizeof(buffer));
    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nFiles in Group %d:\n%s\n", group_id, buffer);
    }
}

static int get_selected_file_id(int sockfd, int group_id) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_FILES %d\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    int n = recv_multiline(sockfd, buffer, sizeof(buffer));
    if (n <= 0) return -1;
    buffer[n] = '\0';

    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nFiles in Group %d:\n%s\n", group_id, buffer);
    }

    int ids[100];
    int count = parse_ids_from_response(buffer, ids, 100, "[ID: ");
    
    if (count == 0) {
        return -1;
    }

    int choice;
    printf("Select file number (1-%d): ", count);
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > count) {
        while(getchar() != '\n');
        printf("Invalid selection.\n");
        return -1;
    }
    while(getchar() != '\n');
    return ids[choice - 1];
}

static int get_selected_group_id(int sockfd) {
    char buffer[32768];
    snprintf(buffer, sizeof(buffer), "LIST_MY_GROUPS\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    int n = recv_multiline(sockfd, buffer, sizeof(buffer));
    if (n <= 0) return -1;
    buffer[n] = '\0';

    if (is_error_response(buffer)) {
        printf("%s\n", buffer);
    } else {
        printf("\nYour Groups:\n%s\n", buffer);
    }

    int ids[100];
    int count = parse_ids_from_response(buffer, ids, 100, "[ID: ");
    
    if (count == 0) {
        return -1;
    }

    int choice;
    printf("Select group number to manage (1-%d): ", count);
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > count) {
        while(getchar() != '\n');
        printf("Invalid selection.\n");
        return -1;
    }
    while(getchar() != '\n');
    return ids[choice - 1];
}

void handle_file_management(int sockfd) {
    int group_id = get_selected_group_id(sockfd);
    if (group_id == -1) return;

    char group_name[256] = "Unknown";
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "GET_GROUP_NAME %d\n", group_id);
    send_all(sockfd, buffer, strlen(buffer));
    recv_line(sockfd, group_name, sizeof(group_name));

    while (1) {
        printf("\n--- File Management (Group: %s) ---\n", group_name);
        printf("1. List Files\n");
        printf("2. Rename File\n");
        printf("3. Delete File\n");
        printf("4. Copy File\n");
        printf("5. Move File\n");
        printf("6. Back to Main Menu\n");
        printf("Select: ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }
        while(getchar() != '\n');

        if (choice == 6) break;

        char buffer[1024];
        char resp[32768];
        int file_id;
        char new_name[256];

        switch (choice) {
            case 1:
                list_files_in_group(sockfd, group_id);
                break;
            case 2:
                file_id = get_selected_file_id(sockfd, group_id);
                if (file_id == -1) break;
                printf("Enter new name: ");
                scanf("%255s", new_name);
                while(getchar() != '\n');
                snprintf(buffer, sizeof(buffer), "RENAME_FILE %d %s\n", file_id, new_name);
                send_all(sockfd, buffer, strlen(buffer));
                recv_line(sockfd, resp, sizeof(resp));
                printf("Server: %s\n", resp);
                break;
            case 3:
                file_id = get_selected_file_id(sockfd, group_id);
                if (file_id == -1) break;
                snprintf(buffer, sizeof(buffer), "DELETE_FILE %d\n", file_id);
                send_all(sockfd, buffer, strlen(buffer));
                recv_line(sockfd, resp, sizeof(resp));
                printf("Server: %s\n", resp);
                break;
            case 4:
                file_id = get_selected_file_id(sockfd, group_id);
                if (file_id == -1) break;
                snprintf(buffer, sizeof(buffer), "COPY_FILE %d\n", file_id);
                send_all(sockfd, buffer, strlen(buffer));
                recv_line(sockfd, resp, sizeof(resp));
                printf("Server: %s\n", resp);
                break;
            case 5:
                file_id = get_selected_file_id(sockfd, group_id);
                if (file_id == -1) break;
                int target_group;
                printf("Enter target Group ID: ");
                if (scanf("%d", &target_group) != 1) {
                    while(getchar() != '\n');
                    break;
                }
                while(getchar() != '\n');
                snprintf(buffer, sizeof(buffer), "MOVE_FILE %d %d\n", file_id, target_group);
                send_all(sockfd, buffer, strlen(buffer));
                recv_line(sockfd, resp, sizeof(resp));
                printf("Server: %s\n", resp);
                break;
            default:
                printf("Invalid choice.\n");
                break;
        }
    }
}
