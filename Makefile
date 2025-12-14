CC=gcc
CFLAGS=-I/usr/include/mysql -I./ -Wall -g
LIBS=-lmysqlclient -lpthread

SERVER_SRC=TCP_Server/server.c TCP_Server/connection_handler.c database/database.c common/utils.c common/file_utils.c TCP_Server/handlers/group_handlers.c TCP_Server/handlers/request_dispatcher.c database/queries.c
CLIENT_SRC=TCP_Client/client.c TCP_Client/ui.c common/utils.c common/file_utils.c

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) -o server $(SERVER_SRC) $(LIBS)
client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o client $(CLIENT_SRC) $(LIBS)

clean:
	rm -f server client

.PHONY: all clean
