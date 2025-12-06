#ifndef DB_CONFIG_H
#define DB_CONFIG_H

// Kết nối bằng UNIX socket
#define DB_HOST ((const char*)NULL)   // rất quan trọng
#define DB_USER "root"
#define DB_PASS "123456"                  // không mật khẩu
#define DB_NAME "file_sharing"
#define DB_PORT 0                     // port = 0 khi dùng socket

#define DB_SOCKET "/var/run/mysqld/mysqld.sock"

#endif
