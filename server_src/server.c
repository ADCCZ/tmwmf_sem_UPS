#include "server.h"
#include "client_handler.h"
#include "room.h"
#include "logger.h"
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
        memset(client->nickname, 0, sizeof(client->nickname));

        // Create thread for client
        pthread_t thread_id;
        int result = pthread_create(&thread_id, NULL, client_handler_thread, client);
        if (result != 0) {
            logger_log(LOG_ERROR, "Failed to create thread for client %d: %s", client->client_id, strerror(result));
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

    // Shutdown room system
    room_system_shutdown();

    if (server_config.listen_fd >= 0) {
        close(server_config.listen_fd);
        server_config.listen_fd = -1;
    }

    logger_log(LOG_INFO, "Server shutdown complete");
}
