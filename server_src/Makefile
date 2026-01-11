CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

SOURCES = main.c server.c client_handler.c client_list.c logger.c room.c game.c

OBJDIR = build

# 1. $(SOURCES:.c=.o) změní přípony na .o
# 2. $(addprefix $(OBJDIR)/, ...) přidá "build/" před každý soubor
# Výsledek: build/main.o build/server.o ...
OBJECTS = $(addprefix $(OBJDIR)/, $(SOURCES:.c=.o))

TARGET = server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -rf $(OBJDIR)

.PHONY: all clean