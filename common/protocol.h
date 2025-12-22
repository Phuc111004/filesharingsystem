#ifndef PROTOCOL_H
#define PROTOCOL_H

// --- COMMAND STRINGS (for text protocol parsing) ---
#define STR_REGISTER "REGISTER"
#define STR_LOGIN "LOGIN"
#define STR_LOGOUT "LOGOUT"

#define STR_CREATE_GROUP "CREATE_GROUP"
#define STR_LIST_GROUPS "LIST_GROUPS"
#define STR_JOIN_GROUP "JOIN_REQ"
#define STR_LIST_MEMBERS "LIST_MEM"
#define STR_LEAVE_GROUP "LEAVE_GROUP"

#define STR_LIST_REQUESTS "LIST_REQ"
#define STR_APPROVE_REQUEST "APPROVE"
#define STR_KICK_MEMBER "KICK"
#define STR_INVITE "INVITE"

#define STR_LIST_FILE "LIST_FILE"
#define STR_MKDIR "MKDIR"
#define STR_UPLOAD "UPLOAD"
#define STR_DOWNLOAD "DOWNLOAD"
#define STR_COPY "COPY"
#define STR_DELETE "DELETE"
#define STR_RENAME "RENAME"
#define STR_MOVE "MOVE"

// --- RESPONSE CODES ---
// Success
#define RES_LIST_DATA 100        // Data follows
#define RES_LOGIN_SUCCESS 110
#define RES_DOWNLOAD_START 102
#define RES_UPLOAD_READY 150

#define RES_SUCCESS 200
#define RES_REGISTER_SUCCESS 201
#define RES_LOGOUT_SUCCESS 202
#define RES_END_DATA 203

// Client Errors
#define RES_BAD_REQUEST 400
#define RES_LOGIN_FAILED 401
#define RES_FORBIDDEN 403
#define RES_NOT_FOUND 404
#define RES_CONFLICT 409 // Exists, Already logged in, etc.

// Server Errors
#define RES_SERVER_ERROR 500
#define RES_UPLOAD_FAILED 500
//thư mục không tồn tại, nhóm không tồn tại, người dùng chưa đăng nhập

#endif // PROTOCOL_H
