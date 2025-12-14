#ifndef QUERIES_H
#define QUERIES_H


#include <mysql/mysql.h>
#include "database.h"


// Example prototypes
int db_create_user(MYSQL* conn, const char* username, const char* password_hash);
// Returns 1 if user exists, 0 if not, -1 on error
int db_user_exists(MYSQL* conn, const char* username);

// Verify username/password: returns 0 on success, 1 on invalid credentials, -1 on error
int db_verify_user(MYSQL* conn, const char* username, const char* password);


// Add more prototypes for groups, requests, directories, logs...


#endif // QUERIES_H
