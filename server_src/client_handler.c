#include "client_handler.h"
#include "client_list.h"
#include "room.h"
#include "game.h"
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
static void handle_ready(client_t *client);
static void handle_start_game(client_t *client);
static void handle_flip(client_t *client, const char *params);
static void handle_pong(client_t *client);
static void handle_reconnect(client_t *client, const char *params);

// Helper function to send error and increment error counter
static void send_error_and_count(client_t *client, const char *error_code, const char *details) {
    char error_msg[MAX_MESSAGE_LENGTH];

    if (details != NULL && strlen(details) > 0) {
        snprintf(error_msg, sizeof(error_msg), "ERROR %s %s", error_code, details);
    } else {
        snprintf(error_msg, sizeof(error_msg), "ERROR %s", error_code);
    }

    client_send_message(client, error_msg);
    client->invalid_message_count++;

    logger_log(LOG_WARNING, "Client %d (%s): Error sent - %s (count: %d/%d)",
              client->client_id,
              client->nickname[0] ? client->nickname : "unauthenticated",
              error_code,
              client->invalid_message_count,
              MAX_ERROR_COUNT);

    // Disconnect after MAX_ERROR_COUNT errors
    if (client->invalid_message_count >= MAX_ERROR_COUNT) {
        logger_log(LOG_ERROR, "Client %d: Max error count reached, closing connection",
                  client->client_id);
        close(client->socket_fd);
    }
}

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
        } else if (strcmp(command, CMD_RECONNECT) == 0) {
            handle_reconnect(client, params);
        } else if (strcmp(command, CMD_CREATE_ROOM) == 0) {
            handle_create_room(client, params);
        } else if (strcmp(command, CMD_JOIN_ROOM) == 0) {
            handle_join_room(client, params);
        } else if (strcmp(command, CMD_FLIP) == 0) {
            handle_flip(client, params);
        } else {
            send_error_and_count(client, ERR_INVALID_COMMAND, command);
        }
    } else {
        // Command without parameters
        strncpy(command, message, sizeof(command) - 1);

        if (strcmp(command, CMD_LIST_ROOMS) == 0) {
            handle_list_rooms(client);
        } else if (strcmp(command, CMD_LEAVE_ROOM) == 0) {
            handle_leave_room(client);
        } else if (strcmp(command, CMD_READY) == 0) {
            handle_ready(client);
        } else if (strcmp(command, CMD_START_GAME) == 0) {
            handle_start_game(client);
        } else if (strcmp(command, CMD_PONG) == 0) {
            handle_pong(client);
        } else {
            send_error_and_count(client, ERR_INVALID_COMMAND, command);
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

static void handle_ready(client_t *client) {
    if (client->room == NULL) {
        client_send_message(client, "ERROR NOT_IN_ROOM Not in a room");
        return;
    }

    room_t *room = client->room;

    if (room->game == NULL) {
        client_send_message(client, "ERROR GAME_NOT_STARTED Game not started");
        return;
    }

    game_t *game = (game_t *)room->game;

    if (game_player_ready(game, client) != 0) {
        client_send_message(client, "ERROR Already ready or game already started");
        return;
    }

    // Send confirmation
    client_send_message(client, "READY_OK");

    // Broadcast to other players
    char broadcast[MAX_MESSAGE_LENGTH];
    snprintf(broadcast, sizeof(broadcast), "PLAYER_READY %s", client->nickname);
    room_broadcast(room, broadcast);

    logger_log(LOG_INFO, "Client %d (%s) marked ready in room %d",
               client->client_id, client->nickname, room->room_id);

    // Check if all players ready
    if (game_all_players_ready(game)) {
        logger_log(LOG_INFO, "All players ready in room %d, starting game", room->room_id);

        // Start the game
        if (game_start(game) == 0) {
            room->state = ROOM_STATE_PLAYING;

            // Send GAME_START to all players
            char game_start_msg[MAX_MESSAGE_LENGTH];
            game_format_start_message(game, game_start_msg, sizeof(game_start_msg));
            room_broadcast(room, game_start_msg);

            // Send TURN to first player
            client_t *first_player = game_get_current_player(game);
            if (first_player != NULL) {
                client_send_message(first_player, "YOUR_TURN");
                logger_log(LOG_INFO, "Room %d: First turn goes to %s",
                           room->room_id, first_player->nickname);
            }
        }
    }
}

static void handle_start_game(client_t *client) {
    if (client->room == NULL) {
        client_send_message(client, "ERROR NOT_IN_ROOM Not in a room");
        return;
    }

    room_t *room = client->room;

    // Only room owner can start game
    if (room->owner != client) {
        client_send_message(client, "ERROR NOT_ROOM_OWNER Only room owner can start game");
        return;
    }

    // Check if game already exists
    if (room->game != NULL) {
        client_send_message(client, "ERROR Game already started");
        return;
    }

    // Need at least 2 players
    if (room->player_count < 2) {
        client_send_message(client, "ERROR NEED_MORE_PLAYERS Need at least 2 players");
        return;
    }

    // Create game with default board size (4x4)
    int board_size = 4;
    room->game = game_create(board_size, room->players, room->player_count);

    if (room->game == NULL) {
        client_send_message(client, "ERROR Failed to create game");
        logger_log(LOG_ERROR, "Failed to create game for room %d", room->room_id);
        return;
    }

    // Send confirmation to owner
    client_send_message(client, "GAME_CREATED Wait for all players to be ready");

    // Broadcast to all players that game is created and they need to be ready
    char broadcast[MAX_MESSAGE_LENGTH];
    snprintf(broadcast, sizeof(broadcast),
             "GAME_CREATED %d Send READY when you are prepared to play", board_size);
    room_broadcast(room, broadcast);

    logger_log(LOG_INFO, "Room %d: Game created by %s (board_size=%d, players=%d)",
               room->room_id, client->nickname, board_size, room->player_count);
}

static void handle_flip(client_t *client, const char *params) {
    if (client->room == NULL) {
        send_error_and_count(client, ERR_NOT_IN_ROOM, "Not in a room");
        return;
    }

    room_t *room = client->room;

    if (room->game == NULL) {
        send_error_and_count(client, ERR_GAME_NOT_STARTED, "Game not started");
        return;
    }

    game_t *game = (game_t *)room->game;

    // Validate params
    if (params == NULL || strlen(params) == 0) {
        send_error_and_count(client, ERR_INVALID_SYNTAX, "Card index required");
        return;
    }

    // Validate card_index is a number
    char *endptr;
    long card_index_long = strtol(params, &endptr, 10);

    if (*endptr != '\0' && *endptr != ' ' && *endptr != '\n') {
        send_error_and_count(client, ERR_INVALID_SYNTAX, "Card index must be a number");
        return;
    }

    int card_index = (int)card_index_long;

    // Validate card_index is within board bounds
    if (card_index < 0 || card_index >= game->total_cards) {
        char err_details[64];
        snprintf(err_details, sizeof(err_details),
                "Card index out of bounds (0-%d)", game->total_cards - 1);
        send_error_and_count(client, ERR_INVALID_MOVE, err_details);
        return;
    }

    // Attempt to flip card
    if (game_flip_card(game, client, card_index) != 0) {
        send_error_and_count(client, ERR_INVALID_CARD, "Cannot flip that card");
        return;
    }

    // Send CARD_REVEAL to all players
    char reveal_msg[MAX_MESSAGE_LENGTH];
    snprintf(reveal_msg, sizeof(reveal_msg), "CARD_REVEAL %d %d %s",
             card_index,
             game->cards[card_index].value,
             client->nickname);
    room_broadcast(room, reveal_msg);

    logger_log(LOG_INFO, "Room %d: Player %s flipped card %d (value=%d)",
               room->room_id, client->nickname, card_index,
               game->cards[card_index].value);

    // If this was the second card, check for match
    if (game->flips_this_turn == 2) {
        int is_match = game_check_match(game);

        if (is_match) {
            // MATCH!
            char match_msg[MAX_MESSAGE_LENGTH];
            snprintf(match_msg, sizeof(match_msg), "MATCH %s %d",
                     client->nickname,
                     game->player_scores[game->current_player_index]);
            room_broadcast(room, match_msg);

            // Check if game is finished
            if (game_is_finished(game)) {
                // Get winners
                client_t *winners[MAX_PLAYERS_PER_ROOM];
                int winner_count = game_get_winners(game, winners);

                // Format GAME_END message
                char game_end_msg[MAX_MESSAGE_LENGTH];
                int offset = snprintf(game_end_msg, sizeof(game_end_msg), "GAME_END");

                for (int i = 0; i < winner_count; i++) {
                    offset += snprintf(game_end_msg + offset,
                                       sizeof(game_end_msg) - offset,
                                       " %s %d",
                                       winners[i]->nickname,
                                       game->player_scores[i]);
                }

                room_broadcast(room, game_end_msg);

                logger_log(LOG_INFO, "Room %d: Game finished, %d winner(s)",
                           room->room_id, winner_count);

                // Clean up game
                game_destroy(game);
                room->game = NULL;
                room->state = ROOM_STATE_FINISHED;
            } else {
                // Same player continues, send YOUR_TURN
                client_send_message(client, "YOUR_TURN");
            }
        } else {
            // MISMATCH
            room_broadcast(room, "MISMATCH");

            // Send YOUR_TURN to next player
            client_t *next_player = game_get_current_player(game);
            if (next_player != NULL) {
                client_send_message(next_player, "YOUR_TURN");
                logger_log(LOG_INFO, "Room %d: Turn passed to %s",
                           room->room_id, next_player->nickname);
            }
        }
    }
}

static void handle_pong(client_t *client) {
    // Just update last activity
    client->last_activity = time(NULL);
    logger_log(LOG_INFO, "Client %d: PONG received", client->client_id);
}

static void handle_reconnect(client_t *new_client, const char *params) {
    if (params == NULL) {
        client_send_message(new_client, "ERROR INVALID_PARAMS Missing client ID");
        return;
    }

    int old_client_id = atoi(params);
    if (old_client_id <= 0) {
        client_send_message(new_client, "ERROR INVALID_PARAMS Invalid client ID");
        return;
    }

    // Find old client by ID
    client_t *old_client = client_list_find_by_id(old_client_id);
    if (old_client == NULL) {
        client_send_message(new_client, "ERROR Client not found or session expired");
        logger_log(LOG_WARNING, "Client %d: RECONNECT failed - client %d not found",
                  new_client->client_id, old_client_id);
        return;
    }

    // Check disconnect time
    time_t disconnect_duration = time(NULL) - old_client->last_activity;
    if (disconnect_duration > SHORT_DISCONNECT_TIMEOUT) {
        client_send_message(new_client, "ERROR Session expired (timeout > 60s)");
        logger_log(LOG_WARNING, "Client %d: RECONNECT failed - timeout too long (%ld seconds)",
                  new_client->client_id, disconnect_duration);
        return;
    }

    logger_log(LOG_INFO, "Client %d: Reconnecting as client %d (%s), disconnect duration: %ld seconds",
              new_client->client_id, old_client_id, old_client->nickname, disconnect_duration);

    // Transfer state from old to new client
    strcpy(new_client->nickname, old_client->nickname);
    new_client->state = old_client->state;
    new_client->room = old_client->room;
    new_client->client_id = old_client->client_id;  // Keep old ID
    new_client->last_activity = time(NULL);

    // Replace old client in client list
    client_list_remove(old_client);
    client_list_add(new_client);

    // Close old socket
    close(old_client->socket_fd);

    // Update room player pointer if in room
    if (new_client->room != NULL) {
        room_t *room = new_client->room;
        for (int i = 0; i < room->player_count; i++) {
            if (room->players[i] == old_client) {
                room->players[i] = new_client;
                logger_log(LOG_INFO, "Client %d: Updated room %d player pointer",
                          new_client->client_id, room->room_id);
                break;
            }
        }

        // Update game player pointer if in game
        if (room->game != NULL) {
            game_t *game = (game_t *)room->game;
            for (int i = 0; i < game->player_count; i++) {
                if (game->players[i] == old_client) {
                    game->players[i] = new_client;
                    logger_log(LOG_INFO, "Client %d: Updated game player pointer",
                              new_client->client_id);
                    break;
                }
            }
        }
    }

    // Free old client (but don't close socket or broadcast disconnect)
    free(old_client);

    // Send WELCOME with same client ID
    char welcome_msg[MAX_MESSAGE_LENGTH];
    snprintf(welcome_msg, sizeof(welcome_msg), "WELCOME %d Reconnected successfully",
             new_client->client_id);
    client_send_message(new_client, welcome_msg);

    // If in game, send GAME_STATE to restore game
    if (new_client->room != NULL && new_client->room->game != NULL) {
        game_t *game = (game_t *)new_client->room->game;
        if (game->state == GAME_STATE_PLAYING) {
            char state_msg[MAX_MESSAGE_LENGTH];
            if (game_format_state_message(game, state_msg, sizeof(state_msg)) == 0) {
                client_send_message(new_client, state_msg);
                logger_log(LOG_INFO, "Client %d: Sent GAME_STATE for reconnection",
                          new_client->client_id);
            }
        }

        // Notify other players
        char broadcast[MAX_MESSAGE_LENGTH];
        snprintf(broadcast, sizeof(broadcast), "PLAYER_RECONNECTED %s", new_client->nickname);
        room_broadcast(new_client->room, broadcast);
    }

    logger_log(LOG_INFO, "Client %d: Reconnection successful", new_client->client_id);
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

    // Remove from client list
    client_list_remove(client);

    close(client->socket_fd);
    free(client);

    return NULL;
}
