#include "server.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>

void print_usage(const char *program_name) {
    printf("Usage: %s <IP> <PORT> <MAX_ROOMS> <MAX_CLIENTS>\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  IP           - IP address to bind to (e.g., 127.0.0.1 or 0.0.0.0)\n");
    printf("  PORT         - Port number to listen on (e.g., 10000)\n");
    printf("  MAX_ROOMS    - Maximum number of game rooms (e.g., 10)\n");
    printf("  MAX_CLIENTS  - Maximum number of connected clients (e.g., 50)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s 127.0.0.1 10000 10 50\n", program_name);
}

void signal_handler(int signum) {
    logger_log(LOG_INFO, "Received signal %d, shutting down...", signum);

    // Get server config and stop it
    server_config_t *config = server_get_config();
    if (config != NULL) {
        config->running = 0;

        // Close listen socket to unblock accept()
        // This makes accept() return immediately with error
        if (config->listen_fd >= 0) {
            shutdown(config->listen_fd, SHUT_RDWR);
        }
    }
}

int main(int argc, char *argv[]) {
    // Check arguments
    if (argc != 5) {
        fprintf(stderr, "Error: Invalid number of arguments\n\n");
        print_usage(argv[0]);
        return 1;
    }

    // Parse arguments
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int max_rooms = atoi(argv[3]);
    int max_clients = atoi(argv[4]);

    // Validate arguments
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number (must be 1-65535)\n");
        return 1;
    }

    if (max_rooms <= 0) {
        fprintf(stderr, "Error: MAX_ROOMS must be greater than 0\n");
        return 1;
    }

    if (max_clients <= 0) {
        fprintf(stderr, "Error: MAX_CLIENTS must be greater than 0\n");
        return 1;
    }

    // Initialize logger
    if (logger_init("server.log") != 0) {
        fprintf(stderr, "Warning: Failed to initialize logger file, using stdout only\n");
    }

    logger_log(LOG_INFO, "=== Pexeso Server Starting ===");
    logger_log(LOG_INFO, "Configuration: IP=%s, Port=%d, MaxRooms=%d, MaxClients=%d",
               ip, port, max_rooms, max_clients);

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize server
    if (server_init(ip, port, max_rooms, max_clients) != 0) {
        logger_log(LOG_ERROR, "Failed to initialize server");
        logger_shutdown();
        return 1;
    }

    // Run server
    server_run();

    // Cleanup
    server_shutdown();
    logger_shutdown();

    printf("Server terminated\n");

    // Use exit(0) instead of return 0 to terminate immediately
    // This prevents waiting for detached threads to finish sleeping
    exit(0);
}
