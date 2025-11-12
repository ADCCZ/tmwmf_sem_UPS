# Packet 7 - JavaFX GUI Implementation

**Author:** Generated implementation
**Date:** 2025-01-12
**Status:** ✅ Completed

## Overview

This packet implements the complete JavaFX graphical user interface for the Pexeso client application. The implementation includes three main screens (Login, Lobby, Game) with full protocol integration and asynchronous message handling.

## Implementation Summary

### 1. Model Classes

#### Room.java
**Location:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/model/Room.java`

Simple data model representing a game room in the lobby:
- Room ID, name, player counts, and state
- Used for displaying rooms in the lobby ListView
- Provides formatted toString() for display

### 2. Controllers

#### LoginController.java (Updated)
**Location:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/controller/LoginController.java`

**Changes:**
- Added scene switching to LobbyView after successful authentication
- Automatic transition on WELCOME message
- Added stage reference for scene management

**Key Methods:**
- `switchToLobby()`: Loads LobbyView and passes connection context

#### LobbyController.java (New)
**Location:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/controller/LobbyController.java`

**Features:**
- Room list display with automatic refresh
- Create room dialog (name, max players, board size)
- Join room functionality
- Real-time room list updates
- Filters out finished games from display
- Scene switching to GameView when joining/creating room

**Key Methods:**
- `handleCreateRoom()`: Shows dialog and sends CREATE_ROOM command
- `handleJoinRoom()`: Validates selection and sends JOIN_ROOM command
- `handleRoomList()`: Parses ROOM_LIST response and updates UI (filters FINISHED rooms)
- `switchToGameView()`: Loads GameView with room context

**Protocol Messages Handled:**
- `ROOM_LIST`: Updates room list display
- `ROOM_CREATED`: Switches to game view as owner
- `ROOM_JOINED`: Switches to game view as player
- `ERROR`: Displays error dialog

#### GameController.java (New)
**Location:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/controller/GameController.java`

**Features:**
- Dynamic card grid generation (4x4, 5x5, or 6x6)
- Player list with real-time score updates
- Turn indicator with visual feedback
- Card flip animations with 1-second reveal on mismatch
- Match/mismatch handling with color coding
- Game end dialog with winner announcement
- Return to lobby functionality

**Key Methods:**
- `initializeBoard(int size)`: Creates card grid dynamically
- `handleCardClick(int index)`: Validates and sends FLIP command
- `revealCard()`: Shows card value with yellow highlight
- `handleMatch()`: Marks cards as matched (green) and updates scores
- `handleMismatch()`: Hides cards after 1-second delay, clears turn label
- `handleGameStart()`: Initializes game board and player list
- `handleGameEnd()`: Shows results dialog and offers return to lobby

**Visual Feedback:**
- YOUR TURN: Green, bold, 18px
- Turn: OtherPlayer: Black, normal, 16px
- Waiting for other player: Gray, normal, 16px
- Hidden cards: "?" with default style
- Revealed cards: Yellow background (#FFC107)
- Matched cards: Green background (#4CAF50), disabled

**Protocol Messages Handled:**
- `GAME_CREATED`: Shows READY button, hides START GAME button
- `PLAYER_READY`: Updates status message
- `GAME_START`: Initializes board and player list
- `YOUR_TURN`: Updates turn label to "YOUR TURN!" (green)
- `CARD_REVEAL`: Shows card value with animation
- `MATCH`: Marks cards as matched, updates score
- `MISMATCH`: Hides cards after delay, clears turn label to "Waiting..."
- `GAME_END`: Shows winner dialog, offers return to lobby
- `PLAYER_JOINED`: Updates status
- `PLAYER_LEFT`: Updates status
- `ERROR`: Shows error dialog

### 3. FXML Views

#### LobbyView.fxml (New)
**Location:** `client_src/src/main/resources/cz/zcu/kiv/ups/pexeso/ui/LobbyView.fxml`

**Layout:**
- Top: Title label and status label
- Center: Room list (ListView) with auto-refresh
- Right: Control buttons (Create, Join, Refresh)
- Bottom: Logout button

#### GameView.fxml (New)
**Location:** `client_src/src/main/resources/cz/zcu/kiv/ups/pexeso/ui/GameView.fxml`

**Layout:**
- Top: Room name, status label, turn indicator
- Left: Player list with scores
- Center: Card grid (GridPane, dynamically populated)
- Bottom: Control buttons (START GAME for owner, READY for players, LEAVE ROOM)

### 4. Bug Fixes

#### Bug #1: Turn Label Not Clearing
**Problem:** "YOUR TURN!" label remained visible after turn changed
**Fix:** In `handleMismatch()`, immediately set:
```java
myTurn = false;
turnLabel.setText("Waiting for other player...");
turnLabel.setStyle("-fx-font-size: 16px; -fx-font-weight: normal; -fx-text-fill: gray;");
```

#### Bug #2: Finished Games in Lobby
**Server Fix (client_handler.c:450):**
```c
room->state = ROOM_STATE_FINISHED;  // Was ROOM_STATE_WAITING
```

**Client Fix (LobbyController.java):**
```java
// Skip finished games - they should not appear in lobby
if (!"FINISHED".equals(state)) {
    rooms.add(new Room(id, name, current, max, state));
}
```

## Thread Safety

All UI updates use `Platform.runLater()` to ensure JavaFX thread safety:
```java
Platform.runLater(() -> {
    // UI update code here
});
```

Network operations run on separate threads to avoid blocking the UI.

## Scene Switching Architecture

Each controller manages scene transitions:
1. **LoginController** → LobbyController (on WELCOME)
2. **LobbyController** → GameController (on ROOM_CREATED/ROOM_JOINED)
3. **GameController** → LobbyController (on LEAVE_ROOM or game end)

Controllers pass the following context:
- `ClientConnection` instance (shared across scenes)
- Player nickname
- Stage reference for scene switching

Example transition:
```java
FXMLLoader loader = new FXMLLoader(getClass().getResource("..."));
Parent root = loader.load();

GameController controller = loader.getController();
controller.setConnection(connection, nickname);
controller.setRoomInfo(roomId, roomName, isOwner);
controller.setStage(stage);

Scene scene = new Scene(root, 800, 600);
stage.setScene(scene);
```

## Files Modified

### New Files:
- `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/model/Room.java`
- `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/controller/LobbyController.java`
- `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/controller/GameController.java`
- `client_src/src/main/resources/cz/zcu/kiv/ups/pexeso/ui/LobbyView.fxml`
- `client_src/src/main/resources/cz/zcu/kiv/ups/pexeso/ui/GameView.fxml`

### Modified Files:
- `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/controller/LoginController.java`
  - Added `switchToLobby()` method
  - Added automatic scene transition on WELCOME

- `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/network/ClientConnection.java`
  - Added `setMessageListener()` method for scene switching

- `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/protocol/ProtocolConstants.java`
  - Increased READ_TIMEOUT_MS from 30000 to 120000 (2 minutes)

- `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/Main.java`
  - Added stage reference passing to LoginController
  - Changed window to resizable

- `server_src/client_handler.c` (Bug fix)
  - Changed room state to ROOM_STATE_FINISHED after game ends

## Testing

To test the GUI:

1. **Compile and run server:**
```bash
cd server_src
make
./server 127.0.0.1 10000 10 50
```

2. **Compile and run client:**
```bash
cd client_src
mvn clean compile
mvn javafx:run
```

3. **Test scenarios:**
   - Login with different nicknames in multiple client instances
   - Create room and start game
   - Join existing room
   - Play complete game (flip cards, match/mismatch)
   - Test finished game filtering (game should disappear from lobby)
   - Test turn label (should show "YOUR TURN!" or "Waiting...")
   - Leave room during game
   - Disconnect and reconnect (if implemented)

## Known Limitations

1. **Non-turn player clicks:** Players who are not on turn can still click cards and will receive an error popup. This is correct behavior but could be improved by disabling card buttons when not on turn.

2. **Turn tracking:** The client only knows it's their turn when receiving YOUR_TURN message. It doesn't track which player's turn it is otherwise (just shows "Waiting...").

3. **Reconnection:** RECONNECT and GAME_STATE messages are not yet fully implemented in the GUI (Packet 8).

## Next Steps

- **Packet 8:** Implement disconnect/reconnect handling with PING/PONG keepalive
- **Packet 9:** Implement comprehensive error handling for invalid messages
- **Packet 10:** Add logging for debugging and testing

## Summary

Packet 7 successfully implements a complete JavaFX GUI for the Pexeso client with:
- ✅ Three-screen navigation (Login → Lobby → Game)
- ✅ Real-time room list with automatic updates
- ✅ Dynamic card grid generation
- ✅ Turn-based gameplay with visual feedback
- ✅ Score tracking and winner announcement
- ✅ Thread-safe UI updates with Platform.runLater()
- ✅ Finished game filtering
- ✅ Turn label management

The client is now fully playable for complete 2-4 player games!
