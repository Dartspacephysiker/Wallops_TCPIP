CC = gcc

CFLAGS = --std=gnu99 -Wall -g #-O2

SERVER = server
CLIENT = client

FILEPARSE = tcp_fileparse

TCP_TSTAMP = tcp_tstamp_to_ascii

#SRC = $(SERVER).c $(CLIENT).c 
FILEPARSE_SRC = tcp_fileparse.c tcp_utils.c
FILEPARSE_OBJ = $(FILEPARSE_SRC:.c=.o)

TCP_TSTAMP_SRC = tcp_tstamp_to_ascii.c
TCP_TSTAMP_OBJ = $(TCP_TSTAMP_SRC:.c=.o)

HEADERS = tcp_fileparse.h tcp_utils.h

all: $(SERVER) $(CLIENT) $(FILEPARSE) $(TCP_TSTAMP)

$(SERVER):
	$(CC) -o $@ $@.c $(CFLAGS)

$(CLIENT):
	$(CC) -o $@ $@.c $(CFLAGS)

$(FILEPARSE): $(FILEPARSE_OBJ)
	$(CC) -o $@ $(FILEPARSE_OBJ) $(CFLAGS)

.c.o: $(HEADERS)
	$(CC) -o $@ $(CFLAGS) -c $<

$(TCP_TSTAMP): $(TCP_TSTAMP_OBJ)
	$(CC) -o $@ $(TCP_TSTAMP_OBJ) $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o $(SERVER) $(CLIENT) $(FILEPARSE)
