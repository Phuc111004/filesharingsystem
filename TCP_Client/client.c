#include "client.h"
#include <stdio.h>


#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../common/protocol.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5500

void print_menu(); // defined in ui.c

void run_client() {
    int sock;
    struct sockaddr_in server_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed");
        return;
    }

    printf("Connected to server.\n");

    char buffer[1024];
    // int user_id = 1; // Mock user ID removed as it's unused now

    while (1) {
        print_menu();
        printf("> ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
        buffer[strcspn(buffer, "\n")] = 0; // remove newline

        char cmd[64], arg1[64], arg2[64];
        int n = sscanf(buffer, "%s %s %s", cmd, arg1, arg2);

        if (n < 1) continue;

        char msg[256] = {0};

        if (strcmp(cmd, "join_group") == 0) {
            // join_group <group_id> -> JOIN_REQ <group_id>
            sprintf(msg, "JOIN_REQ %s", arg1);
        } else if (strcmp(cmd, "leave_group") == 0) {
            // leave_group <group_id> -> LEAVE_GROUP <group_id>
            sprintf(msg, "LEAVE_GROUP %s", arg1);
        } else if (strcmp(cmd, "list_requests") == 0) {
            // list_requests <group_id> -> LIST_REQ <group_id>
            sprintf(msg, "LIST_REQ %s", arg1);
        } else if (strcmp(cmd, "approve") == 0) {
            // approve <req_id> <A/R> -> APPROVE <req_id> <A/R>
            sprintf(msg, "APPROVE %s %s", arg1, arg2);
        } else if (strcmp(cmd, "kick") == 0) {
            // kick <username> <group_id> -> KICK <username> <group_id>
            sprintf(msg, "KICK %s %s", arg1, arg2);
        } else if (strcmp(cmd, "invite") == 0) {
            // invite <username> <group_id> -> INVITE <username> <group_id>
            sprintf(msg, "INVITE %s %s", arg1, arg2);
        } else {
            printf("Unknown command locally. Sending raw...\n");
            strcpy(msg, buffer);
        }

        send(sock, msg, strlen(msg), 0);
        
        memset(buffer, 0, sizeof(buffer));
        recv(sock, buffer, sizeof(buffer)-1, 0);
        printf("Server Response: %s\n", buffer);
    }

    close(sock);
}


int main() {
run_client();
return 0;
}
