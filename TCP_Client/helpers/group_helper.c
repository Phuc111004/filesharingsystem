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