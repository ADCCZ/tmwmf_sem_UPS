#include "client_handler.h"
#include "room.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

// Forward declarations of command handlers
static void handle_hello(client_t *client, const char *params);
static void handle_list_rooms(client_t *client);
static void handle_create_room(client_t *client, const char *params);
static void handle_join_room(client_t *client, const char *params);
static void handle_leave_room(client_t *client);
static void handle_pong(client_t *client);

int client_send_message(client_t *client, const char *message) {
    if (client == NULL || message == NULL) {
        return -1;
    }

    char buffer[MAX_MESSAGE_LENGTH + 2];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", message);

    if (len < 0 || len >= (int)sizeof(buffer)) {
        logger_log(LOG_ERROR, "Client %d: Message too long or formatting error", client->client_id);
        return -1;
    }

    int sent = send(client->socket_fd, buffer, len, 0);
    if (sent < 0) {
        logger_log(LOG_ERROR, "Client %d: Failed to send message", client->client_id);
        return -1;
    }

    return sent;
}

// Parse and dispatch message to appropriate handler
static void handle_message(client_t *client, const char *message) {
    if (message == NULL || strlen(message) == 0) {
        return;
    }

    // Find first space to separate command from parameters
    const char *space = strchr(message, ' ');
    char command[64] = {0};

    if (space != NULL) {
        // Command with parameters
        size_t cmd_len = space - message;
        if (cmd_len >= sizeof(command)) {
            cmd_len = sizeof(command) - 1;
        }
        strncpy(command, message, cmd_len);
        command[cmd_len] = '\0';

        const char *params = space + 1;

        // Dispatch based on command
        if (strcmp(command, CMD_HELLO) == 0) {
            handle_hello(client, params);
        } else if (strcmp(command, CMD_CREATE_ROOM) == 0) {
            handle_create_room(client, params);
        } else if (strcmp(command, CMD_JOIN_ROOM) == 0) {
            handle_join_room(client, params);
        } else {
            logger_log(LOG_WARNING, "Client %d: Unknown command: %s", client->client_id, command);
            client_send_message(client, "ERROR INVALID_COMMAND Unknown command");
            client->invalid_message_count++;
        }
    } else {
        // Command without parameters
        strncpy(command, message, sizeof(command) - 1);

        if (strcmp(command, CMD_LIST_ROOMS) == 0) {
            handle_list_rooms(client);
        } else if (strcmp(command, CMD_LEAVE_ROOM) == 0) {
            handle_leave_room(client);
        } else if (strcmp(command, CMD_PONG) == 0) {
            handle_pong(client);
        } else {
            logger_log(LOG_WARNING, "Client %d: Unknown command: %s", client->client_id, command);
            client_send_message(client, "ERROR INVALID_COMMAND Unknown command");
            client->invalid_message_count++;
        }
    }
}

// Command handlers

static void handle_hello(client_t *client, const char *params) {
    if (client->state != STATE_CONNECTED) {
        client_send_message(client, "ERROR ALREADY_AUTHENTICATED Already authenticated");
        return;
    }

    if (params == NULL || strlen(params) == 0) {
        client_send_message(client, "ERROR INVALID_PARAMS Nickname required");
        return;
    }

    // Extract nickname (first word)
    char nickname[MAX_NICK_LENGTH];
    sscanf(params, "%31s", nickname);

    // Set nickname
    strncpy(client->nickname, nickname, MAX_NICK_LENGTH - 1);
    client->nickname[MAX_NICK_LENGTH - 1] = '\0';
    client->state = STATE_IN_LOBBY;

    // Send WELCOME response
    char response[MAX_MESSAGE_LENGTH];
    snprintf(response, sizeof(response), "WELCOME %d", client->client_id);
    client_send_message(client, response);

    logger_log(LOG_INFO, "Client %d authenticated as '%s'", client->client_id, client->nickname);
}

static void handle_list_rooms(client_t *client) {
    if (client->state < STATE_IN_LOBBY) {
        client_send_message(client, "ERROR NOT_AUTHENTICATED Not authenticated");
        return;
    }

    char buffer[MAX_MESSAGE_LENGTH * 4];  // Large buffer for room list
    room_get_list_message(buffer, sizeof(buffer));
    client_send_message(client, buffer);

    logger_log(LOG_INFO, "Client %d (%s) requested room list", client->client_id, client->nickname);
}

static void handle_create_room(client_t *client, const char *params) {
    if (client->state < STATE_IN_LOBBY) {
        client_send_message(client, "ERROR NOT_AUTHENTICATED Not authenticated");
        return;
    }

    if (client->room != NULL) {
        client_send_message(client, "ERROR ALREADY_IN_ROOM Already in a room");
        return;
    }

    // Parse parameters: <name> <max_players> [board_size]
    char room_name[MAX_ROOM_NAME_LENGTH];
    int max_players = 4;  // Default

    if (params == NULL) {
        client_send_message(client, "ERROR INVALID_PARAMS Room name required");
        return;
    }

    // Parse room name and max players
    int parsed = sscanf(params, "%63s %d", room_name, &max_players);
    if (parsed < 1) {
        client_send_message(client, "ERROR INVALID_PARAMS Invalid format");
        return;
    }

    // Validate max_players
    if (max_players < 2 || max_players > MAX_PLAYERS_PER_ROOM) {
        char error[MAX_MESSAGE_LENGTH];
        snprintf(error, sizeof(error), "ERROR INVALID_PARAMS Max players must be 2-%d", MAX_PLAYERS_PER_ROOM);
        client_send_message(client, error);
        return;
    }

    // Create room
    room_t *room = room_create(room_name, max_players, client);
    if (room == NULL) {
        client_send_message(client, "ERROR ROOM_LIMIT Room limit reached");
        return;
    }

    // Send success response
    char response[MAX_MESSAGE_LENGTH];
    snprintf(response, sizeof(response), "ROOM_CREATED %d %s", room->room_id, room->name);
    client_send_message(client, response);

    logger_log(LOG_INFO, "Client %d (%s) created room %d", client->client_id, client->nickname, room->room_id);
}

static void handle_join_room(client_t *client, const char *params) {
    if (client->state < STATE_IN_LOBBY) {
        client_send_message(client, "ERROR NOT_AUTHENTICATED Not authenticated");
        return;
    }

    if (client->room != NULL) {
        client_send_message(client, "ERROR ALREADY_IN_ROOM Already in a room");
        return;
    }

    if (params == NULL) {
        client_send_message(client, "ERROR INVALID_PARAMS Room ID required");
        return;
    }

    int room_id = atoi(params);
    if (room_id <= 0) {
        client_send_message(client, "ERROR INVALID_PARAMS Invalid room ID");
        return;
    }

    // Find room
    room_t *room = room_get_by_id(room_id);
    if (room == NULL) {
        client_send_message(client, "ERROR ROOM_NOT_FOUND Room not found");
        return;
    }

    // Add player to room
    if (room_add_player(room, client) != 0) {
        client_send_message(client, "ERROR ROOM_FULL Room is full");
        return;
    }

    // Send success response
    char response[MAX_MESSAGE_LENGTH];
    snprintf(response, sizeof(response), "ROOM_JOINED %d %s", room->room_id, room->name);
    client_send_message(client, response);

    // Broadcast to other players in room
    char broadcast[MAX_MESSAGE_LENGTH];
    snprintf(broadcast, sizeof(broadcast), "PLAYER_JOINED %s", client->nickname);
    room_broadcast(room, broadcast);

    logger_log(LOG_INFO, "Client %d (%s) joined room %d", client->client_id, client->nickname, room->room_id);
}

static void handle_leave_room(client_t *client) {
    if (client->room == NULL) {
        client_send_message(client, "ERROR NOT_IN_ROOM Not in a room");
        return;
    }

    room_t *room = client->room;
    int room_id = room->room_id;

    // Remove player from room
    room_remove_player(room, client);

    // Send success response
    client_send_message(client, "LEFT_ROOM");

    // Broadcast to other players (if room still exists)
    room = room_get_by_id(room_id);
    if (room != NULL) {
        char broadcast[MAX_MESSAGE_LENGTH];
        snprintf(broadcast, sizeof(broadcast), "PLAYER_LEFT %s", client->nickname);
        room_broadcast(room, broadcast);
    }

    logger_log(LOG_INFO, "Client %d (%s) left room %d", client->client_id, client->nickname, room_id);
}

static void handle_pong(client_t *client) {
    // Just update last activity
    client->last_activity = time(NULL);
    logger_log(LOG_INFO, "Client %d: PONG received", client->client_id);
}

void* client_handler_thread(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[MAX_MESSAGE_LENGTH];
    char line_buffer[MAX_MESSAGE_LENGTH];
    int line_pos = 0;

    // Initialize client
    client->room = NULL;

    logger_log(LOG_INFO, "Client %d: Handler thread started (fd=%d)", client->client_id, client->socket_fd);

    while (1) {
        int bytes_received = recv(client->socket_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received < 0) {
            logger_log(LOG_ERROR, "Client %d: recv() failed", client->client_id);
            break;
        }

        if (bytes_received == 0) {
            logger_log(LOG_INFO, "Client %d: Connection closed by client", client->client_id);
            break;
        }

        buffer[bytes_received] = '\0';

        // Process received data character by character to handle \n delimited messages
        for (int i = 0; i < bytes_received; i++) {
            char c = buffer[i];

            if (c == '\n') {
                // End of message
                line_buffer[line_pos] = '\0';

                if (line_pos > 0) {
                    // Log the received message
                    logger_log(LOG_INFO, "Client %d: Received message: '%s'", client->client_id, line_buffer);

                    // Update last activity
                    client->last_activity = time(NULL);

                    // Handle the message
                    handle_message(client, line_buffer);
                }

                // Reset line buffer
                line_pos = 0;
            } else if (c == '\r') {
                // Ignore CR (in case client sends \r\n)
                continue;
            } else {
                // Add character to line buffer
                if (line_pos < MAX_MESSAGE_LENGTH - 1) {
                    line_buffer[line_pos++] = c;
                } else {
                    // Line too long - treat as invalid
                    logger_log(LOG_WARNING, "Client %d: Message too long, truncating", client->client_id);
                    client->invalid_message_count++;
                    line_pos = 0;
                }
            }
        }
    }

    // Cleanup - remove from room if in one
    if (client->room != NULL) {
        logger_log(LOG_INFO, "Client %d: Removing from room %d", client->client_id, client->room->room_id);

        room_t *room = client->room;
        int room_id = room->room_id;

        room_remove_player(room, client);

        // Notify other players
        room = room_get_by_id(room_id);
        if (room != NULL) {
            char broadcast[MAX_MESSAGE_LENGTH];
            snprintf(broadcast, sizeof(broadcast), "PLAYER_DISCONNECTED %s LONG", client->nickname);
            room_broadcast(room, broadcast);
        }
    }

    logger_log(LOG_INFO, "Client %d: Closing connection", client->client_id);
    close(client->socket_fd);
    free(client);

    return NULL;
}
