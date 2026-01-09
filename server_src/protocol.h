#ifndef PROTOCOL_H
#define PROTOCOL_H

/**
 * Protocol constants and message definitions
 */

// Buffer sizes
#define MAX_MESSAGE_LENGTH 1024
#define MAX_NICK_LENGTH 32
#define MAX_ROOM_NAME_LENGTH 64

// Timeout constants (in seconds)
#define PING_INTERVAL 15           // Send PING every 15 seconds (deprecated, now using PONG_WAIT_INTERVAL)
#define PONG_TIMEOUT 5             // Expect PONG within 5 seconds
#define PONG_WAIT_INTERVAL 5       // Wait 5 seconds after PONG before sending next PING
#define RECONNECT_TIMEOUT 90       // Reconnect timeout: server waits 90s for client (client needs ~70s for 5 attempts)

// Protocol commands (client to server)
#define CMD_HELLO "HELLO"
#define CMD_LIST_ROOMS "LIST_ROOMS"
#define CMD_CREATE_ROOM "CREATE_ROOM"
#define CMD_JOIN_ROOM "JOIN_ROOM"
#define CMD_LEAVE_ROOM "LEAVE_ROOM"
#define CMD_READY "READY"
#define CMD_START_GAME "START_GAME"
#define CMD_FLIP "FLIP"
#define CMD_PONG "PONG"
#define CMD_RECONNECT "RECONNECT"
#define CMD_QUIT "QUIT"

// Protocol commands (server to client)
#define CMD_WELCOME "WELCOME"
#define CMD_ROOM_LIST "ROOM_LIST"
#define CMD_ROOM_CREATED "ROOM_CREATED"
#define CMD_ROOM_JOINED "ROOM_JOINED"
#define CMD_PLAYER_JOINED "PLAYER_JOINED"
#define CMD_PLAYER_LEFT "PLAYER_LEFT"
#define CMD_PLAYER_READY "PLAYER_READY"
#define CMD_GAME_START "GAME_START"
#define CMD_YOUR_TURN "YOUR_TURN"
#define CMD_CARD_REVEAL "CARD_REVEAL"
#define CMD_MATCH "MATCH"
#define CMD_MISMATCH "MISMATCH"
#define CMD_GAME_END "GAME_END"
#define CMD_GAME_STATE "GAME_STATE"
#define CMD_PING "PING"
#define CMD_PLAYER_DISCONNECTED "PLAYER_DISCONNECTED"
#define CMD_SERVER_SHUTDOWN "SERVER_SHUTDOWN"
#define CMD_ERROR "ERROR"

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

// Client states
typedef enum {
    STATE_DISCONNECTED,
    STATE_CONNECTED,
    STATE_AUTHENTICATED,
    STATE_IN_LOBBY,
    STATE_IN_ROOM,
    STATE_IN_GAME
} client_state_t;

#endif /* PROTOCOL_H */
