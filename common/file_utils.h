#ifndef FILE_UTILS_H
#define FILE_UTILS_H


#include <sys/types.h>


ssize_t send_all(int sockfd, void* buf, size_t len);
ssize_t recv_all(int sockfd, void* buf, size_t len);


#endif // FILE_UTILS_H
