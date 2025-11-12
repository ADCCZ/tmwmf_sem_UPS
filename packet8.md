# Packet 8 - Disconnect/Reconnect & Keepalive

**Author:** Generated implementation
**Date:** 2025-01-12
**Status:** ✅ Completed

## Overview

This packet implements connection resilience with PING/PONG keepalive, automatic reconnection, timeout detection, and game state restoration. The system handles both short disconnects (< 60s with automatic reconnection) and long disconnects (> 60s with player removal).

## Implementation Summary

### 1. Server Side - Connection Management

#### client_list.h/c (New Module)
**Location:** `server_src/client_list.h`, `server_src/client_list.c`

Global client tracking for PING/PONG and timeout management:
- Thread-safe client list with mutex protection
- Add/remove clients from global list
- Find client by ID for reconnection
- Get all active clients for broadcast operations

**Key Functions:**
- `client_list_init()`: Initialize client tracking
- `client_list_add()`: Register new client
- `client_list_remove()`: Unregister client
- `client_list_find_by_id()`: Lookup for reconnection
- `client_list_get_all()`: Enumerate all clients

#### protocol.h - Timeout Constants
**Location:** `server_src/protocol.h`

Added timeout configuration:
```c
#define PING_INTERVAL 15           // Send PING every 15 seconds
#define PONG_TIMEOUT 5             // Expect PONG within 5 seconds
#define SHORT_DISCONNECT_TIMEOUT 60    // < 60s = reconnect possible
#define LONG_DISCONNECT_TIMEOUT 60     // >= 60s = player removed
```

#### server.c - PING and Timeout Threads
**Location:** `server_src/server.c`

**PING Thread (`ping_thread_func`):**
- Runs every PING_INTERVAL (15s)
- Sends PING to all authenticated clients
- Logs each PING transmission

**Timeout Checker Thread (`timeout_checker_thread_func`):**
- Checks every 5 seconds
- Calculates inactive time: `now - client->last_activity`
- Timeout threshold: PING_INTERVAL + PONG_TIMEOUT (20s)
- Closes socket to trigger disconnect if timeout exceeded

**Initialization:**
```c
// Initialize client list
client_list_init(max_clients);

// Start PING thread
pthread_create(&ping_thread, NULL, ping_thread_func, NULL);

// Start timeout checker
pthread_create(&timeout_thread, NULL, timeout_checker_thread_func, NULL);
```

#### client_handler.c - PONG and RECONNECT Handlers
**Location:** `server_src/client_handler.c`

**handle_pong():**
```c
static void handle_pong(client_t *client) {
    client->last_activity = time(NULL);
    logger_log(LOG_INFO, "Client %d: PONG received", client->client_id);
}
```

**handle_reconnect():**
1. Parse client_id from `RECONNECT <client_id>` command
2. Find old client by ID using `client_list_find_by_id()`
3. Check disconnect duration < SHORT_DISCONNECT_TIMEOUT
4. Transfer state (nickname, room, game position) from old to new client
5. Update room and game player pointers
6. Send WELCOME with original client_id
7. Send GAME_STATE if in active game
8. Broadcast PLAYER_RECONNECTED to room

**Connection Cleanup:**
```c
// Remove from client list before freeing
client_list_remove(client);

// Broadcast PLAYER_DISCONNECTED LONG
char broadcast[MAX_MESSAGE_LENGTH];
snprintf(broadcast, sizeof(broadcast), "PLAYER_DISCONNECTED %s LONG", client->nickname);
room_broadcast(room, broadcast);
```

#### game.c - GAME_STATE Message Formatting
**Location:** `server_src/game.c`

Enhanced `game_format_state_message()`:
```c
// Format: GAME_STATE <board_size> <current_player_nick> <player1> <score1> <player2> <score2> ... <card_states>

// Add player names and scores
for (int i = 0; i < game->player_count; i++) {
    offset += snprintf(buffer + offset, buffer_size - offset, " %s %d",
                       game->players[i]->nickname,
                       game->player_scores[i]);
}

// Add card states (0=hidden, value=matched)
for (int i = 0; i < game->total_cards; i++) {
    if (game->cards[i].state == CARD_MATCHED) {
        offset += snprintf(buffer + offset, buffer_size - offset, " %d",
                          game->cards[i].value);
    } else {
        offset += snprintf(buffer + offset, buffer_size - offset, " 0");
    }
}
```

### 2. Client Side - Auto-Reconnect

#### ClientConnection.java - Reconnection Logic
**Location:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/network/ClientConnection.java`

**New Fields:**
```java
private int clientId = -1;  // Client ID for reconnection
private boolean autoReconnect = false;  // Enable after authentication
private volatile boolean reconnecting = false;
```

**setClientId():**
```java
public void setClientId(int clientId) {
    this.clientId = clientId;
    this.autoReconnect = (clientId > 0);  // Enable auto-reconnect
}
```

**reconnect():**
1. Validate clientId > 0
2. Close old connection with `cleanup()`
3. Wait RECONNECT_INTERVAL_MS (2000ms)
4. Create new socket connection
5. Send `RECONNECT <client_id>` command
6. Wait for WELCOME response
7. Restart reader thread if successful
8. Return success/failure

**Auto-Reconnect in readerLoop():**
```java
finally {
    running = false;
    cleanup();

    // Attempt automatic reconnect if enabled
    if (autoReconnect && clientId > 0 && !reconnecting) {
        for (int attempt = 1; attempt <= MAX_RECONNECT_ATTEMPTS; attempt++) {
            if (reconnect()) {
                return;  // Success!
            }
            Thread.sleep(RECONNECT_INTERVAL_MS);
        }

        // Failed after all attempts
        if (listener != null) {
            listener.onDisconnected("Failed to reconnect");
        }
    }
}
```

**Configuration (ProtocolConstants.java):**
```java
public static final int RECONNECT_INTERVAL_MS = 2000;  // 2s between attempts
public static final int MAX_RECONNECT_ATTEMPTS = 5;    // Max 5 attempts
```

#### LoginController.java - Client ID Parsing
**Location:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/controller/LoginController.java`

Parse client ID from WELCOME:
```java
if (message.startsWith(ProtocolConstants.CMD_WELCOME)) {
    // Format: WELCOME <client_id> [message]
    String[] parts = message.split(" ");
    if (parts.length >= 2) {
        int clientId = Integer.parseInt(parts[1]);
        connection.setClientId(clientId);
        log("Client ID: " + clientId);
    }
    switchToLobby();
}
```

#### GameController.java - GAME_STATE Restoration
**Location:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/controller/GameController.java`

**handleGameState():**
```java
private void handleGameState(String message) {
    // Format: GAME_STATE <board_size> <current_player_nick> <player1> <score1> ... <card_states>
    String[] parts = message.split(" ");

    int size = Integer.parseInt(parts[1]);
    String currentPlayerNick = parts[2];

    // Parse players and scores
    int index = 3;
    while (index < parts.length - (size * size)) {
        String playerName = parts[index++];
        int score = Integer.parseInt(parts[index++]);
        players.add(playerName);
        playerScores.put(playerName, score);
    }

    // Initialize board if needed
    if (boardSize != size) {
        initializeBoard(size);
    }

    // Restore card states
    for (int i = 0; i < boardSize * boardSize; i++) {
        int cardValue = Integer.parseInt(parts[index++]);

        if (cardValue > 0) {
            // Card is matched - show it
            cardMatched[i] = true;
            cardValues[i] = cardValue;
            // Update UI: green background, show value, disable button
        }
    }

    updatePlayers();
    updateTurnLabel(currentPlayerNick);
    updateStatus("Game state restored after reconnection");
}
```

## Protocol Messages

### PING/PONG Keepalive

**Server → Client:**
```
PING
```

**Client → Server:**
```
PONG
```

### Reconnection

**Client → Server:**
```
RECONNECT <client_id>
```

**Server → Client (Success):**
```
WELCOME <client_id> Reconnected successfully
GAME_STATE <board_size> <current_player> ...
```

**Server → Client (Failure):**
```
ERROR Client not found or session expired
ERROR Session expired (timeout > 60s)
```

### Disconnect Notifications

**Server → All Players in Room:**
```
PLAYER_DISCONNECTED <nickname> SHORT
PLAYER_DISCONNECTED <nickname> LONG
PLAYER_RECONNECTED <nickname>
```

## Flow Diagrams

### Short Disconnect (< 60s)

```
Client                  Server
  |                        |
  |  <<network issue>>     |
  X----------------------->X
  |                        |
  | (auto-reconnect loop)  |
  | wait 2s                |
  |                        |
  |---RECONNECT 123------->|
  |                        | (validate)
  |                        | (< 60s OK)
  |<---WELCOME 123---------|
  |<---GAME_STATE----------|
  |                        |
  | (game restored)        |
```

### Long Disconnect (> 60s)

```
Client                  Server
  |                        |
  |  <<network issue>>     |
  X----------------------->X
  |                        |
  | (60s timeout)          | (timeout detected)
  |                        |---PLAYER_DISCONNECTED LONG--→ Other Players
  |                        | (remove from game)
  | (auto-reconnect loop)  |
  |---RECONNECT 123------->|
  |                        | (session expired)
  |<---ERROR (expired)-----|
  |                        |
  | (reconnect failed)     |
```

### Normal Keepalive

```
Client                  Server (PING thread)
  |                        |
  |                        | (every 15s)
  |<-------PING------------|
  |                        |
  |-------PONG------------>| (updates last_activity)
  |                        |
  | (timeout checker)      |
  | now - last_activity    |
  | < 20s: OK              |
```

## Files Modified

### Server (C):
- **server_src/protocol.h** - Added timeout constants
- **server_src/client_list.h** (new) - Client tracking interface
- **server_src/client_list.c** (new) - Client tracking implementation
- **server_src/server.c** - PING thread, timeout checker, initialization
- **server_src/client_handler.c** - PONG handler, RECONNECT handler, cleanup
- **server_src/game.c** - Enhanced GAME_STATE formatting with player names and card values
- **server_src/Makefile** - Added client_list.c to build

### Client (Java):
- **client_src/.../network/ClientConnection.java** - Auto-reconnect logic, clientId tracking
- **client_src/.../protocol/ProtocolConstants.java** - Reconnect constants
- **client_src/.../controller/LoginController.java** - Parse and set clientId
- **client_src/.../controller/GameController.java** - GAME_STATE handling and UI restoration

## Testing Scenarios

### 1. Normal Keepalive Test
```bash
# Start server
cd server_src && make && ./server 127.0.0.1 10000 10 50

# Start client, join game
# Observe PING/PONG messages in logs every 15s
```

### 2. Short Disconnect Test (Simulate Network Drop)
```bash
# During active game:
# Linux: iptables -A OUTPUT -p tcp --dport 10000 -j DROP
# Windows: Disconnect network adapter temporarily

# Wait < 60s, reconnect network
# Client should auto-reconnect
# Game state should be restored

# Cleanup: iptables -D OUTPUT -p tcp --dport 10000 -j DROP
```

### 3. Long Disconnect Test
```bash
# During active game, disconnect network
# Wait > 60 seconds
# Reconnect network
# Client should fail to reconnect
# Server should have removed player from game
```

### 4. Timeout Detection Test
```bash
# Start client, authenticate
# Modify client to NOT send PONG responses
# Server should timeout and disconnect after 20s (PING_INTERVAL + PONG_TIMEOUT)
```

## Known Limitations

1. **Game Pausing:** Short disconnects don't pause the game - other players must wait for timeout or reconnection.

2. **Reconnect Window:** 60-second window is fixed (not configurable at runtime).

3. **Card Values:** Hidden card values are not sent in GAME_STATE (security consideration).

4. **Multiple Reconnects:** If multiple clients disconnect simultaneously, last_activity tracking may not reflect individual client states accurately during reconnection.

5. **Reconnect During Turn:** If a player disconnects during their turn, the turn doesn't automatically pass to the next player until timeout.

## Future Improvements

- Configurable timeout values (PING_INTERVAL, PONG_TIMEOUT, etc.)
- Game pause on short disconnect with resume notification
- Progressive backoff for reconnect attempts
- Connection quality indicators in UI
- Reconnect history logging for debugging

## Summary

Packet 8 successfully implements:
- ✅ PING/PONG keepalive every 15s with 5s PONG timeout
- ✅ Automatic timeout detection (20s threshold)
- ✅ Client auto-reconnect (up to 5 attempts, 2s intervals)
- ✅ Short disconnect handling (< 60s): reconnect with state restoration
- ✅ Long disconnect handling (> 60s): player removal and notification
- ✅ GAME_STATE message for full game restoration after reconnect
- ✅ Thread-safe client tracking with global client list
- ✅ PLAYER_DISCONNECTED/PLAYER_RECONNECTED broadcasts

The system now provides robust connection resilience suitable for unstable network conditions!
