#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "protocol.h"
#include <time.h>

/**
 * Client handler module - manages individual client connections
 */

typedef struct {
    int socket_fd;
    char nickname[MAX_NICK_LENGTH];
    client_state_t state;
    time_t last_activity;
    int invalid_message_count;
    int client_id;
} client_t;

/**
 * Thread function for handling a client connection
 * @param arg Pointer to client_t structure
 * @return NULL
 */
void* client_handler_thread(void *arg);

/**
 * Send a message to a client
 * @param client Client to send to
 * @param message Message to send (will be appended with \n if needed)
 * @return Number of bytes sent, or -1 on error
 */
int client_send_message(client_t *client, const char *message);

#endif /* CLIENT_HANDLER_H */
