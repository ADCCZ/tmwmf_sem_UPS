# Makefile for Pexeso Server

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -std=c99
LDFLAGS = -pthread

# Source files
SOURCES = main.c server.c client_handler.c logger.c room.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = server

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET) server.log
	@echo "Clean complete"

# Run the server with default parameters
run: $(TARGET)
	./$(TARGET) 127.0.0.1 10000 10 50

# Valgrind memory check
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET) 127.0.0.1 10000 10 50

# Dependencies
main.o: main.c server.h logger.h
server.o: server.c server.h client_handler.h room.h logger.h protocol.h
client_handler.o: client_handler.c client_handler.h room.h logger.h protocol.h
logger.o: logger.c logger.h
room.o: room.c room.h client_handler.h logger.h protocol.h

.PHONY: all clean run valgrind
