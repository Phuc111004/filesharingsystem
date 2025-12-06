#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H


#include <netinet/in.h>


void start_server();
void* client_thread(void* arg);


#endif // CONNECTION_HANDLER_H
