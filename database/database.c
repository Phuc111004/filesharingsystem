#include "database.h"
#include "../config/db_config.h"
#include <stdio.h>
#include <stdlib.h>


MYSQL* db_connect() {
    MYSQL *conn = mysql_init(NULL);
    if (!conn) {
        fprintf(stderr, "mysql_init() failed\n");
        return NULL;
    }

    // Dùng UNIX socket thay vì TCP
    if (!mysql_real_connect(conn,
                            DB_HOST,     // NULL → dùng socket
                            DB_USER,
                            DB_PASS,
                            DB_NAME,
                            DB_PORT,     // 0
                            DB_SOCKET,   // đường dẫn sock
                            0)) 
    {
        fprintf(stderr, "MySQL connect error: %s\n", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }

    mysql_set_character_set(conn, "utf8mb4");
    return conn;
}



void db_close(MYSQL* conn) {
if (conn) mysql_close(conn);
}


int db_execute(MYSQL* conn, const char* sql) {
if (mysql_query(conn, sql)) {
fprintf(stderr, "Query error: %s\nSQL: %s\n", mysql_error(conn), sql);
return -1;
}
return 0;
}


int db_ping(MYSQL* conn) {
	if (!conn) return -1;
	if (mysql_ping(conn) == 0) return 0;
	fprintf(stderr, "MySQL ping failed: %s\n", mysql_error(conn));
	return -1;
}
