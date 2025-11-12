# Packet 9 - Error Handling & Validation

**Author:** Generated implementation
**Date:** 2025-01-12
**Status:** ✅ Completed

## Overview

This packet implements comprehensive error handling and input validation across both server and client. The server validates all incoming commands and disconnects clients after repeated errors, while the client validates all user inputs before sending commands to prevent invalid operations.

## Implementation Summary

### 1. Server Side - Command Validation

#### protocol.h - Error Codes and Constants
**Location:** `server_src/protocol.h`

Added error codes and validation constants:
```c
// Error codes
#define ERR_INVALID_COMMAND "INVALID_COMMAND"
#define ERR_INVALID_SYNTAX "INVALID_SYNTAX"
#define ERR_INVALID_PARAMS "INVALID_PARAMS"
#define ERR_INVALID_MOVE "INVALID_MOVE"
#define ERR_NOT_AUTHENTICATED "NOT_AUTHENTICATED"
#define ERR_ALREADY_AUTHENTICATED "ALREADY_AUTHENTICATED"
#define ERR_NICK_IN_USE "NICK_IN_USE"
#define ERR_ROOM_NOT_FOUND "ROOM_NOT_FOUND"
#define ERR_ROOM_FULL "ROOM_FULL"
#define ERR_NOT_IN_ROOM "NOT_IN_ROOM"
#define ERR_NOT_ROOM_OWNER "NOT_ROOM_OWNER"
#define ERR_GAME_NOT_STARTED "GAME_NOT_STARTED"
#define ERR_NOT_YOUR_TURN "NOT_YOUR_TURN"
#define ERR_INVALID_CARD "INVALID_CARD"
#define ERR_NOT_IMPLEMENTED "NOT_IMPLEMENTED"

// Error handling
#define MAX_ERROR_COUNT 3  // Disconnect after 3 errors
```

#### client_handler.c - Error Counter and Auto-Disconnect
**Location:** `server_src/client_handler.c`

**send_error_and_count() Helper Function:**
```c
static void send_error_and_count(client_t *client, const char *error_code, const char *details) {
    char error_msg[MAX_MESSAGE_LENGTH];

    // Format error message
    if (details != NULL && strlen(details) > 0) {
        snprintf(error_msg, sizeof(error_msg), "ERROR %s %s", error_code, details);
    } else {
        snprintf(error_msg, sizeof(error_msg), "ERROR %s", error_code);
    }

    // Send error to client
    client_send_message(client, error_msg);

    // Increment error counter
    client->invalid_message_count++;

    // Log error with counter
    logger_log(LOG_WARNING, "Client %d (%s): Error #%d - %s",
               client->client_id, client->nickname,
               client->invalid_message_count, error_msg);

    // Auto-disconnect after MAX_ERROR_COUNT
    if (client->invalid_message_count >= MAX_ERROR_COUNT) {
        logger_log(LOG_WARNING, "Client %d: Disconnecting due to %d errors",
                   client->client_id, MAX_ERROR_COUNT);
        close(client->socket_fd);  // Triggers disconnect
    }
}
```

**Key Features:**
- Formats error message with code and details
- Increments `invalid_message_count` on client struct
- Logs each error with counter for debugging
- Automatically closes socket after MAX_ERROR_COUNT (3) errors
- Used consistently throughout all command handlers

#### handle_message() - Unknown Command Validation
**Location:** `server_src/client_handler.c`

Enhanced unknown command handling:
```c
static void handle_message(client_t *client, const char *message) {
    // ... parse command and params ...

    // Handle known commands
    if (strcmp(command, CMD_HELLO) == 0) {
        handle_hello(client, params);
    } else if (strcmp(command, CMD_LIST_ROOMS) == 0) {
        handle_list_rooms(client);
    }
    // ... other commands ...
    else {
        // Unknown command - use error counter
        send_error_and_count(client, ERR_INVALID_COMMAND, command);
    }
}
```

**Behavior:**
- Any unrecognized command triggers `ERR_INVALID_COMMAND`
- Includes the unknown command text in error details
- Counts toward auto-disconnect threshold

#### handle_flip() - Card Flip Validation
**Location:** `server_src/client_handler.c`

Comprehensive validation for FLIP command:

**1. Null/Empty Parameter Check:**
```c
// Validate params is not null/empty
if (params == NULL || strlen(params) == 0) {
    send_error_and_count(client, ERR_INVALID_SYNTAX, "Card index required");
    return;
}
```

**2. Numeric Validation (strtol):**
```c
// Validate card_index is a number using strtol
char *endptr;
long card_index_long = strtol(params, &endptr, 10);

// Check if conversion was successful
if (*endptr != '\0' && *endptr != ' ' && *endptr != '\n') {
    send_error_and_count(client, ERR_INVALID_SYNTAX, "Card index must be a number");
    return;
}

int card_index = (int)card_index_long;
```

**Why strtol() instead of atoi():**
- `atoi()` returns 0 for both "0" and invalid input (no error detection)
- `strtol()` sets `endptr` to point to first non-digit character
- If `*endptr != '\0'` (excluding whitespace), the input wasn't a valid number
- Allows detection of inputs like "5abc", "xyz", "12.34"

**3. Bounds Validation:**
```c
// Validate card_index is within bounds
if (card_index < 0 || card_index >= game->total_cards) {
    char err_details[128];
    snprintf(err_details, sizeof(err_details),
            "Card index out of bounds (0-%d)", game->total_cards - 1);
    send_error_and_count(client, ERR_INVALID_MOVE, err_details);
    return;
}
```

**Full Validation Flow:**
1. Check params not null/empty → `ERR_INVALID_SYNTAX`
2. Parse with strtol, check endptr → `ERR_INVALID_SYNTAX`
3. Check bounds (0 to total_cards-1) → `ERR_INVALID_MOVE`
4. Check game state, turn, card state → existing error codes
5. All errors count toward auto-disconnect

### 2. Client Side - Input Validation

#### LoginController - Connection Input Validation
**Location:** `client_src/.../controller/LoginController.java`

**Nickname Validation:**
```java
// Check nickname is not empty
if (nickname.isEmpty()) {
    log("ERROR: Nickname is required");
    return;
}

// Validate length
if (nickname.length() > ProtocolConstants.MAX_NICK_LENGTH) {
    log("ERROR: Nickname too long (max " + ProtocolConstants.MAX_NICK_LENGTH + " characters)");
    return;
}

// Check for spaces
if (nickname.contains(" ")) {
    log("ERROR: Nickname cannot contain spaces");
    return;
}

// Check for invalid characters (only alphanumeric, underscore, hyphen allowed)
if (!nickname.matches("[a-zA-Z0-9_-]+")) {
    log("ERROR: Nickname can only contain letters, numbers, underscores, and hyphens");
    return;
}
```

**IP Address Validation:**
```java
// Basic IPv4 format validation
if (!ip.matches("^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$") && !ip.equals("localhost")) {
    log("ERROR: Invalid IP address format (use IPv4 format: xxx.xxx.xxx.xxx or 'localhost')");
    return;
}
```

**Port Validation:**
```java
int port;
try {
    port = Integer.parseInt(portStr);
    if (port < 1 || port > 65535) {
        log("ERROR: Port must be between 1 and 65535");
        return;
    }
} catch (NumberFormatException e) {
    log("ERROR: Invalid port number");
    return;
}
```

**Prevents:**
- Empty fields
- Nicknames with spaces or special characters
- Nicknames exceeding protocol limit (32 chars)
- Invalid IP format
- Out-of-range port numbers

#### LobbyController - Room Management Validation
**Location:** `client_src/.../controller/LobbyController.java`

**handleCreateRoom() Validation:**
```java
// Validate connection
if (connection == null || !connection.isConnected()) {
    showError("Not connected", "You are not connected to the server.");
    return;
}

// Validate room name length
if (roomName.length() > 64) {
    showError("Invalid room name", "Room name must be 64 characters or less.");
    return;
}

// Check for spaces (protocol uses space as delimiter)
if (roomName.contains(" ")) {
    showError("Invalid room name", "Room name cannot contain spaces.");
    return;
}

// Validate max players range
if (maxPlayers < 2 || maxPlayers > 4) {
    showError("Invalid player count", "Max players must be between 2 and 4.");
    return;
}

// Send with error checking
boolean sent = connection.sendMessage(String.format("CREATE_ROOM %s %d", roomName, maxPlayers));
if (sent) {
    updateStatus("Creating room...");
} else {
    showError("Communication error", "Failed to send command to server.");
}
```

**handleJoinRoom() Validation:**
```java
// Validate connection
if (connection == null || !connection.isConnected()) {
    showError("Not connected", "You are not connected to the server.");
    return;
}

// Validate room selection
Room selectedRoom = roomListView.getSelectionModel().getSelectedItem();
if (selectedRoom == null) {
    showError("No room selected", "Please select a room to join.");
    return;
}

// Validate room state
if ("PLAYING".equals(selectedRoom.getState())) {
    showError("Room unavailable", "Cannot join a room that is already playing.");
    return;
}

// Validate room not full
if (selectedRoom.getCurrentPlayers() >= selectedRoom.getMaxPlayers()) {
    showError("Room full", "This room is already full.");
    return;
}

// Send with error checking
boolean sent = connection.sendMessage("JOIN_ROOM " + selectedRoom.getId());
if (sent) {
    updateStatus("Joining room...");
} else {
    showError("Communication error", "Failed to send command to server.");
}
```

**Prevents:**
- Creating rooms when disconnected
- Room names with spaces (breaks protocol parsing)
- Room names exceeding 64 characters
- Invalid player counts (< 2 or > 4)
- Joining without selecting a room
- Joining rooms that are already playing
- Joining full rooms

#### GameController - Game Action Validation
**Location:** `client_src/.../controller/GameController.java`

**handleCardClick() Validation:**
```java
private void handleCardClick(int index) {
    // Validate connection
    if (connection == null || !connection.isConnected()) {
        showAlert("Not connected to server!");
        return;
    }

    // Validate game has started
    if (!gameStarted) {
        showAlert("Game hasn't started yet!");
        return;
    }

    // Validate index bounds
    if (cardButtons == null || index < 0 || index >= cardButtons.length) {
        showAlert("Invalid card index!");
        return;
    }

    // Validate it's player's turn
    if (!myTurn) {
        showAlert("It's not your turn!");
        return;
    }

    // Validate card state
    if (cardMatched[index]) {
        showAlert("Card already matched!");
        return;
    }

    if (flippedThisTurn >= 2) {
        showAlert("Already flipped 2 cards this turn!");
        return;
    }

    if (flippedIndices.contains(index)) {
        showAlert("Card already flipped this turn!");
        return;
    }

    // Send with error checking
    boolean sent = connection.sendMessage("FLIP " + index);
    if (!sent) {
        showAlert("Failed to send command to server!");
    }
}
```

**handleStartGame() Validation:**
```java
// Validate connection
if (connection == null || !connection.isConnected()) {
    showAlert("Not connected to server!");
    return;
}

// Validate is owner
if (!isOwner) {
    showAlert("Only the room owner can start the game!");
    return;
}

// Validate game not started
if (gameStarted) {
    showAlert("Game has already started!");
    return;
}

// Validate enough players
if (players.size() < 2) {
    showAlert("Need at least 2 players to start!");
    return;
}

// Send with error checking
boolean sent = connection.sendMessage("START_GAME");
if (sent) {
    updateStatus("Creating game...");
    startGameButton.setDisable(true);
} else {
    showAlert("Failed to send command to server!");
}
```

**handleReady() Validation:**
```java
// Validate connection
if (connection == null || !connection.isConnected()) {
    showAlert("Not connected to server!");
    return;
}

// Validate not already ready
if (isReady) {
    showAlert("You are already ready!");
    return;
}

// Validate game not started
if (gameStarted) {
    showAlert("Game has already started!");
    return;
}

// Send with error checking
boolean sent = connection.sendMessage("READY");
if (sent) {
    readyButton.setDisable(true);
    isReady = true;
    updateStatus("Ready! Waiting for others...");
} else {
    showAlert("Failed to send command to server!");
}
```

**handleLeaveRoom() Validation:**
```java
// Validate connection
if (connection == null || !connection.isConnected()) {
    showAlert("Not connected to server!");
    return;
}

// Existing confirmation dialog...
if (result.isPresent() && result.get() == ButtonType.OK) {
    boolean sent = connection.sendMessage("LEAVE_ROOM");
    if (!sent) {
        showAlert("Failed to send command to server!");
    }
}
```

**Prevents:**
- Card clicks when disconnected
- Card clicks when game not started
- Out-of-bounds card indices
- Clicks when not player's turn
- Clicking already matched cards
- Flipping more than 2 cards per turn
- Re-flipping same card in one turn
- Starting game when not owner
- Starting game with < 2 players
- Marking ready when already ready
- Sending commands over broken connection

## Error Message Format

### Server Error Messages

**Format:** `ERROR <error_code> [details]`

**Examples:**
```
ERROR INVALID_COMMAND UNKNOWN_CMD
ERROR INVALID_SYNTAX Card index required
ERROR INVALID_SYNTAX Card index must be a number
ERROR INVALID_MOVE Card index out of bounds (0-15)
ERROR NOT_YOUR_TURN Wait for your turn
ERROR INVALID_CARD Card already matched
ERROR ROOM_FULL Room is full
ERROR NICK_IN_USE Nickname already taken
```

### Client Validation Messages

**Connection Errors:**
```
ERROR: IP address is required
ERROR: Invalid IP address format (use IPv4 format: xxx.xxx.xxx.xxx or 'localhost')
ERROR: Port must be between 1 and 65535
ERROR: Nickname cannot contain spaces
ERROR: Nickname can only contain letters, numbers, underscores, and hyphens
```

**Game Errors:**
```
Not connected to server!
Game hasn't started yet!
It's not your turn!
Card already matched!
Invalid card index!
Failed to send command to server!
```

**Room Errors:**
```
Room name cannot contain spaces
Room name must be 64 characters or less
Max players must be between 2 and 4
Cannot join a room that is already playing
This room is already full
```

## Auto-Disconnect Mechanism

### Server Behavior

**Error Counter:**
- Each client has `invalid_message_count` field in `client_t` struct
- Incremented by `send_error_and_count()` on every error
- Counter persists across multiple errors
- Threshold: `MAX_ERROR_COUNT = 3`

**Disconnect Process:**
1. Client sends invalid command (unknown, bad syntax, invalid move)
2. Server calls `send_error_and_count()`
3. Error message sent: `ERROR <code> <details>`
4. Counter incremented: `client->invalid_message_count++`
5. Log entry: `"Client X: Error #N - ..."`
6. If `invalid_message_count >= MAX_ERROR_COUNT`:
   - Log: `"Client X: Disconnecting due to 3 errors"`
   - Call `close(client->socket_fd)`
   - Disconnect handler cleans up client state

**Example Log:**
```
[2025-01-12 14:30:15] Client 5 (Alice): Error #1 - ERROR INVALID_SYNTAX Card index required
[2025-01-12 14:30:16] Client 5 (Alice): Error #2 - ERROR INVALID_MOVE Card index out of bounds (0-15)
[2025-01-12 14:30:17] Client 5 (Alice): Error #3 - ERROR NOT_YOUR_TURN Wait for your turn
[2025-01-12 14:30:17] Client 5: Disconnecting due to 3 errors
[2025-01-12 14:30:17] Client 5 disconnected
```

### Client Prevention

The client prevents most invalid commands from being sent, reducing error counter hits:
- Input validation before network transmission
- UI state management (disable buttons when invalid)
- Connection checks before sending
- Clear user feedback on validation failures

This creates a **defense-in-depth** approach:
1. **Client validation:** Prevents most errors at UI level
2. **Server validation:** Catches malformed/malicious input
3. **Error counter:** Disconnects clients sending repeated invalid data

## Testing Scenarios

### Server Validation Tests

**1. Unknown Command:**
```bash
echo "UNKNOWN_CMD" | nc localhost 10000
# Expected: ERROR INVALID_COMMAND UNKNOWN_CMD
```

**2. Invalid Syntax (FLIP without parameter):**
```bash
echo "HELLO TestUser" | nc localhost 10000
# Wait for WELCOME...
echo "FLIP" | nc localhost 10000
# Expected: ERROR INVALID_SYNTAX Card index required
```

**3. Non-Numeric Card Index:**
```bash
echo "FLIP abc" | nc localhost 10000
# Expected: ERROR INVALID_SYNTAX Card index must be a number
```

**4. Out-of-Bounds Card Index:**
```bash
echo "FLIP 999" | nc localhost 10000
# Expected: ERROR INVALID_MOVE Card index out of bounds (0-15)
```

**5. Error Counter (Auto-Disconnect):**
```bash
# Send 3 invalid commands in succession
echo -e "HELLO TestUser\nBAD1\nBAD2\nBAD3" | nc localhost 10000
# Expected:
# - WELCOME
# - ERROR INVALID_COMMAND BAD1
# - ERROR INVALID_COMMAND BAD2
# - ERROR INVALID_COMMAND BAD3
# - [Connection closed by server]
```

**6. Random Data (from assignment):**
```bash
cat /dev/urandom | nc 127.0.0.1 10000
# Expected: Multiple ERR_INVALID_SYNTAX, then auto-disconnect
```

### Client Validation Tests

**1. Empty Nickname:**
- Clear nickname field
- Click CONNECT
- Expected: "ERROR: Nickname is required"

**2. Invalid Nickname Characters:**
- Enter nickname: "Test User" (with space)
- Click CONNECT
- Expected: "ERROR: Nickname cannot contain spaces"
- Enter nickname: "Test@User" (with special char)
- Click CONNECT
- Expected: "ERROR: Nickname can only contain letters, numbers, underscores, and hyphens"

**3. Invalid IP Address:**
- Enter IP: "999.999.999.999"
- Click CONNECT
- Expected: "ERROR: Invalid IP address format..."

**4. Invalid Port:**
- Enter port: "99999"
- Click CONNECT
- Expected: "ERROR: Port must be between 1 and 65535"

**5. Room Name with Spaces:**
- Create room with name: "My Room"
- Expected: "Invalid room name: Room name cannot contain spaces"

**6. Join Full Room:**
- Try to join room showing (4/4) players
- Expected: "Room full: This room is already full"

**7. Card Click When Not Turn:**
- Wait for other player's turn
- Click any card
- Expected: "It's not your turn!"

**8. Start Game Without Enough Players:**
- Create room (as owner)
- Click START GAME immediately (only 1 player)
- Expected: "Need at least 2 players to start!"

## Error Code Reference

| Code | Meaning | When Triggered | Counts Toward Disconnect |
|------|---------|----------------|-------------------------|
| `INVALID_COMMAND` | Unknown command | Any unrecognized command | ✅ Yes |
| `INVALID_SYNTAX` | Malformed command | Missing params, non-numeric where number expected | ✅ Yes |
| `INVALID_PARAMS` | Invalid parameter values | Out of range, invalid format | ✅ Yes |
| `INVALID_MOVE` | Illegal game move | Card out of bounds, already matched | ✅ Yes |
| `NOT_AUTHENTICATED` | Command requires auth | Commands before HELLO | ✅ Yes |
| `ALREADY_AUTHENTICATED` | Duplicate auth | HELLO sent twice | ✅ Yes |
| `NICK_IN_USE` | Nickname conflict | Another client using same nick | ❌ No |
| `ROOM_NOT_FOUND` | Invalid room ID | JOIN_ROOM with non-existent ID | ❌ No |
| `ROOM_FULL` | Room at capacity | JOIN_ROOM when max players reached | ❌ No |
| `NOT_IN_ROOM` | Command requires room | READY, START_GAME when not in room | ✅ Yes |
| `NOT_ROOM_OWNER` | Insufficient permissions | START_GAME by non-owner | ❌ No |
| `GAME_NOT_STARTED` | Game not in progress | FLIP before GAME_START | ❌ No |
| `NOT_YOUR_TURN` | Turn violation | FLIP when not active player | ❌ No |
| `INVALID_CARD` | Card state error | Flipping matched card | ✅ Yes |

**Note:** The "Counts Toward Disconnect" column indicates which errors increment the error counter. Generally, syntax/protocol violations count, while game state violations don't (since they might be race conditions or network delays).

## Known Limitations

1. **Strtol Validation:**
   - Accepts leading whitespace (by design)
   - Doesn't validate trailing garbage after first space (protocol splits by space anyway)
   - Could use stricter parsing if needed

2. **Client IP Validation:**
   - Only checks IPv4 format (xxx.xxx.xxx.xxx)
   - Doesn't validate octet ranges (0-255)
   - Accepts "localhost" as special case
   - Doesn't support IPv6

3. **Error Counter Persistence:**
   - Counter resets on reconnect (new client_t struct)
   - Could track by IP address if persistent tracking needed
   - Currently per-connection, not per-user

4. **Client-Side Validation:**
   - Checks are best-effort, not cryptographically secure
   - Assumes client is not malicious
   - Server always validates regardless of client checks

## Summary

Packet 9 implements multi-layer validation:

**Server Layer:**
- Validates all commands and parameters
- Uses `strtol()` for robust numeric parsing
- Tracks error count per client
- Auto-disconnects after 3 invalid messages
- Comprehensive error codes for all failure modes

**Client Layer:**
- Validates all user inputs before sending
- Checks connection state before commands
- Validates game state before actions
- Provides clear user feedback
- Prevents most invalid operations at UI level

**Benefits:**
- Prevents accidental invalid commands (client validation)
- Protects against malicious clients (server validation)
- Reduces server load from invalid data
- Improves user experience with immediate feedback
- Enhances debugging with detailed error messages
- Meets assignment requirements for invalid message handling

**Compliance:**
- ✅ Unknown commands → ERROR INVALID
- ✅ Invalid format → ERROR SYNTAX
- ✅ Out-of-bounds moves → ERROR INVALID_MOVE
- ✅ Client GUI validation (no null/invalid values)
- ✅ Error counter with auto-disconnect
