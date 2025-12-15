#include "handlers.h"
#include "../../common/file_utils.h"
#include "../../common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>

#define CHUNK_SIZE 4096

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

// Handler for Upload (Case 3)
void handle_upload(int sockfd) {
    char group[16], s_path[128], l_path[256], buffer[4096];
    
    printf("Group ID: "); scanf("%s", group);
    printf("Server Path: "); scanf("%s", s_path);
    printf("Local File Path: "); scanf("%s", l_path);
    
    snprintf(buffer, sizeof(buffer), "UPLOAD %s %s %s", group, s_path, l_path);
    handle_upload_command(sockfd, buffer);
}
