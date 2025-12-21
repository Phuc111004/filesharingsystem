#include "server.h"
#include "connection_handler.h"
#include <stdio.h>
#include "../database/database.h"
#include <errno.h>


int main(int argc, char **argv) {
	printf("[server] starting...\n");

	int port = 5500;
	if (argc >= 2) {
		port = atoi(argv[1]);
		if (port <= 0) port = 5500;
	}

	MYSQL *db = db_connect();
	if (!db) {
		fprintf(stderr, "[server] ERROR: cannot connect to database. Exiting.\n");
		return 1;
	}

	if (db_ping(db) != 0) {
		fprintf(stderr, "[server] ERROR: database ping failed. Exiting.\n");
		db_close(db);
		return 1;
	}

	printf("[server] database connected successfully.\n");

	start_server(port);

	db_close(db);
	return 0;
}
