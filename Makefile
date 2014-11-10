CC = gcc

CFLAGS = -fPIC -Wall -std=gnu99 -Iutils/ -O2
LDFLAGS = -pipe -Wall -lm -pthread -O2

EXEC = tcp_player

SRC = simple_fifo.c tcp_player.c tcp_player_helpers.c utils/tcp_utils.c
OBJ = $(SRC:.c=.o)

HEADERS = simple_fifo.h defaults.h tcp_player_errors.h tcp_player_helpers.h tcp_player_struct.h tcp_utils.h $(EXEC).h 

all: $(SRC) $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

.c.o: $(HEADERS)
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean

clean:
	rm -f *.o utils/*.o $(EXEC) 
