#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

#include "client_handler.h"
#include <pthread.h>

/**
 * Client list module - tracks all connected clients for PING/timeout management
 */

/**
 * Initialize the client list
 * @param max_clients Maximum number of clients
 * @return 0 on success, -1 on error
 */
int client_list_init(int max_clients);

/**
 * Shutdown the client list
 */
void client_list_shutdown(void);

/**
 * Add a client to the list
 * @param client Client to add
 * @return 0 on success, -1 on error
 */
int client_list_add(client_t *client);

/**
 * Remove a client from the list
 * @param client Client to remove
 */
void client_list_remove(client_t *client);

/**
 * Get all active clients
 * @param clients Output array (must be allocated by caller)
 * @param max_count Size of output array
 * @return Number of clients copied
 */
int client_list_get_all(client_t **clients, int max_count);

/**
 * Find client by ID
 * @param client_id Client ID to search for
 * @return Client pointer or NULL if not found
 */
client_t* client_list_find_by_id(int client_id);

#endif /* CLIENT_LIST_H */
