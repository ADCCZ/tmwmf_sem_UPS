#ifndef ROOM_H
#define ROOM_H

#include "client_handler.h"

/**
 * Room module - manages lobby and game rooms
 */

// Forward declaration for game
struct game_s;

#define MAX_PLAYERS_PER_ROOM 4
#define MAX_ROOM_NAME_LENGTH 64

typedef enum {
    ROOM_STATE_WAITING,    // Waiting for players
    ROOM_STATE_PLAYING,    // Game in progress
    ROOM_STATE_FINISHED    // Game finished
} room_state_t;

typedef struct room_s {
    int room_id;
    char name[MAX_ROOM_NAME_LENGTH];
    client_t *players[MAX_PLAYERS_PER_ROOM];
    int player_count;
    int max_players;
    int board_size;  // Board size for game (4, 6, or 8)
    room_state_t state;
    client_t *owner;  // Room creator
    struct game_s *game;  // Game instance (NULL if no game)
} room_t;

/**
 * Initialize room system
 * @param max_rooms Maximum number of rooms
 * @return 0 on success, -1 on error
 */
int room_system_init(int max_rooms);

/**
 * Shutdown room system
 */
void room_system_shutdown(void);

/**
 * Create a new room
 * @param name Room name
 * @param max_players Maximum number of players (2-4)
 * @param board_size Board size for game (4, 6, or 8)
 * @param owner Room creator
 * @return Pointer to room or NULL on error
 */
room_t* room_create(const char *name, int max_players, int board_size, client_t *owner);

/**
 * Get room by ID
 * @param room_id Room ID
 * @return Pointer to room or NULL if not found
 */
room_t* room_get_by_id(int room_id);

/**
 * Add player to room
 * @param room Room to join
 * @param client Client to add
 * @return 0 on success, -1 on error
 */
int room_add_player(room_t *room, client_t *client);

/**
 * Remove player from room
 * @param room Room to leave
 * @param client Client to remove
 * @return 0 on success, -1 on error
 */
int room_remove_player(room_t *room, client_t *client);

/**
 * Destroy room
 * @param room Room to destroy
 */
void room_destroy(room_t *room);

/**
 * Get list of all rooms
 * @param count Output parameter for room count
 * @return Array of room pointers
 */
room_t** room_get_all(int *count);

/**
 * Get room info as string for LIST_ROOMS response
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written
 */
int room_get_list_message(char *buffer, int buffer_size);

/**
 * Broadcast message to all players in room
 * @param room Room to broadcast to
 * @param message Message to send
 */
void room_broadcast(room_t *room, const char *message);

/**
 * Broadcast message to all players in room except one
 * @param room Room to broadcast to
 * @param message Message to send
 * @param exclude_client Client to exclude from broadcast (can be NULL)
 */
void room_broadcast_except(room_t *room, const char *message, client_t *exclude_client);

#endif /* ROOM_H */
