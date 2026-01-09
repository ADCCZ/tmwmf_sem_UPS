#include "server.h"
#include "client_handler.h"
#include "client_list.h"
#include "room.h"
#include "game.h"
#include "logger.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

static server_config_t server_config;
static pthread_t ping_thread;
static pthread_t timeout_thread;

// Forward declarations
static void* ping_thread_func(void *arg);
static void* timeout_checker_thread_func(void *arg);

server_config_t* server_get_config(void) {
    return &server_config;
}

int server_init(const char *ip, int port, int max_rooms, int max_clients) {
    // Initialize configuration
    strncpy(server_config.ip, ip, sizeof(server_config.ip) - 1);
    server_config.ip[sizeof(server_config.ip) - 1] = '\0';
    server_config.port = port;
    server_config.max_rooms = max_rooms;
    server_config.max_clients = max_clients;
    server_config.running = 1;
    server_config.next_client_id = 1;

    // Create socket
    server_config.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_config.listen_fd < 0) {
        logger_log(LOG_ERROR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Set SO_REUSEADDR to avoid "Address already in use" error
    int opt = 1;
    if (setsockopt(server_config.listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger_log(LOG_WARNING, "Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    // Prepare address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Convert IP address
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        logger_log(LOG_ERROR, "Invalid IP address: %s", ip);
        close(server_config.listen_fd);
        return -1;
    }

    // Bind socket
    if (bind(server_config.listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        logger_log(LOG_ERROR, "Failed to bind to %s:%d: %s", ip, port, strerror(errno));
        close(server_config.listen_fd);
        return -1;
    }

    // Listen for connections
    if (listen(server_config.listen_fd, 10) < 0) {
        logger_log(LOG_ERROR, "Failed to listen: %s", strerror(errno));
        close(server_config.listen_fd);
        return -1;
    }

    // Initialize room system
    if (room_system_init(max_rooms) != 0) {
        logger_log(LOG_ERROR, "Failed to initialize room system");
        close(server_config.listen_fd);
        return -1;
    }

    // Initialize client list
    if (client_list_init(max_clients) != 0) {
        logger_log(LOG_ERROR, "Failed to initialize client list");
        room_system_shutdown();
        close(server_config.listen_fd);
        return -1;
    }

    // Start PING thread (DON'T detach - we need to join on shutdown)
    int result = pthread_create(&ping_thread, NULL, ping_thread_func, NULL);
    if (result != 0) {
        logger_log(LOG_ERROR, "Failed to create PING thread: %s", strerror(result));
        client_list_shutdown();
        room_system_shutdown();
        close(server_config.listen_fd);
        return -1;
    }
    logger_log(LOG_INFO, "PING thread started");

    // Start timeout checker thread (DON'T detach - we need to join on shutdown)
    result = pthread_create(&timeout_thread, NULL, timeout_checker_thread_func, NULL);
    if (result != 0) {
        logger_log(LOG_ERROR, "Failed to create timeout checker thread: %s", strerror(result));
        client_list_shutdown();
        room_system_shutdown();
        close(server_config.listen_fd);
        return -1;
    }
    logger_log(LOG_INFO, "Timeout checker thread started");

    logger_log(LOG_INFO, "Server initialized: %s:%d (max_rooms=%d, max_clients=%d)",
               ip, port, max_rooms, max_clients);

    return 0;
}

void server_run(void) {
    logger_log(LOG_INFO, "Server started, waiting for connections...");

    while (server_config.running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        // Accept new connection
        int client_fd = accept(server_config.listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_fd < 0) {
            if (server_config.running) {
                logger_log(LOG_ERROR, "Failed to accept connection: %s", strerror(errno));
            }
            continue;
        }

        // Get client IP address
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        logger_log(LOG_INFO, "New connection from %s:%d (fd=%d)",
                   client_ip, ntohs(client_addr.sin_port), client_fd);

        // Create client structure
        client_t *client = (client_t *)malloc(sizeof(client_t));
        if (client == NULL) {
            logger_log(LOG_ERROR, "Failed to allocate memory for client");
            close(client_fd);
            continue;
        }

        client->socket_fd = client_fd;
        client->state = STATE_CONNECTED;
        client->last_activity = time(NULL);
        client->invalid_message_count = 0;
        client->client_id = server_config.next_client_id++;
        client->room = NULL;
        client->is_disconnected = 0;
        client->disconnect_time = 0;
        client->waiting_for_pong = 0;
        client->last_ping_time = 0;
        client->last_pong_time = time(NULL);  // Initialize to current time
        memset(client->nickname, 0, sizeof(client->nickname));

        // Add to client list
        if (client_list_add(client) != 0) {
            logger_log(LOG_ERROR, "Failed to add client %d to list", client->client_id);
            close(client_fd);
            free(client);
            continue;
        }

        // Create thread for client
        pthread_t thread_id;
        int result = pthread_create(&thread_id, NULL, client_handler_thread, client);
        if (result != 0) {
            logger_log(LOG_ERROR, "Failed to create thread for client %d: %s", client->client_id, strerror(result));
            client_list_remove(client);
            close(client_fd);
            free(client);
            continue;
        }

        // Detach thread so it cleans up automatically when done
        pthread_detach(thread_id);

        logger_log(LOG_INFO, "Client %d: Thread created successfully", client->client_id);
    }

    logger_log(LOG_INFO, "Server stopped accepting connections");
}

void server_shutdown(void) {
    logger_log(LOG_INFO, "Server shutting down...");

    server_config.running = 0;

    // Notify all clients about server shutdown
    client_t *clients[server_config.max_clients];
    int count = client_list_get_all(clients, server_config.max_clients);

    logger_log(LOG_INFO, "Notifying %d clients about shutdown", count);
    for (int i = 0; i < count; i++) {
        if (clients[i] != NULL && !clients[i]->is_disconnected) {
            client_send_message(clients[i], "SERVER_SHUTDOWN Server is shutting down");
        }
    }

    // Give messages time to be sent before closing connections
    sleep(1);

    // Force close all client sockets to make handler threads exit quickly
    logger_log(LOG_INFO, "Closing all client connections...");
    count = client_list_get_all(clients, server_config.max_clients);
    for (int i = 0; i < count; i++) {
        if (clients[i] != NULL && clients[i]->socket_fd >= 0) {
            shutdown(clients[i]->socket_fd, SHUT_RDWR);
            logger_log(LOG_INFO, "Client %d: Socket shutdown for forced disconnect", clients[i]->client_id);
        }
    }

    // Wait for PING and timeout checker threads to finish FIRST
    // This prevents double-free when timeout checker tries to free clients
    logger_log(LOG_INFO, "Waiting for PING thread to finish...");
    pthread_join(ping_thread, NULL);
    logger_log(LOG_INFO, "PING thread joined");

    logger_log(LOG_INFO, "Waiting for timeout checker thread to finish...");
    pthread_join(timeout_thread, NULL);
    logger_log(LOG_INFO, "Timeout checker thread joined");

    // Wait for handler threads to finish (they are detached, so give them time)
    // Wait AFTER joining PING/timeout threads to ensure all client processing is done
    logger_log(LOG_INFO, "Waiting for handler threads to finish...");
    sleep(3);  // Increased to 3 seconds to ensure all handler threads exit

    // Shutdown room system FIRST (it accesses client pointers)
    logger_log(LOG_INFO, "Shutting down room system...");
    room_system_shutdown();
    logger_log(LOG_INFO, "Room system shutdown complete");

    // NOW it's safe to shutdown client list (no threads accessing it, rooms cleared)
    client_list_shutdown();

    if (server_config.listen_fd >= 0) {
        close(server_config.listen_fd);
        server_config.listen_fd = -1;
    }

    logger_log(LOG_INFO, "Server shutdown complete");
}

/**
 * PING thread - sends PING to clients that have responded to previous PING
 * Only sends new PING 5 seconds after receiving PONG
 */
static void* ping_thread_func(void *arg) {
    (void)arg;

    logger_log(LOG_INFO, "PING thread running");

    while (server_config.running) {
        sleep(1);  // Check every second for more responsive timing

        if (!server_config.running) break;

        time_t now = time(NULL);

        // Get all clients
        client_t *clients[server_config.max_clients];
        int count = client_list_get_all(clients, server_config.max_clients);

        // Send PING only to clients that:
        // 1. Are authenticated and not disconnected
        // 2. Are not waiting for PONG (responded to previous PING)
        // 3. Have waited at least PONG_WAIT_INTERVAL (5s) since last PONG
        for (int i = 0; i < count; i++) {
            client_t *client = clients[i];

            if (client->state >= STATE_AUTHENTICATED && !client->is_disconnected && client->socket_fd >= 0) {
                // Check if we're not waiting for PONG
                if (!client->waiting_for_pong) {
                    // Check if enough time has passed since last PONG
                    time_t time_since_pong = now - client->last_pong_time;

                    if (time_since_pong >= PONG_WAIT_INTERVAL) {
                        // Send PING
                        int result = client_send_message(client, CMD_PING);
                        if (result > 0) {
                            client->waiting_for_pong = 1;
                            client->last_ping_time = now;
                            logger_log(LOG_INFO, "PING sent to client %d (%s)",
                                      client->client_id, client->nickname);
                        }
                    }
                }
            }
        }
    }

    logger_log(LOG_INFO, "PING thread terminated");
    return NULL;
}

/**
 * Timeout checker thread - checks for clients that haven't responded to PING
 * and handles disconnected players waiting for reconnection
 */
static void* timeout_checker_thread_func(void *arg) {
    (void)arg;

    logger_log(LOG_INFO, "Timeout checker thread running");

    while (server_config.running) {
        sleep(5); // Check every 5 seconds

        if (!server_config.running) break;

        time_t now = time(NULL);
        client_t *clients[server_config.max_clients];
        int count = client_list_get_all(clients, server_config.max_clients);

        for (int i = 0; i < count; i++) {
            client_t *client = clients[i];

            // Skip if client was already processed and freed in this iteration
            if (client == NULL) {
                continue;
            }

            // Skip invalid/zombie clients
            // (socket_fd == -1 means disconnected/closed)
            // (socket_fd < -1 means being freed or invalid)
            if (client->socket_fd < -1) {
                continue;
            }

            // Only check authenticated clients with valid socket
            if (client->state >= STATE_AUTHENTICATED && client->socket_fd >= 0) {
                // Check if client hasn't responded to PING within PONG_TIMEOUT
                if (client->waiting_for_pong) {
                    time_t ping_wait_time = now - client->last_ping_time;

                    if (ping_wait_time > PONG_TIMEOUT) {
                        logger_log(LOG_WARNING, "DEBUG: Client %d (%s) didn't respond to PING within %d seconds",
                                  client->client_id, client->nickname, PONG_TIMEOUT);

                        // Mark client as disconnected for reconnect instead of immediate cleanup
                        if (!client->is_disconnected) {
                            client->is_disconnected = 1;
                            client->disconnect_time = now;
                            logger_log(LOG_INFO, "DEBUG: Client %d (%s) marked as disconnected due to PONG timeout, will wait %ds for reconnect",
                                      client->client_id, client->nickname, RECONNECT_TIMEOUT);
                        }

                        // Close socket to trigger cleanup (but keep client in list)
                        int fd = client->socket_fd;
                        if (fd >= 0) {
                            shutdown(fd, SHUT_RDWR);
                            client->socket_fd = -1;  // Mark as closed
                        }
                        continue;  // Skip further checks for this client
                    }
                }

                // Also check for general inactivity (2 minutes)
                time_t inactive_time = now - client->last_activity;
                int timeout_threshold = 120;  // 2 minutes

                if (inactive_time > timeout_threshold) {
                    logger_log(LOG_WARNING, "Client %d (%s) timed out (inactive for %ld seconds)",
                              client->client_id, client->nickname, inactive_time);

                    // Use shutdown() instead of close() to avoid closing recycled FDs
                    int fd = client->socket_fd;
                    if (fd >= 0) {
                        shutdown(fd, SHUT_RDWR);
                        client->socket_fd = -1;  // Mark as closed
                    }
                }
            }

            // Check for disconnected players waiting for reconnect
            if (client->is_disconnected && client->socket_fd == -1) {
                time_t disconnect_duration = now - client->disconnect_time;

                // Log every 10 seconds to show progress
                if (disconnect_duration % 10 == 0 && disconnect_duration > 0) {
                    logger_log(LOG_INFO, "DEBUG: Client %d (%s) waiting for reconnect: %ld/%d seconds",
                              client->client_id, client->nickname, disconnect_duration, RECONNECT_TIMEOUT);
                }

                if (disconnect_duration > RECONNECT_TIMEOUT) {
                    logger_log(LOG_WARNING, "DEBUG: Client %d (%s) reconnect timeout expired (%ld seconds), ending game with forfeit",
                              client->client_id, client->nickname, disconnect_duration);

                    // Get room and game
                    room_t *room = client->room;
                    if (room != NULL && room->game != NULL) {
                        game_t *game = room->game;
                        int room_id = room->room_id;

                        // Give forfeit win to player(s) with highest score
                        int remaining_pairs = game->total_pairs - game->matched_pairs;

                        // Find highest score among remaining players (exclude disconnected client)
                        int highest_score = -1;
                        int winner_count = 0;

                        for (int j = 0; j < game->player_count; j++) {
                            if (game->players[j] != NULL && game->players[j] != client) {
                                if (game->player_scores[j] > highest_score) {
                                    highest_score = game->player_scores[j];
                                    winner_count = 1;
                                } else if (game->player_scores[j] == highest_score) {
                                    winner_count++;
                                }
                            }
                        }

                        // Distribute remaining pairs to winner(s)
                        if (winner_count > 0 && remaining_pairs > 0) {
                            int bonus_per_winner = remaining_pairs / winner_count;
                            int extra_pairs = remaining_pairs % winner_count;

                            for (int j = 0; j < game->player_count; j++) {
                                if (game->players[j] != NULL && game->players[j] != client) {
                                    if (game->player_scores[j] == highest_score) {
                                        game->player_scores[j] += bonus_per_winner;
                                        if (extra_pairs > 0) {
                                            game->player_scores[j]++;
                                            extra_pairs--;
                                        }
                                        logger_log(LOG_INFO, "Room %d: Player %s gets forfeit bonus (new score: %d)",
                                                  room_id, game->players[j]->nickname, game->player_scores[j]);
                                    }
                                }
                            }
                        }

                        // Build GAME_END_FORFEIT message
                        char game_end_msg[MAX_MESSAGE_LENGTH];
                        int offset = snprintf(game_end_msg, sizeof(game_end_msg), "GAME_END_FORFEIT");

                        for (int j = 0; j < game->player_count; j++) {
                            if (game->players[j] != NULL) {
                                offset += snprintf(game_end_msg + offset, sizeof(game_end_msg) - offset,
                                                  " %s %d", game->players[j]->nickname, game->player_scores[j]);
                            }
                        }

                        // Broadcast game end to remaining players
                        room_broadcast_except(room, game_end_msg, client);

                        logger_log(LOG_INFO, "Room %d: Game ended by forfeit (reconnect timeout)", room_id);

                        // Clean up game
                        game_destroy(game);
                        room->game = NULL;
                        room->state = ROOM_STATE_WAITING;

                        // Remove all players from room inline
                        for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                            if (room->players[j] != NULL) {
                                room->players[j]->room = NULL;
                                room->players[j]->state = STATE_IN_LOBBY;
                                room->players[j] = NULL;
                                logger_log(LOG_INFO, "Player removed from room after forfeit timeout");
                            }
                        }
                        room->player_count = 0;

                        logger_log(LOG_INFO, "Room %d is now empty after forfeit, destroying", room_id);

                        // Destroy the room
                        room_destroy(room);
                    }

                    // Clean up disconnected client
                    logger_log(LOG_INFO, "Client %d (%s) cleaned up after reconnect timeout",
                              client->client_id, client->nickname);
                    client_list_remove(client);
                    free(client);

                    // Mark as NULL in local array to prevent double-free if client appears multiple times
                    clients[i] = NULL;
                }
            }
        }
    }

    logger_log(LOG_INFO, "Timeout checker thread terminated");
    return NULL;
}
