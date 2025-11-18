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

    // Start PING thread
    int result = pthread_create(&ping_thread, NULL, ping_thread_func, NULL);
    if (result != 0) {
        logger_log(LOG_ERROR, "Failed to create PING thread: %s", strerror(result));
        client_list_shutdown();
        room_system_shutdown();
        close(server_config.listen_fd);
        return -1;
    }
    pthread_detach(ping_thread);
    logger_log(LOG_INFO, "PING thread started");

    // Start timeout checker thread
    result = pthread_create(&timeout_thread, NULL, timeout_checker_thread_func, NULL);
    if (result != 0) {
        logger_log(LOG_ERROR, "Failed to create timeout checker thread: %s", strerror(result));
        client_list_shutdown();
        room_system_shutdown();
        close(server_config.listen_fd);
        return -1;
    }
    pthread_detach(timeout_thread);
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

    // Shutdown client list
    client_list_shutdown();

    // Shutdown room system
    room_system_shutdown();

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

            // Skip zombie clients (being freed after reconnect)
            if (client->socket_fd == -2) {
                continue;
            }

            if (client->state >= STATE_AUTHENTICATED && !client->is_disconnected) {
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

            // Free zombie clients (marked during reconnect)
            // Must remove from list AND free memory
            if (client->socket_fd == -2) {
                logger_log(LOG_INFO, "Client %d: Freeing zombie client after reconnect",
                          client->client_id);

                // Clear ALL pointers to this client in the snapshot array (in case it appears multiple times)
                client_t *zombie = client;
                for (int j = 0; j < count; j++) {
                    if (clients[j] == zombie) {
                        clients[j] = NULL;
                    }
                }

                // Remove from list (now safe since reconnection is complete)
                client_list_remove(zombie);
                free(zombie);
                continue;
            }

            // Check for disconnected players waiting for reconnection
            if (client->is_disconnected) {
                time_t disconnect_duration = now - client->disconnect_time;

                if (disconnect_duration >= SHORT_DISCONNECT_TIMEOUT) {
                    logger_log(LOG_WARNING, "Client %d (%s) reconnect timeout expired after %ld seconds",
                              client->client_id, client->nickname, disconnect_duration);

                    // Notify other players about LONG disconnect
                    if (client->room != NULL) {
                        room_t *room = client->room;

                        char broadcast[MAX_MESSAGE_LENGTH];
                        snprintf(broadcast, sizeof(broadcast), "PLAYER_DISCONNECTED %s LONG", client->nickname);
                        room_broadcast_except(room, broadcast, client);

                        // End the game since player didn't reconnect (if game exists)
                        if (room->game != NULL) {
                            logger_log(LOG_INFO, "Room %d: Ending game due to player %s disconnect timeout",
                                      room->room_id, client->nickname);

                            // Broadcast game end due to disconnect
                            char game_end_msg[MAX_MESSAGE_LENGTH];
                            snprintf(game_end_msg, sizeof(game_end_msg),
                                    "GAME_END DISCONNECT Player %s did not reconnect", client->nickname);
                            room_broadcast(room, game_end_msg);

                            // Clean up game
                            game_destroy((game_t *)room->game);
                            room->game = NULL;
                            room->state = ROOM_STATE_FINISHED;

                            // Return remaining players to lobby (before removing disconnected player)
                            for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                                if (room->players[j] != NULL && room->players[j] != client) {
                                    room->players[j]->room = NULL;
                                    room->players[j]->state = STATE_IN_LOBBY;
                                    // IMPORTANT: Reset is_disconnected flag to prevent processing them again in this timeout check
                                    room->players[j]->is_disconnected = 0;
                                    logger_log(LOG_INFO, "Client %d (%s) returned to lobby after game end (disconnect)",
                                               room->players[j]->client_id, room->players[j]->nickname);
                                }
                            }
                        } else {
                            // No game yet - just notify about leaving
                            logger_log(LOG_INFO, "Room %d: Player %s disconnect timeout (no active game)",
                                      room->room_id, client->nickname);
                        }

                        // Remove disconnected player from room
                        // NOTE: room_remove_player will auto-destroy room if it becomes empty
                        room_remove_player(room, client);
                    }

                    // Remove client from list and free
                    client_list_remove(client);

                    // Clear ALL pointers to this client in the snapshot array (in case it appears multiple times)
                    client_t *timeout_client = client;
                    for (int j = 0; j < count; j++) {
                        if (clients[j] == timeout_client) {
                            clients[j] = NULL;
                        }
                    }

                    free(timeout_client);
                }
                continue;
            }

            // Only check authenticated clients with valid socket (skip disconnected)
            if (client->state >= STATE_AUTHENTICATED && client->socket_fd >= 0 && !client->is_disconnected) {
                // Check if client hasn't responded to PING within PONG_TIMEOUT
                if (client->waiting_for_pong) {
                    time_t ping_wait_time = now - client->last_ping_time;

                    if (ping_wait_time > PONG_TIMEOUT) {
                        logger_log(LOG_WARNING, "Client %d (%s) didn't respond to PING within %d seconds",
                                  client->client_id, client->nickname, PONG_TIMEOUT);

                        // Use shutdown() instead of close() to avoid closing recycled FDs
                        // This makes recv() return 0, triggering client_handler cleanup
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
        }
    }

    logger_log(LOG_INFO, "Timeout checker thread terminated");
    return NULL;
}
