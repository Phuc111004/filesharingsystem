#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H


#include <netinet/in.h>


void start_server(int port);
void start_server(int port);
void* client_thread(void* arg);


#endif // CONNECTION_HANDLER_H
