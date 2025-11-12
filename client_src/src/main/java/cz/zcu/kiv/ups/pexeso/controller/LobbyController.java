package cz.zcu.kiv.ups.pexeso.controller;

import cz.zcu.kiv.ups.pexeso.model.Room;
import cz.zcu.kiv.ups.pexeso.network.ClientConnection;
import cz.zcu.kiv.ups.pexeso.network.MessageListener;
import javafx.application.Platform;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.stage.Stage;

import java.io.IOException;
import java.util.Optional;

import static cz.zcu.kiv.ups.pexeso.protocol.ProtocolConstants.*;

/**
 * Controller for the lobby view
 */
public class LobbyController implements MessageListener {

    @FXML
    private ListView<Room> roomListView;

    @FXML
    private Button refreshButton;

    @FXML
    private Button createRoomButton;

    @FXML
    private Button joinRoomButton;

    @FXML
    private Label statusLabel;

    @FXML
    private Label nicknameLabel;

    private ClientConnection connection;
    private String nickname;
    private ObservableList<Room> rooms;
    private Stage stage;

    public void initialize() {
        rooms = FXCollections.observableArrayList();
        roomListView.setItems(rooms);

        // Enable/disable join button based on selection
        joinRoomButton.setDisable(true);
        roomListView.getSelectionModel().selectedItemProperty().addListener(
                (observable, oldValue, newValue) -> joinRoomButton.setDisable(newValue == null)
        );
    }

    public void setConnection(ClientConnection connection, String nickname) {
        this.connection = connection;
        this.nickname = nickname;
        this.connection.setMessageListener(this);

        nicknameLabel.setText("Logged in as: " + nickname);

        // Request room list
        refreshRooms();
    }

    public void setStage(Stage stage) {
        this.stage = stage;
    }

    @FXML
    private void handleRefresh() {
        refreshRooms();
    }

    @FXML
    private void handleCreateRoom() {
        // Validate connection
        if (connection == null || !connection.isConnected()) {
            showError("Not connected", "You are not connected to the server.");
            return;
        }

        TextInputDialog nameDialog = new TextInputDialog("MyRoom");
        nameDialog.setTitle("Create Room");
        nameDialog.setHeaderText("Create a new game room");
        nameDialog.setContentText("Room name:");

        Optional<String> nameResult = nameDialog.showAndWait();
        if (!nameResult.isPresent() || nameResult.get().trim().isEmpty()) {
            return;
        }

        String roomName = nameResult.get().trim();

        // Validate room name
        if (roomName.length() > 64) {
            showError("Invalid room name", "Room name must be 64 characters or less.");
            return;
        }

        // Check for invalid characters (spaces)
        if (roomName.contains(" ")) {
            showError("Invalid room name", "Room name cannot contain spaces.");
            return;
        }

        // Ask for max players
        ChoiceDialog<Integer> playersDialog = new ChoiceDialog<>(2, 2, 3, 4);
        playersDialog.setTitle("Create Room");
        playersDialog.setHeaderText("Select maximum number of players");
        playersDialog.setContentText("Max players:");

        Optional<Integer> playersResult = playersDialog.showAndWait();
        if (!playersResult.isPresent()) {
            return;
        }

        int maxPlayers = playersResult.get();

        // Validate max players
        if (maxPlayers < 2 || maxPlayers > 4) {
            showError("Invalid player count", "Max players must be between 2 and 4.");
            return;
        }

        // Send CREATE_ROOM command
        boolean sent = connection.sendMessage(String.format("CREATE_ROOM %s %d", roomName, maxPlayers));
        if (sent) {
            updateStatus("Creating room...");
        } else {
            showError("Communication error", "Failed to send command to server.");
        }
    }

    @FXML
    private void handleJoinRoom() {
        // Validate connection
        if (connection == null || !connection.isConnected()) {
            showError("Not connected", "You are not connected to the server.");
            return;
        }

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
    }

    private void refreshRooms() {
        connection.sendMessage("LIST_ROOMS");
        updateStatus("Refreshing room list...");
    }

    private void updateStatus(String message) {
        Platform.runLater(() -> statusLabel.setText(message));
    }

    private void showError(String title, String message) {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.ERROR);
            alert.setTitle(title);
            alert.setHeaderText(null);
            alert.setContentText(message);
            alert.showAndWait();
        });
    }

    @Override
    public void onMessageReceived(String message) {
        String[] parts = message.split(" ", 2);
        String command = parts[0];

        switch (command) {
            case "ROOM_LIST":
                handleRoomList(message);
                break;
            case "ROOM_CREATED":
                handleRoomCreated(message);
                break;
            case "ROOM_JOINED":
                handleRoomJoined(message);
                break;
            case "ERROR":
                handleError(message);
                break;
            default:
                // Log unknown message
                System.out.println("Lobby received: " + message);
                break;
        }
    }

    private void handleRoomList(String message) {
        // Format: ROOM_LIST <count> [<id> <name> <current> <max> <state>]*
        String[] parts = message.split(" ");
        if (parts.length < 2) return;

        int count = Integer.parseInt(parts[1]);

        Platform.runLater(() -> {
            rooms.clear();

            if (count == 0) {
                updateStatus("No rooms available. Create one!");
                return;
            }

            int index = 2;
            int activeRooms = 0;
            for (int i = 0; i < count; i++) {
                if (index + 4 >= parts.length) break;

                try {
                    int id = Integer.parseInt(parts[index++]);
                    String name = parts[index++];
                    int current = Integer.parseInt(parts[index++]);
                    int max = Integer.parseInt(parts[index++]);
                    String state = parts[index++];

                    // Skip finished games - they should not appear in lobby
                    if (!"FINISHED".equals(state)) {
                        rooms.add(new Room(id, name, current, max, state));
                        activeRooms++;
                    }
                } catch (Exception e) {
                    System.err.println("Error parsing room: " + e.getMessage());
                }
            }

            if (activeRooms == 0) {
                updateStatus("No active rooms available. Create one!");
            } else {
                updateStatus(String.format("Found %d active room(s)", activeRooms));
            }
        });
    }

    private void handleRoomCreated(String message) {
        // Format: ROOM_CREATED <id> <name>
        String[] parts = message.split(" ");
        if (parts.length < 3) return;

        int roomId = Integer.parseInt(parts[1]);
        String roomName = parts[2];

        Platform.runLater(() -> {
            updateStatus("Room created: " + roomName);
            // Switch to game view
            switchToGameView(roomId, roomName, true);
        });
    }

    private void handleRoomJoined(String message) {
        // Format: ROOM_JOINED <id> <name>
        String[] parts = message.split(" ");
        if (parts.length < 3) return;

        int roomId = Integer.parseInt(parts[1]);
        String roomName = parts[2];

        Platform.runLater(() -> {
            updateStatus("Joined room: " + roomName);
            // Switch to game view
            switchToGameView(roomId, roomName, false);
        });
    }

    private void handleError(String message) {
        Platform.runLater(() -> {
            updateStatus("Error: " + message);
            showError("Server Error", message);
        });
    }

    private void switchToGameView(int roomId, String roomName, boolean isOwner) {
        try {
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/GameView.fxml"));
            Parent root = loader.load();

            GameController gameController = loader.getController();
            gameController.setConnection(connection, nickname);
            gameController.setRoomInfo(roomId, roomName, isOwner);
            gameController.setStage(stage);

            Scene scene = new Scene(root, 800, 600);
            stage.setScene(scene);
            stage.setTitle("Pexeso - " + roomName);
        } catch (IOException e) {
            e.printStackTrace();
            showError("Error", "Failed to load game view: " + e.getMessage());
        }
    }

    @Override
    public void onConnected() {
        // Already connected when we get to lobby
    }

    @Override
    public void onDisconnected(String reason) {
        Platform.runLater(() -> {
            showError("Disconnected", "Lost connection to server: " + reason);
            updateStatus("Disconnected");
        });
    }

    @Override
    public void onError(String error) {
        Platform.runLater(() -> {
            showError("Error", error);
            updateStatus("Error: " + error);
        });
    }
}
