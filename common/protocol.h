#ifndef PROTOCOL_H
#define PROTOCOL_H


// Simple protocol opcodes
#define CMD_REGISTER 1
#define CMD_LOGIN 2
#define CMD_CREATE_GROUP 3
#define CMD_JOIN_GROUP 4
#define CMD_UPLOAD 5
#define CMD_DOWNLOAD 6
#define CMD_LIST_GROUPS 7


// message header example
typedef struct {
int cmd; // one of CMD_*
int length; // payload length in bytes
} __attribute__((packed)) msg_header_t;

// Thêm struct này để chứa thông tin file
typedef struct {
    char filename[256];
    long long filesize;
} __attribute__((packed)) file_data;

#endif // PROTOCOL_H
