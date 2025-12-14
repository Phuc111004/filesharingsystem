#ifndef REQUEST_DISPATCHER_H
#define REQUEST_DISPATCHER_H

#include <mysql/mysql.h>

void dispatch_request(MYSQL *db, int current_user_id, char *buffer, char *response);

#endif
