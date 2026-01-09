CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
SOURCES = main.c server.c client_handler.c client_list.c logger.c room.c game.c
TARGET = server

all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
