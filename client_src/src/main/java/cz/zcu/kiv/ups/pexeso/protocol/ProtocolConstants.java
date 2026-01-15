package cz.zcu.kiv.ups.pexeso.protocol;

/**
 * Protocol constants for Pexeso game client-server communication
 */
public class ProtocolConstants {

    // Client to Server commands
    public static final String CMD_HELLO = "HELLO";
    public static final String CMD_LIST_ROOMS = "LIST_ROOMS";
    public static final String CMD_CREATE_ROOM = "CREATE_ROOM";
    public static final String CMD_JOIN_ROOM = "JOIN_ROOM";
    public static final String CMD_LEAVE_ROOM = "LEAVE_ROOM";
    public static final String CMD_START_GAME = "START_GAME";
    public static final String CMD_READY = "READY";
    public static final String CMD_FLIP = "FLIP";
    public static final String CMD_PONG = "PONG";
    public static final String CMD_RECONNECT = "RECONNECT";

    // Server to Client commands
    public static final String CMD_WELCOME = "WELCOME";
    public static final String CMD_ROOM_LIST = "ROOM_LIST";
    public static final String CMD_ROOM_CREATED = "ROOM_CREATED";
    public static final String CMD_ROOM_JOINED = "ROOM_JOINED";
    public static final String CMD_PLAYER_JOINED = "PLAYER_JOINED";
    public static final String CMD_PLAYER_LEFT = "PLAYER_LEFT";
    public static final String CMD_PLAYER_READY = "PLAYER_READY";
    public static final String CMD_PLAYER_RECONNECTED = "PLAYER_RECONNECTED";
    public static final String CMD_ROOM_OWNER_CHANGED = "ROOM_OWNER_CHANGED";
    public static final String CMD_GAME_CREATED = "GAME_CREATED";
    public static final String CMD_GAME_START = "GAME_START";
    public static final String CMD_YOUR_TURN = "YOUR_TURN";
    public static final String CMD_CARD_REVEAL = "CARD_REVEAL";
    public static final String CMD_MATCH = "MATCH";
    public static final String CMD_MISMATCH = "MISMATCH";
    public static final String CMD_GAME_END = "GAME_END";
    public static final String CMD_GAME_END_FORFEIT = "GAME_END_FORFEIT";
    public static final String CMD_GAME_STATE = "GAME_STATE";
    public static final String CMD_LEFT_ROOM = "LEFT_ROOM";
    public static final String CMD_PING = "PING";
    public static final String CMD_PLAYER_DISCONNECTED = "PLAYER_DISCONNECTED";
    public static final String CMD_SERVER_SHUTDOWN = "SERVER_SHUTDOWN";
    public static final String CMD_ERROR = "ERROR";

    // Error codes
    public static final String ERR_INVALID_COMMAND = "INVALID_COMMAND";
    public static final String ERR_INVALID_PARAMS = "INVALID_PARAMS";
    public static final String ERR_NOT_AUTHENTICATED = "NOT_AUTHENTICATED";
    public static final String ERR_ALREADY_AUTHENTICATED = "ALREADY_AUTHENTICATED";
    public static final String ERR_NICK_IN_USE = "NICK_IN_USE";
    public static final String ERR_ROOM_NOT_FOUND = "ROOM_NOT_FOUND";
    public static final String ERR_ROOM_FULL = "ROOM_FULL";
    public static final String ERR_NOT_IN_ROOM = "NOT_IN_ROOM";
    public static final String ERR_NOT_ROOM_OWNER = "NOT_ROOM_OWNER";
    public static final String ERR_GAME_NOT_STARTED = "GAME_NOT_STARTED";
    public static final String ERR_NOT_YOUR_TURN = "NOT_YOUR_TURN";
    public static final String ERR_INVALID_CARD = "INVALID_CARD";
    public static final String ERR_NOT_IMPLEMENTED = "NOT_IMPLEMENTED";

    // Message format
    public static final String MESSAGE_DELIMITER = "\n";
    public static final int MAX_MESSAGE_LENGTH = 1024;
    public static final int MAX_NICK_LENGTH = 32;

    // Timeouts
    public static final int CONNECTION_TIMEOUT_MS = 5000;      // 5 seconds to connect
    public static final int READ_TIMEOUT_MS = 15000;           // 15 seconds read timeout (must be > PONG_WAIT_INTERVAL + safety margin)
    public static final int RECONNECT_INTERVAL_MS = 10000;     // 10 seconds between reconnect attempts
    public static final int MAX_RECONNECT_ATTEMPTS = 7;        // 7 attempts = 70 seconds total (server waits 90s)

    private ProtocolConstants() {
        // Utility class - no instantiation
    }
}
