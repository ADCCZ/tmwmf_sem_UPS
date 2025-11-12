#ifndef GAME_H
#define GAME_H

#include "client_handler.h"

/**
 * Game module - Pexeso (Memory) game logic
 */

#define MIN_BOARD_SIZE 4    // 4x4 = 16 cards (8 pairs)
#define MAX_BOARD_SIZE 6    // 6x6 = 36 cards (18 pairs)
#define MAX_PLAYERS_PER_ROOM 4  // Maximum players per game

typedef enum {
    CARD_HIDDEN,     // Card is face down
    CARD_REVEALED,   // Card is face up
    CARD_MATCHED     // Card has been matched
} card_state_t;

typedef struct {
    int value;           // Card value (1-18, pairs have same value)
    card_state_t state;  // Current state
} card_t;

typedef enum {
    GAME_STATE_WAITING,   // Waiting for players to be ready
    GAME_STATE_PLAYING,   // Game in progress
    GAME_STATE_FINISHED   // Game ended
} game_state_t;

typedef struct game_s {
    int board_size;              // Board size (4, 5, or 6)
    int total_cards;             // board_size * board_size
    int total_pairs;             // total_cards / 2
    card_t *cards;               // Array of cards

    client_t *players[MAX_PLAYERS_PER_ROOM];  // Players in game
    int player_count;
    int player_scores[MAX_PLAYERS_PER_ROOM];  // Score for each player
    int player_ready[MAX_PLAYERS_PER_ROOM];   // Ready status

    int current_player_index;    // Index of player whose turn it is
    int first_card_index;        // First flipped card (-1 if none)
    int second_card_index;       // Second flipped card (-1 if none)
    int flips_this_turn;         // Number of cards flipped this turn (0, 1, or 2)

    int matched_pairs;           // Number of pairs matched
    game_state_t state;
} game_t;

/**
 * Create a new game
 * @param board_size Board size (4, 5, or 6)
 * @param players Array of players
 * @param player_count Number of players
 * @return Pointer to game or NULL on error
 */
game_t* game_create(int board_size, client_t **players, int player_count);

/**
 * Destroy a game
 * @param game Game to destroy
 */
void game_destroy(game_t *game);

/**
 * Mark a player as ready
 * @param game Game
 * @param client Player
 * @return 0 on success, -1 on error
 */
int game_player_ready(game_t *game, client_t *client);

/**
 * Check if all players are ready
 * @param game Game
 * @return 1 if all ready, 0 otherwise
 */
int game_all_players_ready(game_t *game);

/**
 * Start the game (after all players ready)
 * @param game Game
 * @return 0 on success, -1 on error
 */
int game_start(game_t *game);

/**
 * Handle a card flip by a player
 * @param game Game
 * @param client Player attempting flip
 * @param card_index Index of card to flip
 * @return 0 on success, -1 on error
 */
int game_flip_card(game_t *game, client_t *client, int card_index);

/**
 * Check if the two flipped cards match
 * @param game Game
 * @return 1 if match, 0 if mismatch, -1 on error
 */
int game_check_match(game_t *game);

/**
 * Get the player whose turn it is
 * @param game Game
 * @return Pointer to player or NULL
 */
client_t* game_get_current_player(game_t *game);

/**
 * Check if game is finished
 * @param game Game
 * @return 1 if finished, 0 otherwise
 */
int game_is_finished(game_t *game);

/**
 * Get winner(s) of the game
 * @param game Game
 * @param winners Array to store winners (must be at least MAX_PLAYERS_PER_ROOM)
 * @return Number of winners (can be >1 in case of tie)
 */
int game_get_winners(game_t *game, client_t **winners);

/**
 * Format game state for GAME_START message
 * @param game Game
 * @param buffer Buffer to write to
 * @param buffer_size Size of buffer
 * @return 0 on success, -1 on error
 */
int game_format_start_message(game_t *game, char *buffer, int buffer_size);

/**
 * Format game state for GAME_STATE message (reconnection)
 * @param game Game
 * @param buffer Buffer to write to
 * @param buffer_size Size of buffer
 * @return 0 on success, -1 on error
 */
int game_format_state_message(game_t *game, char *buffer, int buffer_size);

#endif // GAME_H
