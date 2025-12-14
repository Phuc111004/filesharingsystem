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

#define SERVER_PORT 5500 // Configurable
#define SERVER_IP "127.0.0.1"
#define CHUNK_SIZE 4096

// Helper to check if string starts with prefix
int startswith(const char *str, const char *pre) {
    return strncmp(pre, str, strlen(pre)) == 0;
}

void handle_upload_command(int sockfd, char *input_line) {
    // Expected Input: UPLOAD <group_id> <server_path> <local_file_path>
    // Protocol Send: UPLOAD <group_id> <server_path> <filename> <filesize>
    
    char cmd[16], group_id[16], server_path[128], local_path[256];
    int parsed = sscanf(input_line, "%s %s %s %s", cmd, group_id, server_path, local_path);
    
    if (parsed != 4) {
        printf("Usage: UPLOAD <group_id> <server_path> <local_file_path>\n");
        return;
    }

    FILE *fp = fopen(local_path, "rb");
    if (!fp) {
        perror("Cannot open local file");
        return;
    }

    // Get filesize
    struct stat st;
    if (stat(local_path, &st) == -1) {
        perror("Cannot stat file");
        fclose(fp);
        return;
    }
    long long filesize = st.st_size;

    // Get filename from local_path
    char *filename = strrchr(local_path, '/');
    if (!filename) filename = strrchr(local_path, '\\');
    if (filename) filename++; else filename = local_path;

    // Construct Protocol String
    char proto_msg[512];
    snprintf(proto_msg, sizeof(proto_msg), "UPLOAD %s %s %s %lld\n", group_id, server_path, filename, filesize);
    
    send_all(sockfd, proto_msg, strlen(proto_msg));

    // Wait for "150 Ready" (or error)
    char response[256] = {0};
    ssize_t n = recv(sockfd, response, sizeof(response)-1, 0);
    if (n <= 0) {
        printf("Server disconnected during handshake.\n");
        fclose(fp);
        return;
    }
    response[n] = '\0';

    int code;
    sscanf(response, "%d", &code);
    if (code != 150) {
        printf("Server Error: %s", response);
        fclose(fp);
        return;
    }

    printf("Server ready. Uploading %s (%lld bytes)...\n", filename, filesize);

    // Send Bytes
    char buffer[CHUNK_SIZE];
    size_t nb;
    while ((nb = fread(buffer, 1, CHUNK_SIZE, fp)) > 0) {
        if (send_all(sockfd, buffer, nb) < 0) {
            perror("Send error");
            break;
        }
    }
    fclose(fp);
    
    // Wait for Final Response
    memset(response, 0, sizeof(response));
    n = recv(sockfd, response, sizeof(response)-1, 0);
    if (n > 0) {
        response[n] = '\0';
        printf("%s", response);
    } else {
        printf("Upload complete (No final response from server).\n");
    }
}

void run_client() {
    int sockfd;
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return;
    }

    printf("Connected to server %s:%d\n", SERVER_IP, SERVER_PORT);

    int choice;
    char buffer[4096];
    char user[128], pass[128];
    
    while (1) {
        print_menu();
        printf("Select: ");
        if (scanf("%d", &choice) != 1) {
            // clear stdin
            while(getchar() != '\n'); 
            continue;
        }

        switch (choice) {
            case 1: // Login
                printf("Username: "); scanf("%s", user);
                printf("Password: "); scanf("%s", pass);
                snprintf(buffer, sizeof(buffer), "LOGIN %s %s\n", user, pass);
                send_all(sockfd, buffer, strlen(buffer));
                
                // Read response
                memset(buffer, 0, sizeof(buffer));
                recv(sockfd, buffer, sizeof(buffer)-1, 0);
                printf("Server: %s", buffer);
                break;

            case 2: // Register
                printf("Username: "); scanf("%s", user);
                printf("Password: "); scanf("%s", pass);
                snprintf(buffer, sizeof(buffer), "REGISTER %s %s\n", user, pass);
                send_all(sockfd, buffer, strlen(buffer));
                
                // Read response
                memset(buffer, 0, sizeof(buffer));
                recv(sockfd, buffer, sizeof(buffer)-1, 0);
                printf("Server: %s", buffer);
                break;

            case 3: // Upload
                {
                    char group[16], s_path[128], l_path[256];
                    printf("Group ID: "); scanf("%s", group);
                    printf("Server Path: "); scanf("%s", s_path);
                    printf("Local File Path: "); scanf("%s", l_path);
                    
                    // Reconstruct UPLOAD command string for helper
                    snprintf(buffer, sizeof(buffer), "UPLOAD %s %s %s", group, s_path, l_path);
                    handle_upload_command(sockfd, buffer);
                }
                break;

            case 4: // Create Group
                printf("Feature under development\n");
                break;

            case 5: // List Groups
                printf("Feature under development\n");
                break;

            case 6: // List Members
                printf("Feature under development\n");
                break;

            case 7: // Join Group
                {
                    // Step 1: Request list of groups from server
                    snprintf(buffer, sizeof(buffer), "LIST_GROUPS\n");
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    if (n <= 0) {
                        printf("Server disconnected.\n");
                        break;
                    }
                    buffer[n] = '\0';
                    
                    // Step 2: Display the list
                    printf("\nAvailable Groups:\n");
                    printf("%s\n", buffer);
                    
                    // Step 3: Parse group IDs from response
                    // Format: [num] GroupName (Admin: username) [ID: group_id]
                    int group_ids[100]; // Support up to 100 groups
                    int group_count = 0;
                    
                    char *line = strtok(buffer, "\n");
                    while (line != NULL && group_count < 100) {
                        // Extract group_id from [ID: xxx] pattern
                        char *id_start = strstr(line, "[ID: ");
                        if (id_start) {
                            id_start += 5; // Move past "[ID: "
                            char *id_end = strchr(id_start, ']');
                            if (id_end) {
                                char id_str[16];
                                int len = id_end - id_start;
                                if (len < 16) {
                                    strncpy(id_str, id_start, len);
                                    id_str[len] = '\0';
                                    group_ids[group_count++] = atoi(id_str);
                                }
                            }
                        }
                        line = strtok(NULL, "\n");
                    }
                    
                    if (group_count == 0) {
                        printf("No groups available to join.\n");
                        break;
                    }
                    
                    // Step 4: Prompt user to select a number
                    int selection;
                    printf("\nEnter number to join (1-%d): ", group_count);
                    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
                        printf("Invalid selection.\n");
                        while(getchar() != '\n'); // Clear input buffer
                        break;
                    }
                    
                    // Step 5: Send JOIN_REQ with selected group_id
                    int selected_group_id = group_ids[selection - 1];
                    snprintf(buffer, sizeof(buffer), "JOIN_REQ %d\n", selected_group_id);
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    printf("Server: %s", buffer);
                }
                break;

            case 8: // Invite User (Enhanced)
                {
                    // Step 1: Get list of admin groups
                    snprintf(buffer, sizeof(buffer), "LIST_ADMIN_GROUPS\n");
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    if (n <= 0) {
                        printf("Server disconnected.\n");
                        break;
                    }
                    buffer[n] = '\0';
                    
                    printf("\nYour Admin Groups:\n%s\n", buffer);
                    
                    // Parse group IDs
                    int group_ids[100];
                    int group_count = 0;
                    char *line = strtok(buffer, "\n");
                    while (line != NULL && group_count < 100) {
                        char *id_start = strstr(line, "[ID: ");
                        if (id_start) {
                            id_start += 5;
                            char *id_end = strchr(id_start, ']');
                            if (id_end) {
                                char id_str[16];
                                int len = id_end - id_start;
                                if (len < 16) {
                                    strncpy(id_str, id_start, len);
                                    id_str[len] = '\0';
                                    group_ids[group_count++] = atoi(id_str);
                                }
                            }
                        }
                        line = strtok(NULL, "\n");
                    }
                    
                    if (group_count == 0) {
                        printf("You are not admin of any groups.\n");
                        break;
                    }
                    
                    // Step 2: Select group
                    int selection;
                    printf("Select group number (1-%d): ", group_count);
                    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
                        printf("Invalid selection.\n");
                        while(getchar() != '\n');
                        break;
                    }
                    int selected_group_id = group_ids[selection - 1];
                    
                    // Step 3: Get non-members list
                    snprintf(buffer, sizeof(buffer), "LIST_NON_MEMBERS %d\n", selected_group_id);
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    if (n <= 0) break;
                    buffer[n] = '\0';
                    
                    printf("\nAvailable Users to Invite:\n%s\n", buffer);
                    
                    // Step 4: Enter username to invite
                    printf("Enter username to invite: ");
                    scanf("%s", user);
                    
                    snprintf(buffer, sizeof(buffer), "INVITE %s %d\n", user, selected_group_id);
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    printf("Server: %s", buffer);
                }
                break;

            case 9: // Approve Request (Enhanced)
                {
                    // Step 1: Get list of pending join requests
                    snprintf(buffer, sizeof(buffer), "LIST_JOIN_REQUESTS\n");
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    if (n <= 0) {
                        printf("Server disconnected.\n");
                        break;
                    }
                    buffer[n] = '\0';
                    
                    printf("\nPending Join Requests:\n%s\n", buffer);
                    
                    // Parse request IDs
                    int req_ids[100];
                    int req_count = 0;
                    char *line = strtok(buffer, "\n");
                    while (line != NULL && req_count < 100) {
                        char *id_start = strstr(line, "[ReqID: ");
                        if (id_start) {
                            id_start += 8;
                            char *id_end = strchr(id_start, ']');
                            if (id_end) {
                                char id_str[16];
                                int len = id_end - id_start;
                                if (len < 16) {
                                    strncpy(id_str, id_start, len);
                                    id_str[len] = '\0';
                                    req_ids[req_count++] = atoi(id_str);
                                }
                            }
                        }
                        line = strtok(NULL, "\n");
                    }
                    
                    if (req_count == 0) {
                        printf("No pending requests.\n");
                        break;
                    }
                    
                    // Step 2: Select request
                    int selection;
                    printf("Select request number (1-%d): ", req_count);
                    if (scanf("%d", &selection) != 1 || selection < 1 || selection > req_count) {
                        printf("Invalid selection.\n");
                        while(getchar() != '\n');
                        break;
                    }
                    int selected_req_id = req_ids[selection - 1];
                    
                    // Step 3: Accept or Reject
                    char action[10];
                    printf("Accept (A) or Reject (R)? ");
                    scanf("%s", action);
                    
                    snprintf(buffer, sizeof(buffer), "APPROVE %d %s\n", selected_req_id, action);
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    printf("Server: %s", buffer);
                }
                break;

            case 10: // Accept Invitation (Enhanced)
                {
                    // Step 1: Get list of pending invitations
                    snprintf(buffer, sizeof(buffer), "LIST_MY_INVITATIONS\n");
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    if (n <= 0) {
                        printf("Server disconnected.\n");
                        break;
                    }
                    buffer[n] = '\0';
                    
                    printf("\nYour Pending Invitations:\n%s\n", buffer);
                    
                    // Parse invitation IDs
                    int inv_ids[100];
                    int inv_count = 0;
                    char *line = strtok(buffer, "\n");
                    while (line != NULL && inv_count < 100) {
                        char *id_start = strstr(line, "[InvID: ");
                        if (id_start) {
                            id_start += 8;
                            char *id_end = strchr(id_start, ']');
                            if (id_end) {
                                char id_str[16];
                                int len = id_end - id_start;
                                if (len < 16) {
                                    strncpy(id_str, id_start, len);
                                    id_str[len] = '\0';
                                    inv_ids[inv_count++] = atoi(id_str);
                                }
                            }
                        }
                        line = strtok(NULL, "\n");
                    }
                    
                    if (inv_count == 0) {
                        printf("No pending invitations.\n");
                        break;
                    }
                    
                    // Step 2: Select invitation
                    int selection;
                    printf("Select invitation number (1-%d): ", inv_count);
                    if (scanf("%d", &selection) != 1 || selection < 1 || selection > inv_count) {
                        printf("Invalid selection.\n");
                        while(getchar() != '\n');
                        break;
                    }
                    int selected_inv_id = inv_ids[selection - 1];
                    
                    // Step 3: Accept or Reject
                    char action[10];
                    printf("Accept (A) or Reject (R)? ");
                    scanf("%s", action);
                    
                    snprintf(buffer, sizeof(buffer), "APPROVE %d %s\n", selected_inv_id, action);
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    printf("Server: %s", buffer);
                }
                break;

            case 11: // Kick Member (Enhanced)
                {
                    // Step 1: Get list of admin groups
                    snprintf(buffer, sizeof(buffer), "LIST_ADMIN_GROUPS\n");
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    if (n <= 0) {
                        printf("Server disconnected.\n");
                        break;
                    }
                    buffer[n] = '\0';
                    
                    printf("\nYour Admin Groups:\n%s\n", buffer);
                    
                    // Parse group IDs
                    int group_ids[100];
                    int group_count = 0;
                    char *line = strtok(buffer, "\n");
                    while (line != NULL && group_count < 100) {
                        char *id_start = strstr(line, "[ID: ");
                        if (id_start) {
                            id_start += 5;
                            char *id_end = strchr(id_start, ']');
                            if (id_end) {
                                char id_str[16];
                                int len = id_end - id_start;
                                if (len < 16) {
                                    strncpy(id_str, id_start, len);
                                    id_str[len] = '\0';
                                    group_ids[group_count++] = atoi(id_str);
                                }
                            }
                        }
                        line = strtok(NULL, "\n");
                    }
                    
                    if (group_count == 0) {
                        printf("You are not admin of any groups.\n");
                        break;
                    }
                    
                    // Step 2: Select group
                    int selection;
                    printf("Select group number (1-%d): ", group_count);
                    if (scanf("%d", &selection) != 1 || selection < 1 || selection > group_count) {
                        printf("Invalid selection.\n");
                        while(getchar() != '\n');
                        break;
                    }
                    int selected_group_id = group_ids[selection - 1];
                    
                    // Step 3: Get members list
                    snprintf(buffer, sizeof(buffer), "LIST_MEMBERS %d\n", selected_group_id);
                    send_all(sockfd, buffer, strlen(buffer));
                    
                    memset(buffer, 0, sizeof(buffer));
                    n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
                    if (n <= 0) break;
                    buffer[n] = '\0';
                    
                    printf("\nGroup Members:\n%s\n", buffer);
                    
                    // Step 4: Enter username to kick
                    printf("Enter username to kick: ");
                    scanf("%s", user);
                    
                    // Step 5: Confirm
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
                break;

            case 12: // Leave Group
                printf("Group ID: "); scanf("%s", user);
                snprintf(buffer, sizeof(buffer), "LEAVE_GROUP %s\n", user);
                send_all(sockfd, buffer, strlen(buffer));
                
                memset(buffer, 0, sizeof(buffer));
                recv(sockfd, buffer, sizeof(buffer)-1, 0);
                printf("Server: %s", buffer);
                break;

            case 13: // Logout
                snprintf(buffer, sizeof(buffer), "LOGOUT\n");
                send_all(sockfd, buffer, strlen(buffer));
                memset(buffer, 0, sizeof(buffer));
                recv(sockfd, buffer, sizeof(buffer)-1, 0);
                printf("Server: %s", buffer);
                break;

            case 0: // Exit
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
