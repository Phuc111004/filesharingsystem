#ifndef QUERIES_H
#define QUERIES_H


#include <mysql/mysql.h>
#include "database.h"


// Example prototypes
int db_create_user(MYSQL* conn, const char* username, const char* password_hash);
int db_get_user_by_username(MYSQL* conn, const char* username, MYSQL_ROW *out_row);


// Add more prototypes for groups, requests, directories, logs...


#endif // QUERIES_H
