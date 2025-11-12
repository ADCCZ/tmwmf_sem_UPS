#include "client_list.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

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

    if (client_array != NULL) {
        free(client_array);
        client_array = NULL;
    }

    max_clients = 0;
    client_count = 0;

    logger_log(LOG_INFO, "Client list shutdown");
    pthread_mutex_unlock(&list_mutex);
}

int client_list_add(client_t *client) {
    if (client == NULL) return -1;

    pthread_mutex_lock(&list_mutex);

    // Find empty slot
    for (int i = 0; i < max_clients; i++) {
        if (client_array[i] == NULL) {
            client_array[i] = client;
            client_count++;
            logger_log(LOG_INFO, "Client %d added to list (total: %d)",
                      client->client_id, client_count);
            pthread_mutex_unlock(&list_mutex);
            return 0;
        }
    }

    logger_log(LOG_WARNING, "Client list full, cannot add client %d", client->client_id);
    pthread_mutex_unlock(&list_mutex);
    return -1;
}

void client_list_remove(client_t *client) {
    if (client == NULL) return;

    pthread_mutex_lock(&list_mutex);

    for (int i = 0; i < max_clients; i++) {
        if (client_array[i] == client) {
            client_array[i] = NULL;
            client_count--;
            logger_log(LOG_INFO, "Client %d removed from list (total: %d)",
                      client->client_id, client_count);
            pthread_mutex_unlock(&list_mutex);
            return;
        }
    }

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
        if (client_array[i] != NULL && client_array[i]->client_id == client_id) {
            client_t *client = client_array[i];
            pthread_mutex_unlock(&list_mutex);
            return client;
        }
    }

    pthread_mutex_unlock(&list_mutex);
    return NULL;
}
