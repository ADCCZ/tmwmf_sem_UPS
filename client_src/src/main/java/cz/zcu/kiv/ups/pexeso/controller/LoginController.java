package cz.zcu.kiv.ups.pexeso.controller;

import cz.zcu.kiv.ups.pexeso.network.ClientConnection;
import cz.zcu.kiv.ups.pexeso.network.MessageListener;
import cz.zcu.kiv.ups.pexeso.protocol.ProtocolConstants;
import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.scene.control.Button;
import javafx.scene.control.TextArea;
import javafx.scene.control.TextField;

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
            log("Successfully authenticated!");
        } else if (message.startsWith(ProtocolConstants.CMD_ERROR)) {
            log("Server error: " + message);
        } else if (message.startsWith(ProtocolConstants.CMD_PING)) {
            // Respond to PING with PONG
            connection.sendMessage(ProtocolConstants.CMD_PONG);
            log("Sent: " + ProtocolConstants.CMD_PONG);
        }
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
