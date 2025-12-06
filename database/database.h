#ifndef DATABASE_H
#define DATABASE_H


#include <mysql/mysql.h>


MYSQL* db_connect();
void db_close(MYSQL* conn);


// helper: execute a query that does not return rows
int db_execute(MYSQL* conn, const char* sql);
// helper: check connection liveness
int db_ping(MYSQL* conn);


#endif // DATABASE_H
