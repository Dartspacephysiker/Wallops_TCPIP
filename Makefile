CC = gcc

#CFLAGS = -fPIC -Wall -std=gnu99 -Iutils/ -g
#LDFLAGS = -pipe -Wall -lm -pthread -g
CFLAGS = -fPIC -Wall -std=gnu99 -Iutils/ -O2
LDFLAGS = -pipe -Wall -lm -pthread -O2

#Reguliere, old-fashioned tcp_player
EXEC = tcp_player
SRC = simple_fifo.c tcp_player.c tcp_player_helpers.c utils/tcp_utils.c
OBJ = $(SRC:.c=.o)
#HEADERS = simple_fifo.h defaults.h tcp_player_errors.h tcp_player_helpers.h tcp_player_struct.h tcp_utils.h $(EXEC).h
#HEADERS = simple_fifo.h defaults.h tcp_player_helpers.h tcp_player_struct.h tcp_utils.h $(EXEC).h $(EXEC_HS).h
HEADERS = simple_fifo.h defaults.h tcp_player_helpers.h tcp_player_struct.h tcp_utils.h

#High-speed version
EXEC_HS = tcp_player_highspeed
SRC_HS = simple_fifo.c tcp_player_highspeed.c tcp_player_helpers.c utils/tcp_utils.c
OBJ_HS = $(SRC_HS:.c=.o)
#HEADERS_HS = simple_fifo.h defaults.h tcp_player_helpers.h tcp_player_struct.h tcp_utils.h

all: $(SRC) $(EXEC) $(EXEC_HS)

$(EXEC): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

$(EXEC_HS): $(OBJ_HS)
	$(CC) -o $@ $(OBJ_HS) $(LDFLAGS)


.c.o: $(HEADERS)
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean

clean:
	rm -f *.o utils/*.o $(EXEC) $(EXEC_HS)
