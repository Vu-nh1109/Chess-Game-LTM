CC = gcc
CFLAGS = -Wall -I.
LDFLAGS = -pthread

PROTOCOL_SRC = protocol.c
PROTOCOL_OBJ = protocol.o
SERVER_SRC = server.c
CLIENT_SRC = client.c

all: server client

protocol.o: $(PROTOCOL_SRC) protocol.h
	$(CC) $(CFLAGS) -c $(PROTOCOL_SRC)

server: $(SERVER_SRC) $(PROTOCOL_OBJ)
	$(CC) $(CFLAGS) -o server $(SERVER_SRC) $(PROTOCOL_OBJ) $(LDFLAGS)

client: $(CLIENT_SRC) $(PROTOCOL_OBJ)
	$(CC) $(CFLAGS) -o client $(CLIENT_SRC) $(PROTOCOL_OBJ) $(LDFLAGS)

clean:
	rm -f server client *.o

.PHONY: all clean