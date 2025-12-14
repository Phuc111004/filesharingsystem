#include "file_utils.h"
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

ssize_t send_all(int sockfd, const void* buf, size_t len) {
    const char* p = (const char*)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sockfd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t recv_all(int sockfd, void* buf, size_t len) {
    char* p = (char*)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(sockfd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t recv_exact(int sockfd, void* buf, size_t len) {
    return recv_all(sockfd, buf, len);
}
