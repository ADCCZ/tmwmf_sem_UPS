CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

SOURCES = main.c server.c client_handler.c client_list.c logger.c room.c game.c

OBJDIR = build

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