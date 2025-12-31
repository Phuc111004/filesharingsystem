#include "server.h"
#include "connection_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "../database/database.h"

static void print_local_ips(void) {
	struct ifaddrs *ifaddr = NULL;
	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		return;
	}

	printf("[server] Local IPv4 addresses (LAN/Wi-Fi):\n");
	for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
		// Skip down interfaces
		if (!(ifa->ifa_flags & IFF_UP)) continue;
		char addr_buf[INET_ADDRSTRLEN];
		const struct sockaddr_in *sa = (const struct sockaddr_in*)ifa->ifa_addr;
		if (inet_ntop(AF_INET, &sa->sin_addr, addr_buf, sizeof(addr_buf))) {
			printf("  - %s: %s\n", ifa->ifa_name, addr_buf);
		}
	}
	freeifaddrs(ifaddr);
}


int main(int argc, char **argv) {
	printf("[server] starting...\n");

	const char *bind_ip = "0.0.0.0"; // listen on all interfaces by default
	int port = 5500;

	// Usage: ./server [bind_ip] [port]; if only one arg is provided, treat it as port.
	if (argc >= 3) {
		bind_ip = argv[1];
		port = atoi(argv[2]);
	} else if (argc == 2) {
		port = atoi(argv[1]);
	}
	if (port <= 0) port = 5500;
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
	print_local_ips();

	start_server(bind_ip, port);

	db_close(db);
	return 0;
}
