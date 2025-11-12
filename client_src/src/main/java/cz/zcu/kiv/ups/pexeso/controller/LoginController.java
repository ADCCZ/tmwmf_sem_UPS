package cz.zcu.kiv.ups.pexeso.controller;

import cz.zcu.kiv.ups.pexeso.network.ClientConnection;
import cz.zcu.kiv.ups.pexeso.network.MessageListener;
import cz.zcu.kiv.ups.pexeso.protocol.ProtocolConstants;
import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.TextArea;
import javafx.scene.control.TextField;
import javafx.stage.Stage;

import java.io.IOException;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;

/**
 * Controller for the login screen
 */
public class LoginController implements MessageListener {

    @FXML
    private TextField ipField;

    @FXML
    private TextField portField;

    @FXML
    private TextField nicknameField;

    @FXML
    private Button connectButton;

    @FXML
    private TextArea logArea;

    private ClientConnection connection;
    private boolean connected = false;
    private DateTimeFormatter timeFormatter = DateTimeFormatter.ofPattern("HH:mm:ss");
    private Stage stage;
    private String authenticatedNickname;

    /**
     * Initialize the controller (called by JavaFX)
     */
    @FXML
    public void initialize() {
        // Set default values
        ipField.setText("127.0.0.1");
        portField.setText("10000");
        nicknameField.setText("Player" + (int)(Math.random() * 1000));

        // Initialize connection
        connection = new ClientConnection(this);

        log("Client initialized. Ready to connect.");
    }

    /**
     * Set the stage for scene switching
     */
    public void setStage(Stage stage) {
        this.stage = stage;
    }

    /**
     * Handle connect button click
     */
    @FXML
    private void handleConnect() {
        if (connected) {
            disconnect();
        } else {
            connect();
        }
    }

    /**
     * Connect to server
     */
    private void connect() {
        String ip = ipField.getText().trim();
        String portStr = portField.getText().trim();
        String nickname = nicknameField.getText().trim();

        // Validate inputs
        if (ip.isEmpty()) {
            log("ERROR: IP address is required");
            return;
        }

        if (portStr.isEmpty()) {
            log("ERROR: Port is required");
            return;
        }

        if (nickname.isEmpty()) {
            log("ERROR: Nickname is required");
            return;
        }

        if (nickname.length() > ProtocolConstants.MAX_NICK_LENGTH) {
            log("ERROR: Nickname too long (max " + ProtocolConstants.MAX_NICK_LENGTH + " characters)");
            return;
        }

        // Check for invalid characters in nickname
        if (nickname.contains(" ")) {
            log("ERROR: Nickname cannot contain spaces");
            return;
        }

        // Check for special characters that might break protocol
        if (!nickname.matches("[a-zA-Z0-9_-]+")) {
            log("ERROR: Nickname can only contain letters, numbers, underscores, and hyphens");
            return;
        }

        // Basic IP address format validation
        if (!ip.matches("^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$") && !ip.equals("localhost")) {
            log("ERROR: Invalid IP address format (use IPv4 format: xxx.xxx.xxx.xxx or 'localhost')");
            return;
        }

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

        // Disable input fields during connection
        setInputsEnabled(false);
        log("Connecting to " + ip + ":" + port + "...");

        // Save nickname for later use
        authenticatedNickname = nickname;

        // Connect in background thread to avoid blocking UI
        new Thread(() -> {
            try {
                connection.connect(ip, port);

                // Connection successful, send HELLO
                String helloMessage = ProtocolConstants.CMD_HELLO + " " + nickname;
                connection.sendMessage(helloMessage);
                log("Sent: " + helloMessage);

            } catch (IOException e) {
                Platform.runLater(() -> {
                    log("ERROR: Failed to connect: " + e.getMessage());
                    setInputsEnabled(true);
                    connectButton.setText("CONNECT");
                });
            }
        }, "Connection-Thread").start();
    }

    /**
     * Disconnect from server
     */
    private void disconnect() {
        log("Disconnecting...");
        connection.disconnect();
    }

    /**
     * Set input fields enabled/disabled
     */
    private void setInputsEnabled(boolean enabled) {
        ipField.setDisable(!enabled);
        portField.setDisable(!enabled);
        nicknameField.setDisable(!enabled);
    }

    /**
     * Log a message to the log area with timestamp
     */
    private void log(String message) {
        String timestamp = LocalTime.now().format(timeFormatter);
        String logMessage = "[" + timestamp + "] " + message + "\n";

        // Ensure we're on JavaFX thread
        if (Platform.isFxApplicationThread()) {
            logArea.appendText(logMessage);
        } else {
            Platform.runLater(() -> logArea.appendText(logMessage));
        }
    }

    // MessageListener implementation

    @Override
    public void onMessageReceived(String message) {
        log("Received: " + message);

        // Parse basic responses
        if (message.startsWith(ProtocolConstants.CMD_WELCOME)) {
            // Parse client ID from WELCOME message
            // Format: WELCOME <client_id> [message]
            String[] parts = message.split(" ");
            if (parts.length >= 2) {
                try {
                    int clientId = Integer.parseInt(parts[1]);
                    connection.setClientId(clientId);
                    log("Client ID: " + clientId);
                } catch (NumberFormatException e) {
                    log("Warning: Could not parse client ID from WELCOME");
                }
            }

            log("Successfully authenticated!");
            // Switch to lobby view
            switchToLobby();
        } else if (message.startsWith(ProtocolConstants.CMD_ERROR)) {
            log("Server error: " + message);
        } else if (message.startsWith(ProtocolConstants.CMD_PING)) {
            // Respond to PING with PONG
            connection.sendMessage(ProtocolConstants.CMD_PONG);
            log("Sent: " + ProtocolConstants.CMD_PONG);
        }
    }

    /**
     * Switch to lobby view after successful authentication
     */
    private void switchToLobby() {
        Platform.runLater(() -> {
            try {
                FXMLLoader loader = new FXMLLoader(getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LobbyView.fxml"));
                Parent root = loader.load();

                LobbyController lobbyController = loader.getController();
                lobbyController.setConnection(connection, authenticatedNickname);
                lobbyController.setStage(stage);

                Scene scene = new Scene(root, 800, 600);
                stage.setScene(scene);
                stage.setTitle("Pexeso - Lobby");

                log("Switched to lobby");
            } catch (IOException e) {
                e.printStackTrace();
                log("ERROR: Failed to load lobby view: " + e.getMessage());
            }
        });
    }

    @Override
    public void onConnected() {
        Platform.runLater(() -> {
            connected = true;
            connectButton.setText("DISCONNECT");
            log("Connected to server!");
        });
    }

    @Override
    public void onDisconnected(String reason) {
        Platform.runLater(() -> {
            connected = false;
            connectButton.setText("CONNECT");
            setInputsEnabled(true);
            log("Disconnected: " + reason);
        });
    }

    @Override
    public void onError(String error) {
        Platform.runLater(() -> {
            log("ERROR: " + error);
            if (connected) {
                disconnect();
            }
        });
    }
}
