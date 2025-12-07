#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../common/utils.h"
#include "ui.h"


void run_client() {
	int sockfd;
	struct sockaddr_in serv_addr;
	char buffer[1024];

	// Create socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(5500);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect");
		close(sockfd);
		return;
	}

	printf("Connected to server.\n");

	while (1) {
		print_menu();
		printf("Choose option: ");
		fflush(stdout);
		int choice = 0;
		if (scanf("%d", &choice) != 1) {
			// clear stdin
			int c; while ((c = getchar()) != '\n' && c != EOF) ;
			continue;
		}
		// consume newline
		int c; while ((c = getchar()) != '\n' && c != EOF) ;

		if (choice == 1) { // Sign-up
			char username[128];
			char password[128];
			printf("user: ");
			if (!fgets(username, sizeof(username), stdin)) break;
			username[strcspn(username, "\r\n")] = 0;
			printf("password: ");
			if (!fgets(password, sizeof(password), stdin)) break;
			password[strcspn(password, "\r\n")] = 0;

			// prepare request: REGISTER <user> <pass>\n
			snprintf(buffer, sizeof(buffer), "REGISTER %s %s\n", username, password);
			ssize_t sent = send(sockfd, buffer, strlen(buffer), 0);
			if (sent <= 0) {
				perror("send");
				break;
			}

			// wait response
			ssize_t rec = recv(sockfd, buffer, sizeof(buffer)-1, 0);
			if (rec <= 0) {
				perror("recv");
				break;
			}
			buffer[rec] = '\0';
			printf("Server: %s\n", buffer);
		} else if (choice == 2) { // Login
			char username[128];
			char password[128];
			printf("user: ");
			if (!fgets(username, sizeof(username), stdin)) break;
			username[strcspn(username, "\r\n")] = 0;
			printf("password: ");
			if (!fgets(password, sizeof(password), stdin)) break;
			password[strcspn(password, "\r\n")] = 0;

			// prepare request: LOGIN <user> <pass>\n
			snprintf(buffer, sizeof(buffer), "LOGIN %s %s\n", username, password);
			ssize_t sent = send(sockfd, buffer, strlen(buffer), 0);
			if (sent <= 0) {
				perror("send");
				break;
			}

			// wait response
			ssize_t rec = recv(sockfd, buffer, sizeof(buffer)-1, 0);
			if (rec <= 0) {
				perror("recv");
				break;
			}
			buffer[rec] = '\0';
			printf("Server: %s\n", buffer);
		} else if (choice == 0) {
			printf("Exiting client.\n");
			break;
		} else {
			printf("Option not implemented yet.\n");
		}
	}

	close(sockfd);
}


int main() {
run_client();
return 0;
}
