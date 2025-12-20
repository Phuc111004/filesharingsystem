#include "client.h"
#include "../common/protocol.h"
#include "../common/file_utils.h"
#include "../common/utils.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include "handlers/handlers.h"

#define SERVER_PORT 5500 // Configurable
#define SERVER_IP "127.0.0.1"
#define CHUNK_SIZE 4096

void run_client() {
    int sockfd;
    struct sockaddr_in serv_addr;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    // Setup server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        return;
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sockfd);
        return;
    }

    printf("Connected to server %s:%d\n", SERVER_IP, SERVER_PORT);

    int choice;
    
    // Main menu loop
    while (1) {
        print_menu();
        printf("Select: ");
        
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n'); // Clear stdin
            continue;
        }

        switch (choice) {
            case 1:  // Login
                handle_login(sockfd);
                break;

            case 2:  // Register
                handle_register(sockfd);
                break;

            case 3:  // Upload
                handle_upload(sockfd);
                brea.k;

            case 4:  // Create Group
                printf("Feature under development\n");
                break;

            case 5:  // List Groups
                printf("Feature under development\n");
                break;

            case 6:  // List Members
                printf("Feature under development\n");
                break;

            case 7:  // Join Group
                handle_join_group(sockfd);
                break;

            case 8:  // Invite User
                handle_invite_user(sockfd);
                break;

            case 9:  // Approve Request
                handle_approve_request(sockfd);
                break;

            case 10: // Accept Invitation
                handle_accept_invitation(sockfd);
                break;

            case 11: // Kick Member
                handle_kick_member(sockfd);
                break;

            case 12: // Leave Group
                handle_leave_group(sockfd);
                break;

            case 13: // Logout
                handle_logout(sockfd);
                break;

            case 0:  // Exit
                close(sockfd);
                return;

            default:
                printf("Unknown option.\n");
        }
    }

    close(sockfd);
}

int main(int argc, char **argv) {
    run_client();
    return 0;
}
