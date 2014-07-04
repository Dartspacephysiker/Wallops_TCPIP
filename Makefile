CC = gcc

CFLAGS = -g
SERVER = server
CLIENT = client

#SRC = $(SERVER).c $(CLIENT).c
#OBJ = $(SRC:.c=.o)

all: $(SERVER) $(CLIENT)

$(SERVER):
	$(CC) -o $@ $@.c $(CFLAGS)

$(CLIENT):
	$(CC) -o $@ $@.c $(CFLAGS)



#.c.o:
#	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean

clean:
	rm -f *.o $(SERVER) $(CLIENT)
