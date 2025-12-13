#include "connection_handler.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h> 

void start_server() {
// TODO: implement socket(), bind(), listen(), accept() loop
// For each accepted client create a pthread running client_thread
printf("[server] start_server() called (TODO implement)\n");
}


void* client_thread(void* arg) {
int client_sock = *(int*)arg;
// detach and free arg in caller
printf("[server] client thread (socket=%d) started (TODO)\n", client_sock);


// TODO: receive requests, parse protocol, call DB functions, send responses


close(client_sock);
return NULL;
}

void handle_upload(int client_sock) {
    file_data meta;
    
    if (recv_all(client_sock, &meta, sizeof(meta)) <= 0) {
        printf("[Server] Failed to receive metadata.\n");
        return;
    }

    printf("[Server] Receiving file: %s (%lld bytes)\n", meta.filename, meta.filesize);

    struct stat st = {0};
    if (stat("storage", &st) == -1) {
        mkdir("storage", 0700);
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "storage/%s", meta.filename);

}