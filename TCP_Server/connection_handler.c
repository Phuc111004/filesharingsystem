#include "connection_handler.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>


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
