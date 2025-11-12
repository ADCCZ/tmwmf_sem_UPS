#include "room.h"
#include "logger.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static room_t **rooms = NULL;
static int max_rooms = 0;
static int next_room_id = 1;
static pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    pthread_mutex_lock(&rooms_mutex);

    if (rooms != NULL) {
        for (int i = 0; i < max_rooms; i++) {
            if (rooms[i] != NULL) {
                room_destroy(rooms[i]);
            }
        }
        free(rooms);
        rooms = NULL;
    }

    pthread_mutex_unlock(&rooms_mutex);
    logger_log(LOG_INFO, "Room system shutdown");
}

room_t* room_create(const char *name, int max_players, client_t *owner) {
    if (name == NULL || owner == NULL) {
        return NULL;
    }

    if (max_players < 2 || max_players > MAX_PLAYERS_PER_ROOM) {
        logger_log(LOG_WARNING, "Invalid max_players: %d (must be 2-4)", max_players);
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

    // Find and remove player
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] == client) {
            room->players[i] = NULL;
            room->player_count--;
            client->room = NULL;
            client->state = STATE_IN_LOBBY;

            logger_log(LOG_INFO, "Client %d (%s) left room %d",
                       client->client_id, client->nickname, room->room_id);

            // If room is empty, destroy it
            if (room->player_count == 0) {
                logger_log(LOG_INFO, "Room %d is empty, destroying", room->room_id);

                // Find room in array and set to NULL
                for (int j = 0; j < max_rooms; j++) {
                    if (rooms[j] == room) {
                        rooms[j] = NULL;
                        break;
                    }
                }

                free(room);
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

    // Remove all players
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] != NULL) {
            room->players[i]->room = NULL;
            room->players[i]->state = STATE_IN_LOBBY;
        }
    }

    logger_log(LOG_INFO, "Room %d destroyed", room->room_id);
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

            // Format: ROOM_LIST <count> <id> <name> <players> <max> <state> ...
            if (room_count == 0) {
                // Add count placeholder (we'll know total at the end)
                offset += snprintf(buffer + offset, buffer_size - offset, " %d", 0);
            }

            const char *state_str = "WAITING";
            if (r->state == ROOM_STATE_PLAYING) {
                state_str = "PLAYING";
            } else if (r->state == ROOM_STATE_FINISHED) {
                state_str = "FINISHED";
            }

            offset += snprintf(buffer + offset, buffer_size - offset,
                               " %d %s %d %d %s",
                               r->room_id, r->name, r->player_count, r->max_players, state_str);

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

void room_broadcast(room_t *room, const char *message) {
    if (room == NULL || message == NULL) {
        return;
    }

    pthread_mutex_lock(&rooms_mutex);

    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] != NULL) {
            client_send_message(room->players[i], message);
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}
