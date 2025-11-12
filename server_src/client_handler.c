#include "client_handler.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

int client_send_message(client_t *client, const char *message) {
    if (client == NULL || message == NULL) {
        return -1;
    }

    char buffer[MAX_MESSAGE_LENGTH + 2];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", message);

    if (len < 0 || len >= (int)sizeof(buffer)) {
        logger_log(LOG_ERROR, "Client %d: Message too long or formatting error", client->client_id);
        return -1;
    }

    int sent = send(client->socket_fd, buffer, len, 0);
    if (sent < 0) {
        logger_log(LOG_ERROR, "Client %d: Failed to send message", client->client_id);
        return -1;
    }

    return sent;
}

void* client_handler_thread(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[MAX_MESSAGE_LENGTH];
    char line_buffer[MAX_MESSAGE_LENGTH];
    int line_pos = 0;

    logger_log(LOG_INFO, "Client %d: Handler thread started (fd=%d)", client->client_id, client->socket_fd);

    // Send initial test message
    client_send_message(client, "ERROR NOT_IMPLEMENTED Server skeleton - all commands return this error");

    while (1) {
        int bytes_received = recv(client->socket_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received < 0) {
            logger_log(LOG_ERROR, "Client %d: recv() failed", client->client_id);
            break;
        }

        if (bytes_received == 0) {
            logger_log(LOG_INFO, "Client %d: Connection closed by client", client->client_id);
            break;
        }

        buffer[bytes_received] = '\0';

        // Process received data character by character to handle \n delimited messages
        for (int i = 0; i < bytes_received; i++) {
            char c = buffer[i];

            if (c == '\n') {
                // End of message
                line_buffer[line_pos] = '\0';

                if (line_pos > 0) {
                    // Log the received message
                    logger_log(LOG_INFO, "Client %d: Received message: '%s'", client->client_id, line_buffer);

                    // Update last activity
                    client->last_activity = time(NULL);

                    // For now, just respond with NOT_IMPLEMENTED
                    char response[MAX_MESSAGE_LENGTH];
                    snprintf(response, sizeof(response), "ERROR %s Command not yet implemented", ERR_NOT_IMPLEMENTED);
                    client_send_message(client, response);
                }

                // Reset line buffer
                line_pos = 0;
            } else if (c == '\r') {
                // Ignore CR (in case client sends \r\n)
                continue;
            } else {
                // Add character to line buffer
                if (line_pos < MAX_MESSAGE_LENGTH - 1) {
                    line_buffer[line_pos++] = c;
                } else {
                    // Line too long - treat as invalid
                    logger_log(LOG_WARNING, "Client %d: Message too long, truncating", client->client_id);
                    client->invalid_message_count++;
                    line_pos = 0;
                }
            }
        }
    }

    // Cleanup
    logger_log(LOG_INFO, "Client %d: Closing connection", client->client_id);
    close(client->socket_fd);
    free(client);

    return NULL;
}
