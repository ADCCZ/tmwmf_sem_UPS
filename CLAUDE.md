# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a KIV/UPS semester project implementing a networked multiplayer Memory (Pexeso) card game with client-server architecture (1:N).

**Game:** Memory/Pexeso for 2-4 players
**Architecture:** TCP-based client-server
**Server:** C (using BSD sockets, no external networking libraries)
**Client:** Java with JavaFX GUI (no external networking libraries beyond standard library)
**Protocol:** Custom text-based protocol (detailed specification included in repository)

## Critical Constraints

### Networking Rules
- **NO external networking or serialization libraries allowed**
- Server MUST use only BSD sockets (C)
- Client MUST use only Java standard library (Socket, BufferedReader, etc.)
- C++2y networking interface is forbidden
- Protocol is text-based over TCP, no encryption

### Build Requirements
- Both applications MUST be built with standard build tools (make for C, Maven/Ant for Java)
- NO manual compilation with gcc/javac
- NO IDE-specific build processes
- NO bash scripts for building

### Code Quality Requirements
- Applications must be modular (C modules, Java classes)
- Must handle segfaults, exceptions, deadlocks gracefully
- Must validate all network input and reject invalid messages
- Must implement logging for connections, disconnections, errors, game events
- Must run indefinitely without restarts (handle multiple game sessions)

## Protocol Implementation

The complete protocol specification is in the repository root. Key points:

### Message Format
- Text-based, newline-terminated (`\n`)
- ASCII only (no diacritics)
- Format: `COMMAND [PARAM1] [PARAM2] ...\n`
- Parameters separated by single space

### Connection Recovery
- **Short disconnect (< 60s):** Game pauses, client auto-reconnects, server sends GAME_STATE
- **Long disconnect (> 60s):** Player removed, game ends or continues with remaining players
- Keepalive: Server sends PING every 15s, expects PONG within 5s

### State Machine
Client states: DISCONNECTED → CONNECTED → AUTHENTICATED → IN_LOBBY → IN_ROOM → IN_GAME
Server tracks per-client state and handles reconnection via `RECONNECT` command

## Project Structure

```
server_src/     # C server implementation
client_src/     # Java client implementation
```

### Expected Server Architecture (C)
- Main module: server initialization, configuration parsing (IP, port, room limit, player limit)
- Network module: BSD socket handling, select/epoll for multiplexing
- Protocol module: message parsing, validation, serialization
- Game logic: Pexeso rules, card matching, scoring
- Room management: lobby, room creation/joining, player management
- Connection management: disconnect detection, reconnection handling
- Logging module: timestamped logs for all events

### Expected Client Architecture (Java)
- Main class: application entry point
- Network layer: Socket connection, separate thread for receiving messages
- Protocol layer: message parsing and building
- JavaFX UI: non-blocking, shows connection state, lobby, room list, game board
- Game state: tracks current game, player scores, revealed cards
- Reconnection logic: automatic retry on disconnect

## Implementation Phases

Based on assignment requirements:

1. **Week 5:** Protocol specification with state diagrams (COMPLETED - see protocol spec)
2. **Week 9:** Server skeleton - connection handling, basic message exchange (LIST_ROOMS, CREATE_ROOM, JOIN_ROOM)
3. **Week 12:** Invalid message handling, disconnect/reconnect logic

## Building and Running

### Server (Linux/WSL)
- Build: `make` in server_src/
- Run: `./server [IP] [PORT] [MAX_ROOMS] [MAX_PLAYERS]`
- Example: `./server 0.0.0.0 10000 10 50`

### Client (Windows/Linux)
- Build: `mvn clean package` in client_src/
- Run: `java -jar target/pexeso-client-1.0-SNAPSHOT.jar`

**Windows users:**
- Use `build-client.bat` to build
- Use `run-client.bat` to run
- See `client_src/README-WINDOWS.md` for detailed instructions
- See `QUICK-START-WINDOWS.md` for quick setup guide

## Testing Requirements

### Must Test
- Complete game with 2 players without errors
- Network outage simulation (iptables DROP/REJECT rules)
- Invalid message handling (random data from /dev/urandom, malformed commands, out-of-range values)
- State violation (commands in wrong state, turns when not player's turn)
- Multiple concurrent game rooms
- Memory leaks (server with valgrind -g)

### Testing Commands (from assignment)
```bash
# Invalid data test
cat /dev/urandom | nc 127.0.0.1 10000

# Simulate network drop
iptables -A INPUT -p tcp --dport 10000 -j DROP
iptables -D INPUT -p tcp --dport 10000 -j DROP  # remove

# Simulate network reject
iptables -A INPUT -p tcp --dport 10000 -j REJECT
iptables -D INPUT -p tcp --dport 10000 -j REJECT  # remove

# Memory leak check (compile with -g)
valgrind ./server
```

## Critical Implementation Notes

### Server Parallelism
- Multiple game rooms must run in parallel without interfering
- Synchronization required for shared data structures
- Recommended: select() or epoll() with single thread, or thread pool

### Invalid Message Handling
- Implement counter: disconnect after 3 invalid messages (configurable)
- Log all invalid messages with client ID and content
- Types to handle:
  - Unknown commands
  - Wrong parameter count/types
  - Values out of bounds
  - Commands in wrong state
  - Game rule violations

### Client UI Requirements
- Must be JavaFX (NOT Swing, NOT console)
- Must be non-blocking (network operations on separate thread)
- Must show: current game state, player list, scores, whose turn, connection status
- Must display disconnect warnings (server unreachable, opponent disconnected)

### Game Flow
1. Player connects → lobby (sees room list)
2. Player creates or joins room (max 4 players, min 2 to start)
3. Room creator starts game → all receive GAME_START
4. Turn-based: flip 2 cards → MATCH (go again) or MISMATCH (next player)
5. Game ends when all pairs found → GAME_END → back to lobby

### Card Reveal Logic
- First FLIP → server broadcasts CARD_REVEAL
- Second FLIP → server broadcasts CARD_REVEAL + MATCH/MISMATCH
- On MATCH: same player stays on turn
- On MISMATCH: cards flip back, next player's turn

## Deployment Environment

Must test on:
- GNU/Linux (server and client)
- Windows (client only)
- Lab: UC-326

Server must accept IP/port configuration (not hardcoded).

## Documentation Requirements

Final documentation must include:
- Game variant description (Pexeso rules as implemented)
- Complete protocol specification (already done)
- Implementation description: modules/classes, layering, parallelization method
- Build requirements: Java version, GCC version, dependencies
- Build instructions
- Results evaluation

## Key Protocol Messages Reference

| Direction | Command | Purpose |
|-----------|---------|---------|
| C→S | HELLO \<nick\> | Initial authentication |
| C→S | LIST_ROOMS | Request room list |
| C→S | CREATE_ROOM \<name\> \<max\> \<size\> | Create game room |
| C→S | JOIN_ROOM \<id\> | Join existing room |
| C→S | START_GAME | Begin game (room creator only) |
| C→S | FLIP \<card_id\> | Flip card (during turn) |
| C→S | PONG | Keepalive response |
| S→C | WELCOME \<client_id\> | Auth success |
| S→C | ROOM_LIST ... | List of rooms |
| S→C | GAME_START ... | Game begins (broadcast) |
| S→C | YOUR_TURN | Client's turn |
| S→C | CARD_REVEAL \<id\> \<val\> \<nick\> | Card flipped (broadcast) |
| S→C | MATCH \<nick\> \<score\> | Pair found (broadcast) |
| S→C | MISMATCH | No match (broadcast) |
| S→C | GAME_END ... | Game over (broadcast) |
| S→C | PING | Keepalive check |
| S→C | PLAYER_DISCONNECTED \<nick\> SHORT/LONG | Disconnect notification |
| S→C | ERROR \<code\> \<msg\> | Error response |

See full protocol specification document for complete details, error codes, and state transitions.
