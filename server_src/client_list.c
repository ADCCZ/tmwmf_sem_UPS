#include "client_list.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static client_t **client_array = NULL;
static int max_clients = 0;
static int client_count = 0;
static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_list_init(int max) {
    pthread_mutex_lock(&list_mutex);

    max_clients = max;
    client_array = (client_t **)calloc(max_clients, sizeof(client_t *));

    if (client_array == NULL) {
        logger_log(LOG_ERROR, "Failed to allocate client list");
        pthread_mutex_unlock(&list_mutex);
        return -1;
    }

    client_count = 0;
    logger_log(LOG_INFO, "Client list initialized (max: %d)", max_clients);

    pthread_mutex_unlock(&list_mutex);
    return 0;
}

void client_list_shutdown(void) {
    pthread_mutex_lock(&list_mutex);

    logger_log(LOG_INFO, "Client list shutdown starting (total clients: %d)...", client_count);

    if (client_array != NULL) {
        // Free all client structures first
        int freed_count = 0;
        for (int i = 0; i < max_clients; i++) {
            if (client_array[i] != NULL) {
                client_t *client = client_array[i];

                logger_log(LOG_INFO, "Freeing client %d at index %d (socket_fd=%d, nickname='%s')",
                          client->client_id, i, client->socket_fd,
                          client->nickname[0] != '\0' ? client->nickname : "(no name)");

                // Close socket if still open (handler thread may have already closed it)
                if (client->socket_fd >= 0) {
                    close(client->socket_fd);
                    client->socket_fd = -1;
                }

                // Mark as NULL BEFORE free to prevent use-after-free if another thread accesses
                client_array[i] = NULL;
                client_count--;

                // CRITICAL: Clear ALL other references to this client (duplicates)
                for (int j = i + 1; j < max_clients; j++) {
                    if (client_array[j] == client) {
                        logger_log(LOG_ERROR, "BUG: Found duplicate of client %d at index %d - clearing to prevent double-free!",
                                  client->client_id, j);
                        client_array[j] = NULL;
                        client_count--;
                    }
                }

                // Free the client structure
                free(client);
                freed_count++;

                logger_log(LOG_INFO, "Client freed successfully (%d/%d)", freed_count, client_count + freed_count);
            }
        }

        logger_log(LOG_INFO, "All clients freed, freeing client array...");

        // Then free the array itself
        free(client_array);
        client_array = NULL;
    }

    max_clients = 0;
    client_count = 0;

    logger_log(LOG_INFO, "Client list shutdown complete");
    pthread_mutex_unlock(&list_mutex);
}

int client_list_add(client_t *client) {
    if (client == NULL) return -1;

    pthread_mutex_lock(&list_mutex);

    // Check if client_id is already in list (prevent duplicate IDs)
    for (int i = 0; i < max_clients; i++) {
        if (client_array[i] != NULL && client_array[i]->client_id == client->client_id) {
            logger_log(LOG_ERROR, "BUG: Attempting to add client %d which already has same ID in list at index %d!",
                      client->client_id, i);
            pthread_mutex_unlock(&list_mutex);
            return -1;
        }
    }

    // Find empty slot or zombie client slot
    int zombie_slot = -1;
    for (int i = 0; i < max_clients; i++) {
        if (client_array[i] == NULL) {
            // Found empty slot
            client_array[i] = client;
            client_count++;
            logger_log(LOG_INFO, "Client %d added to list at index %d (total: %d)",
                      client->client_id, i, client_count);
            pthread_mutex_unlock(&list_mutex);
            return 0;
        }

        // Track first zombie client (disconnected, waiting for reconnect) as backup
        if (zombie_slot == -1 && client_array[i]->is_disconnected && client_array[i]->socket_fd == -1) {
            zombie_slot = i;
        }
    }

    // No empty slot found - check if we can use a zombie slot
    if (zombie_slot != -1) {
        client_t *zombie = client_array[zombie_slot];
        logger_log(LOG_WARNING, "Client list full - replacing zombie client %d at index %d with new client %d",
                  zombie->client_id, zombie_slot, client->client_id);

        // Clear the slot and free zombie (don't call client_list_remove - we already have mutex!)
        client_array[zombie_slot] = NULL;
        // Note: client_count stays same (removing and adding = no change)

        // Free the zombie memory
        free(zombie);

        // Now add new client to the slot
        client_array[zombie_slot] = client;
        logger_log(LOG_INFO, "Client %d added to list at index %d (replaced zombie, total: %d)",
                  client->client_id, zombie_slot, client_count);
        pthread_mutex_unlock(&list_mutex);
        return 0;
    }

    logger_log(LOG_WARNING, "Client list full, cannot add client %d", client->client_id);
    pthread_mutex_unlock(&list_mutex);
    return -1;
}

void client_list_remove(client_t *client) {
    if (client == NULL) return;

    pthread_mutex_lock(&list_mutex);

    int removed_count = 0;
    for (int i = 0; i < max_clients; i++) {
        if (client_array[i] == client) {
            client_array[i] = NULL;
            client_count--;
            removed_count++;
            logger_log(LOG_INFO, "Client %d removed from list at index %d (total: %d)",
                      client->client_id, i, client_count);
        }
    }

    if (removed_count == 0) {
        logger_log(LOG_WARNING, "Client %d not found in list during removal", client->client_id);
    } else if (removed_count > 1) {
        logger_log(LOG_ERROR, "BUG: Client %d was in list %d times! (DUPLICATE)", client->client_id, removed_count);
    }

    pthread_mutex_unlock(&list_mutex);
}

void client_list_replace(client_t *old_client, client_t *new_client) {
    if (old_client == NULL || new_client == NULL) return;

    pthread_mutex_lock(&list_mutex);

    int old_client_index = -1;
    int new_client_index = -1;

    // Find both old and new client positions
    for (int i = 0; i < max_clients; i++) {
        if (client_array[i] == old_client) {
            old_client_index = i;
        }
        if (client_array[i] == new_client) {
            new_client_index = i;
        }
    }

    if (old_client_index == -1) {
        logger_log(LOG_WARNING, "Client %d (old) not found for replacement",
                  old_client->client_id);
        pthread_mutex_unlock(&list_mutex);
        return;
    }

    // If new_client is already in list (was added before RECONNECT), remove it first
    if (new_client_index != -1) {
        logger_log(LOG_INFO, "Client %d (new) already in list at index %d, removing before replace",
                  new_client->client_id, new_client_index);
        client_array[new_client_index] = NULL;
        client_count--;
    }

    // Replace old client with new client
    client_array[old_client_index] = new_client;
    logger_log(LOG_INFO, "Client %d replaced in list at index %d (reconnect, same ID)",
              new_client->client_id, old_client_index);

    pthread_mutex_unlock(&list_mutex);
}

int client_list_get_all(client_t **clients, int max_count) {
    if (clients == NULL) return 0;

    pthread_mutex_lock(&list_mutex);

    int count = 0;
    for (int i = 0; i < max_clients && count < max_count; i++) {
        if (client_array[i] != NULL) {
            clients[count++] = client_array[i];
        }
    }

    pthread_mutex_unlock(&list_mutex);
    return count;
}

client_t* client_list_find_by_id(int client_id) {
    pthread_mutex_lock(&list_mutex);

    for (int i = 0; i < max_clients; i++) {
        if (client_array[i] != NULL &&
            client_array[i]->client_id == client_id) {
            client_t *client = client_array[i];
            pthread_mutex_unlock(&list_mutex);
            return client;
        }
    }

    pthread_mutex_unlock(&list_mutex);
    return NULL;
}
