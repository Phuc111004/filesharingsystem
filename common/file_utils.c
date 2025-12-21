#include "file_utils.h"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/socket.h>

ssize_t send_all(int sockfd, const void* buf, size_t len) {
    char* p = (char*)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sockfd, p + total, len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

ssize_t recv_all(int sockfd, void* buf, size_t len) {
    char* p = (char*)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(sockfd, p + total, len - total, 0);
        if (n <= 0) return -1; // Lỗi hoặc đóng kết nối
        total += n;
    }
    return total;
}
