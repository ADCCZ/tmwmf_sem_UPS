#ifndef SERVER_H
#define SERVER_H

/**
 * Server module - TCP socket management and accept loop
 */

typedef struct {
    char ip[64];
    int port;
    int max_rooms;
    int max_clients;
    int listen_fd;
    int running;
    int next_client_id;
} server_config_t;

/**
 * Initialize the server
 * @param ip IP address to bind to
 * @param port Port number to listen on
 * @param max_rooms Maximum number of game rooms
 * @param max_clients Maximum number of connected clients
 * @return 0 on success, -1 on error
 */
int server_init(const char *ip, int port, int max_rooms, int max_clients);

/**
 * Start the server main loop (accepts connections and spawns threads)
 */
void server_run(void);

/**
 * Shutdown the server and cleanup resources
 */
void server_shutdown(void);

/**
 * Get the server configuration (for signal handlers)
 */
server_config_t* server_get_config(void);

#endif /* SERVER_H */
