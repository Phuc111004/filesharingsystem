#include "handlers.h"
#include "../../common/file_utils.h"
#include "../../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// Handler for Login (Case 1)
void handle_login(int sockfd) {
    char user[128], pass[128], buffer[4096];
    
    printf("Username: "); scanf("%s", user);
    printf("Password: "); scanf("%s", pass);
    snprintf(buffer, sizeof(buffer), "LOGIN %s %s\n", user, pass);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer)-1, 0);
    printf("Server: %s", buffer);
}

// Handler for Register (Case 2)
void handle_register(int sockfd) {
    char user[128], pass[128], buffer[4096];
    
    printf("Username: "); scanf("%s", user);
    printf("Password: "); scanf("%s", pass);
    snprintf(buffer, sizeof(buffer), "REGISTER %s %s\n", user, pass);
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer)-1, 0);
    printf("Server: %s", buffer);
}

// Handler for Logout (Case 13)
void handle_logout(int sockfd) {
    char buffer[4096];
    
    snprintf(buffer, sizeof(buffer), "LOGOUT\n");
    send_all(sockfd, buffer, strlen(buffer));
    
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer)-1, 0);
    printf("Server: %s", buffer);
}
