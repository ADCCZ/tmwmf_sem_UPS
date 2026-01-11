#include "room.h"
#include "logger.h"
#include "protocol.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static room_t **rooms = NULL;
static int max_rooms = 0;
static int next_room_id = 1;
static pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declaration for internal broadcast function
static void room_broadcast_except_locked(room_t *room, const char *message, client_t *exclude_client);

int room_system_init(int max_rooms_count) {
    pthread_mutex_lock(&rooms_mutex);

    max_rooms = max_rooms_count;
    rooms = (room_t **)calloc(max_rooms, sizeof(room_t *));

    if (rooms == NULL) {
        logger_log(LOG_ERROR, "Failed to allocate memory for rooms");
        pthread_mutex_unlock(&rooms_mutex);
        return -1;
    }

    logger_log(LOG_INFO, "Room system initialized (max_rooms=%d)", max_rooms);
    pthread_mutex_unlock(&rooms_mutex);
    return 0;
}

void room_system_shutdown(void) {
    // No mutex needed during shutdown - all other threads are already terminated
    // (PING and timeout_checker joined, handler threads had 3s to finish)
    logger_log(LOG_INFO, "Room system shutdown starting");

    if (rooms != NULL) {
        for (int i = 0; i < max_rooms; i++) {
            if (rooms[i] != NULL) {
                room_t *room = rooms[i];

                // CRITICAL: Do NOT access room->players[] - clients may be already freed!
                // Client pointers in room->players[] may be dangling pointers after client_list operations.

                // Free game if exists (prevent memory leak)
                if (room->game != NULL) {
                    game_t *game = (game_t *)room->game;
                    logger_log(LOG_INFO, "Room %d: Freeing game during shutdown", room->room_id);
                    game_destroy(game);
                    room->game = NULL;
                }

                logger_log(LOG_INFO, "Room %d destroyed during shutdown", room->room_id);
                free(room);
                rooms[i] = NULL;
            }
        }
        free(rooms);
        rooms = NULL;
    }

    logger_log(LOG_INFO, "Room system shutdown complete");
}

room_t* room_create(const char *name, int max_players, int board_size, client_t *owner) {
    if (name == NULL || owner == NULL) {
        return NULL;
    }

    if (max_players < 2 || max_players > MAX_PLAYERS_PER_ROOM) {
        logger_log(LOG_WARNING, "Invalid max_players: %d (must be 2-4)", max_players);
        return NULL;
    }

    if (board_size < 4 || board_size > 8 || board_size % 2 != 0) {
        logger_log(LOG_WARNING, "Invalid board_size: %d (must be 4, 6, or 8)", board_size);
        return NULL;
    }

    pthread_mutex_lock(&rooms_mutex);

    // Find free slot
    int free_slot = -1;
    for (int i = 0; i < max_rooms; i++) {
        if (rooms[i] == NULL) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        logger_log(LOG_WARNING, "No free room slots available");
        pthread_mutex_unlock(&rooms_mutex);
        return NULL;
    }

    // Allocate room
    room_t *room = (room_t *)calloc(1, sizeof(room_t));
    if (room == NULL) {
        logger_log(LOG_ERROR, "Failed to allocate memory for room");
        pthread_mutex_unlock(&rooms_mutex);
        return NULL;
    }

    // Initialize room
    room->room_id = next_room_id++;
    strncpy(room->name, name, MAX_ROOM_NAME_LENGTH - 1);
    room->name[MAX_ROOM_NAME_LENGTH - 1] = '\0';
    room->max_players = max_players;
    room->board_size = board_size;
    room->player_count = 0;
    room->state = ROOM_STATE_WAITING;
    room->owner = owner;
    room->game = NULL;  // No game initially

    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        room->players[i] = NULL;
    }

    // Add creator to room
    room->players[0] = owner;
    room->player_count = 1;
    owner->room = room;
    owner->state = STATE_IN_ROOM;

    rooms[free_slot] = room;

    logger_log(LOG_INFO, "Room created: id=%d, name='%s', max_players=%d, owner=%s",
               room->room_id, room->name, room->max_players, owner->nickname);

    pthread_mutex_unlock(&rooms_mutex);
    return room;
}

room_t* room_get_by_id(int room_id) {
    pthread_mutex_lock(&rooms_mutex);

    for (int i = 0; i < max_rooms; i++) {
        if (rooms[i] != NULL && rooms[i]->room_id == room_id) {
            pthread_mutex_unlock(&rooms_mutex);
            return rooms[i];
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
    return NULL;
}

int room_add_player(room_t *room, client_t *client) {
    if (room == NULL || client == NULL) {
        return -1;
    }

    pthread_mutex_lock(&rooms_mutex);

    // Check if room is full
    if (room->player_count >= room->max_players) {
        logger_log(LOG_WARNING, "Room %d is full", room->room_id);
        pthread_mutex_unlock(&rooms_mutex);
        return -1;
    }

    // Check if player already in room
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] == client) {
            logger_log(LOG_WARNING, "Client %d already in room %d", client->client_id, room->room_id);
            pthread_mutex_unlock(&rooms_mutex);
            return -1;
        }
    }

    // Find free slot
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] == NULL) {
            room->players[i] = client;
            room->player_count++;
            client->room = room;
            client->state = STATE_IN_ROOM;

            logger_log(LOG_INFO, "Client %d (%s) joined room %d",
                       client->client_id, client->nickname, room->room_id);

            pthread_mutex_unlock(&rooms_mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
    return -1;
}

int room_remove_player(room_t *room, client_t *client) {
    if (room == NULL || client == NULL) {
        return -1;
    }

    pthread_mutex_lock(&rooms_mutex);

    // Check if this client is the owner
    int was_owner = (room->owner == client);

    // Find and remove player
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] == client) {
            room->players[i] = NULL;
            room->player_count--;
            client->room = NULL;
            client->state = STATE_IN_LOBBY;

            logger_log(LOG_INFO, "Client %d (%s) left room %d%s (player_count now: %d)",
                       client->client_id, client->nickname, room->room_id,
                       was_owner ? " (was owner)" : "", room->player_count);

            // Check if we need to cancel the game (not enough players left)
            if (room->game != NULL && room->player_count < 2) {
                logger_log(LOG_INFO, "Room %d: Not enough players (%d) to continue game, forfeit win for highest scorer",
                          room->room_id, room->player_count);

                game_t *game = (game_t *)room->game;

                // Give forfeit win to player(s) with highest score
                int remaining_pairs = game->total_pairs - game->matched_pairs;

                // Find highest score among remaining players (excluding who left)
                int highest_score = -1;
                int winner_count = 0;

                for (int k = 0; k < game->player_count; k++) {
                    if (game->players[k] != NULL && game->players[k] != client) {
                        if (game->player_scores[k] > highest_score) {
                            highest_score = game->player_scores[k];
                            winner_count = 1;
                        } else if (game->player_scores[k] == highest_score) {
                            winner_count++;
                        }
                    }
                }

                // Distribute remaining pairs to player(s) with highest score
                if (winner_count > 0 && remaining_pairs > 0) {
                    int bonus_per_winner = remaining_pairs / winner_count;
                    int extra_pairs = remaining_pairs % winner_count;

                    for (int k = 0; k < game->player_count; k++) {
                        if (game->players[k] != NULL && game->players[k] != client) {
                            if (game->player_scores[k] == highest_score) {
                                game->player_scores[k] += bonus_per_winner;
                                if (extra_pairs > 0) {
                                    game->player_scores[k]++;
                                    extra_pairs--;
                                }
                                logger_log(LOG_INFO, "Room %d: Player %s (highest score) gets forfeit bonus (new score: %d)",
                                          room->room_id, game->players[k]->nickname, game->player_scores[k]);
                            }
                        }
                    }
                }

                // Build GAME_END_FORFEIT message with final scores (auto-return to lobby, no dialog)
                char game_cancel_msg[MAX_MESSAGE_LENGTH];
                int offset = snprintf(game_cancel_msg, sizeof(game_cancel_msg), "GAME_END_FORFEIT");

                for (int k = 0; k < game->player_count; k++) {
                    if (game->players[k] != NULL) {
                        offset += snprintf(game_cancel_msg + offset, sizeof(game_cancel_msg) - offset,
                                          " %s %d", game->players[k]->nickname, game->player_scores[k]);
                    }
                }

                // Notify remaining players (use _locked version - mutex already held)
                room_broadcast_except_locked(room, game_cancel_msg, NULL);

                logger_log(LOG_INFO, "Room %d: Game ended by forfeit - sent final scores to remaining players", room->room_id);

                // Clean up game
                game_destroy(game);
                room->game = NULL;
                room->state = ROOM_STATE_WAITING;

                // Store room_id for logging
                int forfeit_room_id = room->room_id;

                // Collect remaining players - they will be removed from room (go to lobby)
                // Since GAME_END_FORFEIT was sent, clients expect to return to lobby automatically
                client_t *remaining_players[MAX_PLAYERS_PER_ROOM];
                int remaining_count = 0;

                for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                    if (room->players[j] != NULL) {
                        remaining_players[remaining_count++] = room->players[j];
                        // Set state to lobby BEFORE unlocking mutex
                        room->players[j]->state = STATE_IN_LOBBY;
                        room->players[j]->room = NULL;
                        room->players[j] = NULL;  // Clear from room array
                        logger_log(LOG_INFO, "Room %d: Player %s removed from room (forfeit, going to lobby)",
                                  forfeit_room_id, remaining_players[remaining_count - 1]->nickname);
                    }
                }

                // Room is now empty
                room->player_count = 0;

                // Unlock mutex and destroy room
                pthread_mutex_unlock(&rooms_mutex);
                room_destroy(room);

                logger_log(LOG_INFO, "Room %d destroyed after forfeit", forfeit_room_id);
                return 0;
            }

            // If owner left and room still has players, transfer ownership or destroy room
            if (was_owner && room->player_count > 0) {
                // Find first remaining player and make them owner
                for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                    if (room->players[j] != NULL && !room->players[j]->is_disconnected) {
                        room->owner = room->players[j];
                        logger_log(LOG_INFO, "Room %d: Ownership transferred to %s",
                                  room->room_id, room->owner->nickname);

                        // Notify all players about ownership change (use _locked version - mutex already held)
                        char ownership_msg[MAX_MESSAGE_LENGTH];
                        snprintf(ownership_msg, sizeof(ownership_msg),
                                "ROOM_OWNER_CHANGED %s", room->owner->nickname);
                        room_broadcast_except_locked(room, ownership_msg, NULL);
                        break;
                    }
                }

                // If no connected player found (all disconnected), destroy room
                if (room->owner == client) {
                    logger_log(LOG_INFO, "Room %d: Owner left and no connected players remain, destroying room",
                              room->room_id);

                    // Store room_id before destroy
                    int room_id = room->room_id;

                    // Notify remaining (disconnected) players (use _locked version - mutex already held)
                    char room_closed_msg[MAX_MESSAGE_LENGTH];
                    snprintf(room_closed_msg, sizeof(room_closed_msg),
                            "ROOM_CLOSED Owner left");
                    room_broadcast_except_locked(room, room_closed_msg, NULL);

                    // Remove all players from room
                    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                        if (room->players[j] != NULL) {
                            room->players[j]->room = NULL;
                            room->players[j]->state = STATE_IN_LOBBY;
                        }
                    }

                    pthread_mutex_unlock(&rooms_mutex);
                    room_destroy(room);
                    logger_log(LOG_INFO, "Room %d destroyed after owner left", room_id);
                    return 0;
                }
            }

            // If room is empty, destroy it
            if (room->player_count == 0) {
                int room_id = room->room_id;

                // Unlock mutex before calling room_destroy (it will lock again)
                pthread_mutex_unlock(&rooms_mutex);
                room_destroy(room);

                logger_log(LOG_INFO, "Room %d destroyed (empty)", room_id);
                return 0;
            }

            pthread_mutex_unlock(&rooms_mutex);
            return 0;
        }
    }

    logger_log(LOG_WARNING, "Client %d not found in room %d", client->client_id, room->room_id);
    pthread_mutex_unlock(&rooms_mutex);
    return -1;
}

void room_destroy(room_t *room) {
    if (room == NULL) {
        return;
    }

    pthread_mutex_lock(&rooms_mutex);

    // Remove all players
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] != NULL) {
            room->players[i]->room = NULL;
            room->players[i]->state = STATE_IN_LOBBY;
        }
    }

    // Remove room from global rooms array
    for (int i = 0; i < max_rooms; i++) {
        if (rooms[i] == room) {
            rooms[i] = NULL;
            break;
        }
    }

    logger_log(LOG_INFO, "Room %d destroyed", room->room_id);

    pthread_mutex_unlock(&rooms_mutex);

    free(room);
}

room_t** room_get_all(int *count) {
    pthread_mutex_lock(&rooms_mutex);

    *count = 0;
    for (int i = 0; i < max_rooms; i++) {
        if (rooms[i] != NULL) {
            (*count)++;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
    return rooms;
}

int room_get_list_message(char *buffer, int buffer_size) {
    pthread_mutex_lock(&rooms_mutex);

    int offset = snprintf(buffer, buffer_size, "ROOM_LIST");

    int room_count = 0;
    for (int i = 0; i < max_rooms && offset < buffer_size - 1; i++) {
        if (rooms[i] != NULL) {
            room_t *r = rooms[i];

            // Skip FINISHED rooms - they should not appear in lobby
            if (r->state == ROOM_STATE_FINISHED) {
                continue;
            }

            // Format: ROOM_LIST <count> <id> <name> <players> <max> <state> <board_size> ...
            if (room_count == 0) {
                // Add count placeholder (we'll know total at the end)
                offset += snprintf(buffer + offset, buffer_size - offset, " %d", 0);
            }

            const char *state_str = "WAITING";
            if (r->state == ROOM_STATE_PLAYING) {
                state_str = "PLAYING";
            }

            offset += snprintf(buffer + offset, buffer_size - offset,
                               " %d %s %d %d %s %d",
                               r->room_id, r->name, r->player_count, r->max_players, state_str, r->board_size);

            room_count++;
        }
    }

    // Update count in message
    if (room_count > 0) {
        char temp[16];
        snprintf(temp, sizeof(temp), " %d", room_count);
        // Insert count after "ROOM_LIST"
        memmove(buffer + 9 + strlen(temp), buffer + 11, offset - 11);
        memcpy(buffer + 9, temp, strlen(temp));
        offset += strlen(temp) - 2;
    } else {
        offset += snprintf(buffer + offset, buffer_size - offset, " 0");
    }

    pthread_mutex_unlock(&rooms_mutex);
    return offset;
}

// Internal function: broadcast without locking (assumes mutex is already held)
static void room_broadcast_except_locked(room_t *room, const char *message, client_t *exclude_client) {
    if (room == NULL || message == NULL) {
        return;
    }

    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] != NULL && room->players[i] != exclude_client) {
            client_send_message(room->players[i], message);
        }
    }
}

void room_broadcast(room_t *room, const char *message) {
    room_broadcast_except(room, message, NULL);
}

void room_broadcast_except(room_t *room, const char *message, client_t *exclude_client) {
    if (room == NULL || message == NULL) {
        return;
    }

    pthread_mutex_lock(&rooms_mutex);
    room_broadcast_except_locked(room, message, exclude_client);
    pthread_mutex_unlock(&rooms_mutex);
}
