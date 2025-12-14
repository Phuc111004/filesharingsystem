#include "file_utils.h"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

ssize_t send_all(int sockfd, const void* buf, size_t len) {
    const char* p = (const char*)buf;
    size_t total_sent = 0;
    size_t bytes_left = len;
    ssize_t n;

    while (total_sent < len) {
        n = send(sockfd, p + total_sent, bytes_left, 0);
        
        if (n == -1) {
            // Nếu bị ngắt bởi tín hiệu (như Ctrl+C, SIGCHLD), thử gửi lại
            if (errno == EINTR) continue; 
            return -1; // Lỗi thật sự
        }
        
        total_sent += n;
        bytes_left -= n;
    }
    return total_sent;
}

// Nhận toàn bộ len bytes vào buf
ssize_t recv_all(int sockfd, void* buf, size_t len) {
    char* p = (char*)buf;
    size_t total_received = 0;
    size_t bytes_left = len;
    ssize_t n;

    while (total_received < len) {
        n = recv(sockfd, p + total_received, bytes_left, 0);
        
        if (n == -1) {
            if (errno == EINTR) continue; // Bị ngắt, thử nhận lại
            return -1; // Lỗi socket
        }
        
        if (n == 0) {
            return 0; // Đối phương đóng kết nối (EOF)
        }

        total_received += n;
        bytes_left -= n;
    }
    return total_received;
}
