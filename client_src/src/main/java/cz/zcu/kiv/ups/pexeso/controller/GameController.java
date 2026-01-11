package cz.zcu.kiv.ups.pexeso.controller;

import cz.zcu.kiv.ups.pexeso.network.ClientConnection;
import cz.zcu.kiv.ups.pexeso.network.MessageListener;
import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.geometry.Pos;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.GridPane;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

import java.io.IOException;
import java.util.*;

/**
 * Controller for the game view
 */
public class GameController implements MessageListener {

    @FXML
    private Label roomNameLabel;

    @FXML
    private Label statusLabel;

    @FXML
    private Label turnLabel;

    @FXML
    private VBox playersVBox;

    @FXML
    private GridPane cardGrid;

    @FXML
    private Button startGameButton;

    @FXML
    private Button readyButton;

    @FXML
    private Button leaveRoomButton;

    private ClientConnection connection;
    private String nickname;
    private boolean isOwner;
    private Stage stage;

    // Game state
    private int boardSize = 0;
    private Button[] cardButtons;
    private boolean[] cardMatched;
    private int[] cardValues;
    private boolean myTurn = false;
    private int flippedThisTurn = 0;
    private List<Integer> flippedIndices = new ArrayList<>();

    private Map<String, Integer> playerScores = new LinkedHashMap<>();
    private List<String> players = new ArrayList<>();
    private boolean gameStarted = false;
    private boolean isReady = false;
    private boolean reconnectionAlertShown = false;  // Track if reconnection alert was shown

    public void initialize() {
        // Initially hide game controls
        startGameButton.setVisible(false);
        readyButton.setVisible(false);
        cardGrid.setVisible(false);
    }

    public void setConnection(ClientConnection connection, String nickname) {
        this.connection = connection;
        this.nickname = nickname;
        this.connection.setMessageListener(this);
    }

    public void setRoomInfo(int roomId, String roomName, boolean isOwner) {
        this.isOwner = isOwner;

        roomNameLabel.setText("Room: " + roomName);
        startGameButton.setVisible(isOwner);

        updateStatus("Waiting for players...");
    }

    public void setStage(Stage stage) {
        this.stage = stage;
    }

    @FXML
    private void handleStartGame() {
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

        // Let server validate player count - client's player list
        // is not populated until game actually starts

        // Send with error checking
        boolean sent = connection.sendMessage("START_GAME");
        if (sent) {
            updateStatus("Creating game...");
            startGameButton.setDisable(true);
        } else {
            showAlert("Failed to send command to server!");
        }
    }

    @FXML
    private void handleReady() {
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
    }

    @FXML
    private void handleLeaveRoom() {
        // Validate connection
        if (connection == null || !connection.isConnected()) {
            showAlert("Not connected to server!");
            return;
        }

        Alert confirm = new Alert(Alert.AlertType.CONFIRMATION);
        confirm.setTitle("Leave Room");
        confirm.setHeaderText("Are you sure you want to leave?");
        confirm.setContentText("The game will end if you leave.");

        Optional<ButtonType> result = confirm.showAndWait();
        if (result.isPresent() && result.get() == ButtonType.OK) {
            boolean sent = connection.sendMessage("LEAVE_ROOM");
            if (!sent) {
                showAlert("Failed to send command to server!");
            }
        }
    }

    private void updateStatus(String message) {
        Platform.runLater(() -> statusLabel.setText(message));
    }

    private void updateTurnLabel(String player) {
        Platform.runLater(() -> {
            if (player.equals(nickname)) {
                turnLabel.setText("YOUR TURN!");
                turnLabel.setStyle("-fx-font-size: 18px; -fx-font-weight: bold; -fx-text-fill: green;");
                myTurn = true;
            } else {
                turnLabel.setText("Turn: " + player);
                turnLabel.setStyle("-fx-font-size: 16px; -fx-font-weight: normal; -fx-text-fill: black;");
                myTurn = false;
            }
        });
    }

    private void updatePlayers() {
        Platform.runLater(() -> {
            playersVBox.getChildren().clear();

            Label title = new Label("Players:");
            title.setStyle("-fx-font-weight: bold;");
            playersVBox.getChildren().add(title);

            for (String player : players) {
                Integer score = playerScores.getOrDefault(player, 0);
                Label playerLabel = new Label(String.format("%s: %d", player, score));

                if (player.equals(nickname)) {
                    playerLabel.setStyle("-fx-font-weight: bold; -fx-text-fill: blue;");
                }

                playersVBox.getChildren().add(playerLabel);
            }
        });
    }

    private void initializeBoard(int size) {
        this.boardSize = size;
        int totalCards = size * size;
        cardButtons = new Button[totalCards];
        cardMatched = new boolean[totalCards];
        cardValues = new int[totalCards];
        Arrays.fill(cardMatched, false);
        Arrays.fill(cardValues, -1);

        Platform.runLater(() -> {
            cardGrid.getChildren().clear();
            cardGrid.setVisible(true);
            cardGrid.setHgap(5);
            cardGrid.setVgap(5);
            cardGrid.setAlignment(Pos.CENTER);

            for (int i = 0; i < totalCards; i++) {
                final int index = i;
                Button btn = new Button("?");
                btn.setPrefSize(80, 80);
                btn.setStyle("-fx-font-size: 24px; -fx-font-weight: bold;");
                btn.setOnAction(e -> handleCardClick(index));

                cardButtons[i] = btn;
                cardGrid.add(btn, i % size, i / size);
            }
        });
    }

    private void handleCardClick(int index) {
        // Validate connection
        if (connection == null || !connection.isConnected()) {
            showAlert("Not connected to server!");
            return;
        }

        // Validate game state
        if (!gameStarted) {
            showAlert("Game hasn't started yet!");
            return;
        }

        // Validate index bounds
        if (cardButtons == null || index < 0 || index >= cardButtons.length) {
            showAlert("Invalid card index!");
            return;
        }

        if (!myTurn) {
            showAlert("Not your turn!");
            return;
        }

        if (cardMatched[index]) {
            showAlert("Card already matched!");
            return;
        }

        if (flippedIndices.contains(index)) {
            showAlert("Card already flipped this turn!");
            return;
        }

        if (flippedThisTurn >= 2) {
            showAlert("Already flipped 2 cards!");
            return;
        }

        // Send FLIP command
        boolean sent = connection.sendMessage("FLIP " + index);
        if (!sent) {
            showAlert("Failed to send command to server!");
        }
    }

    private void revealCard(int index, int value, String playerName) {
        Platform.runLater(() -> {
            if (index < 0 || index >= cardButtons.length) return;

            cardValues[index] = value;
            cardButtons[index].setText(String.valueOf(value));
            cardButtons[index].setStyle("-fx-font-size: 24px; -fx-font-weight: bold; " +
                                        "-fx-background-color: #FFC107;");

            flippedIndices.add(index);
            flippedThisTurn++;
        });
    }

    private void handleMatch(String playerName, int score) {
        Platform.runLater(() -> {
            // Mark cards as matched
            for (int index : flippedIndices) {
                cardMatched[index] = true;
                cardButtons[index].setStyle("-fx-font-size: 24px; -fx-font-weight: bold; " +
                                           "-fx-background-color: #4CAF50; -fx-text-fill: white;");
                cardButtons[index].setDisable(true);
            }

            // Update score
            playerScores.put(playerName, score);
            updatePlayers();

            // Reset flip state
            flippedIndices.clear();
            flippedThisTurn = 0;

            updateStatus(playerName + " found a match!");
        });
    }

    private void handleMismatch(String nextPlayer) {
        Platform.runLater(() -> {
            // Update turn information
            if (nextPlayer != null && !nextPlayer.isEmpty()) {
                updateTurnLabel(nextPlayer);
                if (nextPlayer.equals(nickname)) {
                    updateStatus("No match! Your turn now.");
                } else {
                    updateStatus("No match! " + nextPlayer + "'s turn.");
                }
            } else {
                myTurn = false;
                turnLabel.setText("Waiting for other player...");
                turnLabel.setStyle("-fx-font-size: 16px; -fx-font-weight: normal; -fx-text-fill: gray;");
                updateStatus("No match!");
            }

            // Wait a bit, then hide cards
            new Thread(() -> {
                try {
                    Thread.sleep(1000); // Show cards for 1 second
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }

                Platform.runLater(() -> {
                    for (int index : flippedIndices) {
                        if (!cardMatched[index]) {
                            cardButtons[index].setText("?");
                            cardButtons[index].setStyle("-fx-font-size: 24px; -fx-font-weight: bold;");
                        }
                    }

                    flippedIndices.clear();
                    flippedThisTurn = 0;
                });
            }).start();
        });
    }

    private void showAlert(String message) {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.INFORMATION);
            alert.setTitle("Info");
            alert.setHeaderText(null);
            alert.setContentText(message);
            alert.show();
        });
    }

    @Override
    public void onMessageReceived(String message) {
        String[] parts = message.split(" ");
        String command = parts[0];

        // Log all messages except PING (which has its own log)
        if (!command.equals("PING")) {
        }

        switch (command) {
            case "PLAYER_JOINED":
                if (parts.length > 1) {
                    String player = parts[1];
                    updateStatus(player + " joined the room");
                }
                break;

            case "PLAYER_LEFT":
                if (parts.length > 1) {
                    String player = parts[1];
                    updateStatus(player + " left the room");
                }
                break;

            case "ROOM_OWNER_CHANGED":
                // Format: ROOM_OWNER_CHANGED <new_owner_nickname>
                if (parts.length > 1) {
                    String newOwner = parts[1];
                    handleOwnerChanged(newOwner);
                }
                break;

            case "PLAYER_DISCONNECTED":
                // Format: PLAYER_DISCONNECTED <nickname> SHORT|LONG
                if (parts.length >= 3) {
                    String disconnectedPlayer = parts[1];
                    String disconnectType = parts[2];
                    handlePlayerDisconnected(disconnectedPlayer, disconnectType);
                }
                break;

            case "PLAYER_RECONNECTED":
                // Format: PLAYER_RECONNECTED <nickname>
                if (parts.length >= 2) {
                    String reconnectedPlayer = parts[1];
                    updateStatus(reconnectedPlayer + " reconnected!");
                }
                break;

            case "ROOM_JOINED":
                // Received after reconnect - we're back in the room
                if (parts.length >= 3) {
                    int reconnectRoomId = Integer.parseInt(parts[1]);
                    String reconnectRoomName = parts[2];
                    updateStatus("Reconnected to room: " + reconnectRoomName);
                    Platform.runLater(() -> {
                        roomNameLabel.setText("Room: " + reconnectRoomName);
                    });
                }
                break;

            case "GAME_CREATED":
                // Format: GAME_CREATED <board_size> <message>
                if (parts.length > 1) {
                    try {
                        boardSize = Integer.parseInt(parts[1]);
                        Platform.runLater(() -> {
                            readyButton.setVisible(true);
                            startGameButton.setVisible(false);
                        });
                        updateStatus("Game created! Click READY when prepared.");
                    } catch (NumberFormatException e) {
                        e.printStackTrace();
                    }
                }
                break;

            case "PLAYER_READY":
                if (parts.length > 1) {
                    String player = parts[1];
                    updateStatus(player + " is ready");
                }
                break;

            case "GAME_START":
                handleGameStart(message);
                break;

            case "GAME_STATE":
                handleGameState(message);
                break;

            case "YOUR_TURN":
                updateTurnLabel(nickname);
                updateStatus("Your turn! Click on a card.");
                break;

            case "CARD_REVEAL":
                // Format: CARD_REVEAL <index> <value> <player>
                if (parts.length >= 4) {
                    try {
                        int index = Integer.parseInt(parts[1]);
                        int value = Integer.parseInt(parts[2]);
                        String player = parts[3];
                        revealCard(index, value, player);
                    } catch (NumberFormatException e) {
                        e.printStackTrace();
                    }
                }
                break;

            case "MATCH":
                // Format: MATCH <player> <score>
                if (parts.length >= 3) {
                    String player = parts[1];
                    int score = Integer.parseInt(parts[2]);
                    handleMatch(player, score);
                }
                break;

            case "MISMATCH":
                // Format: MISMATCH [next_player]
                String nextPlayer = parts.length >= 2 ? parts[1] : null;
                handleMismatch(nextPlayer);
                break;

            case "GAME_END":
                handleGameEnd(message, false);
                break;

            case "GAME_END_FORFEIT":
                handleGameEnd(message, true);
                break;

            case "LEFT_ROOM":
                returnToLobby();
                break;

            case "PING":
                // Respond to PING with PONG
                connection.sendMessage("PONG");
                break;

            case "ERROR":
                String errorRaw = message.substring(6); // Remove "ERROR "
                String userFriendlyError = formatErrorMessage(errorRaw);
                updateStatus(userFriendlyError);
                Platform.runLater(() -> {
                    // Re-enable Start Game button if game hasn't started yet
                    if (!gameStarted && isOwner) {
                        startGameButton.setDisable(false);
                    }

                    Alert alert = new Alert(Alert.AlertType.ERROR);
                    alert.setTitle("Error");
                    alert.setHeaderText(null);
                    alert.setContentText(userFriendlyError);
                    alert.showAndWait();
                });
                break;
        }
    }

    private void handleGameState(String message) {
        // Format: GAME_STATE <board_size> <current_player_nick> <player1> <score1> <player2> <score2> ... <card_states>
        String[] parts = message.split(" ");
        if (parts.length < 4) return;

        try {
            int size = Integer.parseInt(parts[1]);
            String currentPlayerNick = parts[2];

            // Parse players and scores
            players.clear();
            playerScores.clear();

            int index = 3;
            while (index < parts.length - (size * size)) {
                if (index + 1 >= parts.length) break;

                String playerName = parts[index++];
                int score = Integer.parseInt(parts[index++]);

                players.add(playerName);
                playerScores.put(playerName, score);
            }

            // Initialize board if not already done
            if (boardSize != size) {
                initializeBoard(size);
            }

            // Restore card states
            int cardIndex = index;
            for (int i = 0; i < boardSize * boardSize && cardIndex < parts.length; i++, cardIndex++) {
                int cardValue = Integer.parseInt(parts[cardIndex]);

                if (cardValue > 0) {
                    // Card is matched
                    final int finalIndex = i;
                    final int finalValue = cardValue;

                    cardMatched[i] = true;
                    cardValues[i] = cardValue;

                    Platform.runLater(() -> {
                        cardButtons[finalIndex].setText(String.valueOf(finalValue));
                        cardButtons[finalIndex].setStyle("-fx-font-size: 24px; -fx-font-weight: bold; " +
                                                     "-fx-background-color: #4CAF50; -fx-text-fill: white;");
                        cardButtons[finalIndex].setDisable(true);
                    });
                }
            }

            updatePlayers();
            updateTurnLabel(currentPlayerNick);

            gameStarted = true;
            updateStatus("Game state restored after reconnection");

            Platform.runLater(() -> {
                readyButton.setVisible(false);
                startGameButton.setVisible(false);
            });


        } catch (Exception e) {
            e.printStackTrace();
            updateStatus("Error restoring game state: " + e.getMessage());
        }
    }

    private void handleGameStart(String message) {
        // Format: GAME_START <board_size> <player1> <player2> ...
        String[] parts = message.split(" ");
        if (parts.length < 3) return;

        try {
            int size = Integer.parseInt(parts[1]);

            players.clear();
            playerScores.clear();

            for (int i = 2; i < parts.length; i++) {
                String player = parts[i];
                players.add(player);
                playerScores.put(player, 0);
            }

            initializeBoard(size);
            updatePlayers();

            gameStarted = true;

            // First player in list always starts
            String firstPlayer = players.get(0);
            updateTurnLabel(firstPlayer);

            if (firstPlayer.equals(nickname)) {
                updateStatus("Game started! Your turn - click on a card.");
            } else {
                updateStatus("Game started! Waiting for " + firstPlayer + "'s turn...");
            }

            Platform.runLater(() -> {
                readyButton.setVisible(false);
            });

        } catch (NumberFormatException e) {
            e.printStackTrace();
        }
    }

    private void handleGameEnd(String message, boolean forfeit) {
        // Format: GAME_END <player1> <score1> <player2> <score2> ...
        // or: GAME_END_FORFEIT <player1> <score1> <player2> <score2> ...
        String[] parts = message.split(" ");

        StringBuilder resultMsg = new StringBuilder();

        if (forfeit) {
            resultMsg.append("Game Over - Forfeit!\n\n");
            resultMsg.append("A player disconnected.\n\n");
            resultMsg.append("Final Scores:\n");
        } else {
            resultMsg.append("Game Over!\n\nFinal Scores:\n");
        }

        int maxScore = -1;
        List<String> winners = new ArrayList<>();

        for (int i = 1; i < parts.length; i += 2) {
            if (i + 1 < parts.length) {
                String player = parts[i];
                int score = Integer.parseInt(parts[i + 1]);

                resultMsg.append(String.format("%s: %d\n", player, score));

                if (score > maxScore) {
                    maxScore = score;
                    winners.clear();
                    winners.add(player);
                } else if (score == maxScore) {
                    winners.add(player);
                }
            }
        }

        resultMsg.append("\nWinner(s): ");
        resultMsg.append(String.join(", ", winners));

        String finalMsg = resultMsg.toString();

        Platform.runLater(() -> {
            // Show game results
            Alert alert = new Alert(Alert.AlertType.INFORMATION);
            if (forfeit) {
                alert.setTitle("Game Over - Forfeit");
            } else {
                alert.setTitle("Game Over");
            }
            alert.setHeaderText(null);
            alert.setContentText(finalMsg);
            alert.showAndWait();

            // Automatically return to lobby without asking
            returnToLobby();
        });
    }

    private void handleOwnerChanged(String newOwnerNickname) {
        // Check if this client is the new owner
        boolean amINewOwner = newOwnerNickname.equals(this.nickname);

        this.isOwner = amINewOwner;

        Platform.runLater(() -> {
            if (amINewOwner) {
                // I became the owner - show Start button
                startGameButton.setVisible(true);
                updateStatus("You are now the room owner. Start the game when ready.");
            } else {
                // Someone else is the owner - hide Start button
                startGameButton.setVisible(false);
                updateStatus(newOwnerNickname + " is now the room owner.");
            }
        });
    }

    private void returnToLobby() {
        Platform.runLater(() -> {
            if (stage == null) {
                System.err.println("Cannot return to lobby: stage is null");
                return;
            }

            try {
                FXMLLoader loader = new FXMLLoader(getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LobbyView.fxml"));
                Parent root = loader.load();

                LobbyController lobbyController = loader.getController();
                lobbyController.setConnection(connection, nickname);
                lobbyController.setStage(stage);

                Scene scene = new Scene(root, 800, 600);
                stage.setScene(scene);
                stage.setTitle("Pexeso - Lobby");
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
    }

    @Override
    public void onConnected() {
        // Reset reconnection alert flag after successful reconnection
        reconnectionAlertShown = false;
        Platform.runLater(() -> updateStatus("Connected to server"));
    }

    @Override
    public void onDisconnected(String reason) {
        // Reset reconnection alert flag
        reconnectionAlertShown = false;

        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.ERROR);
            alert.setTitle("Disconnected");
            alert.setHeaderText("Lost connection to server");
            alert.setContentText(reason);
            alert.showAndWait();

            // Return to login screen
            returnToLoginScreen();
        });
    }

    private void returnToLoginScreen() {
        if (stage == null) {
            System.err.println("Cannot return to login: stage is null");
            return;
        }

        try {
            FXMLLoader loader = new FXMLLoader(getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LoginView.fxml"));
            Parent root = loader.load();

            // Pass stage to LoginController
            LoginController loginController = loader.getController();
            loginController.setStage(stage);

            Scene scene = new Scene(root, 400, 300);
            stage.setScene(scene);
            stage.setTitle("Pexeso - Login");
        } catch (IOException e) {
            e.printStackTrace();
            System.err.println("Failed to load login view: " + e.getMessage());
        }
    }

    /**
     * Handle player disconnection notification
     */
    private void handlePlayerDisconnected(String playerName, String disconnectType) {
        // Don't show dialog for our own disconnect or empty player name
        if (playerName == null || playerName.trim().isEmpty()) {
            return;
        }

        if (playerName.equals(this.nickname)) {
            return;
        }

        Platform.runLater(() -> {
            if ("SHORT".equals(disconnectType)) {
                updateStatus(playerName + " disconnected - waiting for reconnect (90s)");
                // Show warning but don't remove player from list
                Alert alert = new Alert(Alert.AlertType.WARNING);
                alert.setTitle("Player Disconnected");
                alert.setHeaderText(playerName + " lost connection");
                alert.setContentText("Waiting up to 90 seconds for reconnection. Game is paused.");
                alert.show(); // Non-blocking
            } else if ("LONG".equals(disconnectType)) {
                updateStatus(playerName + " disconnected permanently - game may end");
                // Show error - player won't return
                Alert alert = new Alert(Alert.AlertType.ERROR);
                alert.setTitle("Player Lost");
                alert.setHeaderText(playerName + " disconnected permanently");
                alert.setContentText("Player did not reconnect within timeout. Game may be ended.");
                alert.show();
            }
        });
    }

    /**
     * Format error message to be more user-friendly
     * Removes error codes and provides clearer messages
     */
    private String formatErrorMessage(String rawError) {
        // Split into error code and message
        String[] parts = rawError.split(" ", 2);
        String errorCode = parts[0];
        String errorMessage = parts.length > 1 ? parts[1] : "";

        // Map error codes to user-friendly messages
        switch (errorCode) {
            case "NEED_MORE_PLAYERS":
                // Use server's message which includes exact player count
                return errorMessage.isEmpty() ? "Need more players to start the game" : errorMessage;
            case "NOT_ROOM_OWNER":
                return "Only the room owner can start the game";
            case "GAME_NOT_STARTED":
                return "Game has not started yet";
            case "NOT_YOUR_TURN":
                return "It's not your turn";
            case "INVALID_CARD":
                return "Cannot flip that card";
            case "INVALID_MOVE":
                return errorMessage.isEmpty() ? "Invalid move" : errorMessage;
            case "NOT_IN_ROOM":
                return "You are not in a room";
            case "ALREADY_IN_ROOM":
                return "You are already in a room";
            default:
                // If we have a message, use it; otherwise use the code
                return errorMessage.isEmpty() ? errorCode.replace("_", " ") : errorMessage;
        }
    }

    @Override
    public void onError(String error) {
        Platform.runLater(() -> {
            updateStatus(error);

            // Show alert ONLY for the first reconnection message
            if (error.contains("Connection lost") && !reconnectionAlertShown) {
                reconnectionAlertShown = true;

                Alert alert = new Alert(Alert.AlertType.WARNING);
                alert.setTitle("Connection Issue");
                alert.setHeaderText("Connection Lost");
                alert.setContentText("Connection to server was lost. Attempting to reconnect...");
                alert.show();  // Non-blocking
            }
            // For subsequent reconnection messages, just update status (no alert)
        });
    }
}
