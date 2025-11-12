#include "game.h"
#include "logger.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Shuffle array using Fisher-Yates algorithm
static void shuffle_array(int *array, int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

game_t* game_create(int board_size, client_t **players, int player_count) {
    if (board_size < MIN_BOARD_SIZE || board_size > MAX_BOARD_SIZE) {
        logger_log(LOG_ERROR, "Invalid board size: %d (must be %d-%d)",
                   board_size, MIN_BOARD_SIZE, MAX_BOARD_SIZE);
        return NULL;
    }

    if (player_count < 2 || player_count > MAX_PLAYERS_PER_ROOM) {
        logger_log(LOG_ERROR, "Invalid player count: %d", player_count);
        return NULL;
    }

    game_t *game = (game_t *)calloc(1, sizeof(game_t));
    if (game == NULL) {
        logger_log(LOG_ERROR, "Failed to allocate memory for game");
        return NULL;
    }

    // Initialize basic properties
    game->board_size = board_size;
    game->total_cards = board_size * board_size;
    game->total_pairs = game->total_cards / 2;
    game->player_count = player_count;
    game->current_player_index = 0;
    game->first_card_index = -1;
    game->second_card_index = -1;
    game->flips_this_turn = 0;
    game->matched_pairs = 0;
    game->state = GAME_STATE_WAITING;

    // Copy players
    for (int i = 0; i < player_count; i++) {
        game->players[i] = players[i];
        game->player_scores[i] = 0;
        game->player_ready[i] = 0;
    }

    // Allocate cards
    game->cards = (card_t *)calloc(game->total_cards, sizeof(card_t));
    if (game->cards == NULL) {
        logger_log(LOG_ERROR, "Failed to allocate memory for cards");
        free(game);
        return NULL;
    }

    // Create array of card values (pairs)
    int *values = (int *)malloc(game->total_cards * sizeof(int));
    if (values == NULL) {
        logger_log(LOG_ERROR, "Failed to allocate memory for card values");
        free(game->cards);
        free(game);
        return NULL;
    }

    // Fill with pairs (1,1,2,2,3,3,...)
    for (int i = 0; i < game->total_pairs; i++) {
        values[i * 2] = i + 1;
        values[i * 2 + 1] = i + 1;
    }

    // Shuffle
    srand(time(NULL));
    shuffle_array(values, game->total_cards);

    // Assign shuffled values to cards
    for (int i = 0; i < game->total_cards; i++) {
        game->cards[i].value = values[i];
        game->cards[i].state = CARD_HIDDEN;
    }

    free(values);

    logger_log(LOG_INFO, "Game created: board_size=%d, total_cards=%d, pairs=%d, players=%d",
               board_size, game->total_cards, game->total_pairs, player_count);

    return game;
}

void game_destroy(game_t *game) {
    if (game == NULL) {
        return;
    }

    if (game->cards != NULL) {
        free(game->cards);
    }

    free(game);
    logger_log(LOG_INFO, "Game destroyed");
}

int game_player_ready(game_t *game, client_t *client) {
    if (game == NULL || client == NULL) {
        return -1;
    }

    if (game->state != GAME_STATE_WAITING) {
        logger_log(LOG_WARNING, "Cannot mark ready, game already started");
        return -1;
    }

    // Find player in game
    for (int i = 0; i < game->player_count; i++) {
        if (game->players[i] == client) {
            game->player_ready[i] = 1;
            logger_log(LOG_INFO, "Player %s marked as ready (%d/%d)",
                       client->nickname, i + 1, game->player_count);
            return 0;
        }
    }

    logger_log(LOG_WARNING, "Player %s not found in game", client->nickname);
    return -1;
}

int game_all_players_ready(game_t *game) {
    if (game == NULL) {
        return 0;
    }

    for (int i = 0; i < game->player_count; i++) {
        if (game->player_ready[i] == 0) {
            return 0;
        }
    }

    return 1;
}

int game_start(game_t *game) {
    if (game == NULL) {
        return -1;
    }

    if (game->state != GAME_STATE_WAITING) {
        logger_log(LOG_WARNING, "Game already started");
        return -1;
    }

    game->state = GAME_STATE_PLAYING;
    game->current_player_index = 0;

    logger_log(LOG_INFO, "Game started, first player: %s",
               game->players[0]->nickname);

    return 0;
}

int game_flip_card(game_t *game, client_t *client, int card_index) {
    if (game == NULL || client == NULL) {
        return -1;
    }

    // Check if game is playing
    if (game->state != GAME_STATE_PLAYING) {
        logger_log(LOG_WARNING, "Cannot flip card, game not playing");
        return -1;
    }

    // Check if it's player's turn
    if (game->players[game->current_player_index] != client) {
        logger_log(LOG_WARNING, "Player %s tried to flip card but it's not their turn",
                   client->nickname);
        return -1;
    }

    // Check if already flipped 2 cards this turn
    if (game->flips_this_turn >= 2) {
        logger_log(LOG_WARNING, "Player %s tried to flip card but already flipped 2",
                   client->nickname);
        return -1;
    }

    // Validate card index
    if (card_index < 0 || card_index >= game->total_cards) {
        logger_log(LOG_WARNING, "Invalid card index: %d (must be 0-%d)",
                   card_index, game->total_cards - 1);
        return -1;
    }

    // Check if card is already revealed or matched
    if (game->cards[card_index].state != CARD_HIDDEN) {
        logger_log(LOG_WARNING, "Card %d is already revealed/matched", card_index);
        return -1;
    }

    // Reveal the card
    game->cards[card_index].state = CARD_REVEALED;
    game->flips_this_turn++;

    logger_log(LOG_INFO, "Player %s flipped card %d (value=%d), flip %d/2",
               client->nickname, card_index, game->cards[card_index].value,
               game->flips_this_turn);

    // Store card indices
    if (game->flips_this_turn == 1) {
        game->first_card_index = card_index;
    } else {
        game->second_card_index = card_index;
    }

    return 0;
}

int game_check_match(game_t *game) {
    if (game == NULL || game->flips_this_turn != 2) {
        return 0;
    }

    int first_value = game->cards[game->first_card_index].value;
    int second_value = game->cards[game->second_card_index].value;

    if (first_value == second_value) {
        // Match!
        game->cards[game->first_card_index].state = CARD_MATCHED;
        game->cards[game->second_card_index].state = CARD_MATCHED;
        game->player_scores[game->current_player_index]++;
        game->matched_pairs++;

        logger_log(LOG_INFO, "MATCH! Player %s matched cards %d and %d (value=%d), score=%d",
                   game->players[game->current_player_index]->nickname,
                   game->first_card_index, game->second_card_index, first_value,
                   game->player_scores[game->current_player_index]);

        // Reset for next turn (same player continues)
        game->first_card_index = -1;
        game->second_card_index = -1;
        game->flips_this_turn = 0;

        // Check if game is finished
        if (game->matched_pairs == game->total_pairs) {
            game->state = GAME_STATE_FINISHED;
            logger_log(LOG_INFO, "Game finished! All pairs matched");
        }

        return 1; // Match
    } else {
        // No match
        game->cards[game->first_card_index].state = CARD_HIDDEN;
        game->cards[game->second_card_index].state = CARD_HIDDEN;

        logger_log(LOG_INFO, "MISMATCH! Player %s cards %d (val=%d) and %d (val=%d)",
                   game->players[game->current_player_index]->nickname,
                   game->first_card_index, first_value,
                   game->second_card_index, second_value);

        // Move to next player
        game->current_player_index = (game->current_player_index + 1) % game->player_count;

        logger_log(LOG_INFO, "Turn passed to player %s",
                   game->players[game->current_player_index]->nickname);

        // Reset for next turn
        game->first_card_index = -1;
        game->second_card_index = -1;
        game->flips_this_turn = 0;

        return 0; // Mismatch
    }
}

client_t* game_get_current_player(game_t *game) {
    if (game == NULL || game->state != GAME_STATE_PLAYING) {
        return NULL;
    }

    return game->players[game->current_player_index];
}

int game_is_finished(game_t *game) {
    if (game == NULL) {
        return 0;
    }

    return (game->state == GAME_STATE_FINISHED);
}

int game_get_winners(game_t *game, client_t **winners) {
    if (game == NULL || winners == NULL || game->state != GAME_STATE_FINISHED) {
        return 0;
    }

    // Find maximum score
    int max_score = 0;
    for (int i = 0; i < game->player_count; i++) {
        if (game->player_scores[i] > max_score) {
            max_score = game->player_scores[i];
        }
    }

    // Collect all players with max score (handles ties)
    int winner_count = 0;
    for (int i = 0; i < game->player_count; i++) {
        if (game->player_scores[i] == max_score) {
            winners[winner_count++] = game->players[i];
        }
    }

    logger_log(LOG_INFO, "Game winners: %d player(s) with score %d",
               winner_count, max_score);

    return winner_count;
}

int game_format_start_message(game_t *game, char *buffer, int buffer_size) {
    if (game == NULL || buffer == NULL || buffer_size < 1) {
        return -1;
    }

    // Format: GAME_START <board_size> <player1> <player2> ...
    int offset = snprintf(buffer, buffer_size, "GAME_START %d", game->board_size);

    for (int i = 0; i < game->player_count; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, " %s",
                           game->players[i]->nickname);
    }

    return 0;
}

int game_format_state_message(game_t *game, char *buffer, int buffer_size) {
    if (game == NULL || buffer == NULL || buffer_size < 1) {
        return -1;
    }

    // Format: GAME_STATE <board_size> <current_player_nick> <player1> <score1> <player2> <score2> ... <card_states>
    int offset = snprintf(buffer, buffer_size, "GAME_STATE %d %s",
                          game->board_size,
                          game->players[game->current_player_index]->nickname);

    // Add player names and scores
    for (int i = 0; i < game->player_count; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, " %s %d",
                           game->players[i]->nickname,
                           game->player_scores[i]);
    }

    // Add card states (0=hidden, value=matched)
    for (int i = 0; i < game->total_cards; i++) {
        if (game->cards[i].state == CARD_MATCHED) {
            offset += snprintf(buffer + offset, buffer_size - offset, " %d",
                              game->cards[i].value);
        } else {
            offset += snprintf(buffer + offset, buffer_size - offset, " 0");
        }
    }

    return 0;
}
