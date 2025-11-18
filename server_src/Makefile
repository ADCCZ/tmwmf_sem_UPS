# Makefile for Pexeso Server

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -std=c99
LDFLAGS = -pthread

# Directories
BUILD_DIR = build
SRC_DIR = .

# Source files
SOURCES = main.c server.c client_handler.c client_list.c logger.c room.c game.c
OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
TARGET = server

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

# Compile source files to object files
$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TARGET) server.log
	@echo "Clean complete"

# Run the server with default parameters
run: $(TARGET)
	./$(TARGET) 127.0.0.1 10000 10 50

# Valgrind memory check
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET) 127.0.0.1 10000 10 50

# Dependencies
$(BUILD_DIR)/main.o: main.c server.h logger.h
$(BUILD_DIR)/server.o: server.c server.h client_handler.h client_list.h room.h logger.h protocol.h
$(BUILD_DIR)/client_handler.o: client_handler.c client_handler.h client_list.h room.h game.h logger.h protocol.h
$(BUILD_DIR)/client_list.o: client_list.c client_list.h client_handler.h logger.h
$(BUILD_DIR)/logger.o: logger.c logger.h
$(BUILD_DIR)/room.o: room.c room.h client_handler.h logger.h protocol.h
$(BUILD_DIR)/game.o: game.c game.h client_handler.h logger.h protocol.h

.PHONY: all clean run valgrind
