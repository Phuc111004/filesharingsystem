#ifndef HANDLERS_H
#define HANDLERS_H

#include <sys/types.h>

void handle_login(int sockfd);
void handle_register(int sockfd);
void handle_upload(int sockfd);
void handle_create_group(int sockfd);
void handle_join_group(int sockfd);
void handle_invite_user(int sockfd);
void handle_approve_request(int sockfd);
void handle_accept_invitation(int sockfd);
void handle_kick_member(int sockfd);
void handle_leave_group(int sockfd);
void handle_logout(int sockfd);

#endif // HANDLERS_H
