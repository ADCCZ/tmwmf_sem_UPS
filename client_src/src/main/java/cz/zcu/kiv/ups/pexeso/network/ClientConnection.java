package cz.zcu.kiv.ups.pexeso.network;

import cz.zcu.kiv.ups.pexeso.protocol.ProtocolConstants;
import cz.zcu.kiv.ups.pexeso.util.Logger;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;

/**
 * Manages TCP connection to the game server
 * Runs a separate thread for receiving messages
 */
public class ClientConnection {

    private Socket socket;
    private PrintWriter out;
    private BufferedReader in;
    private Thread readerThread;
    private volatile boolean running = false;
    private MessageListener listener;

    private String serverHost;
    private int serverPort;
    private int clientId = -1;  // Client ID for reconnection
    private boolean autoReconnect = false;  // Enable auto-reconnect after authentication
    private volatile boolean reconnecting = false;
    private volatile boolean userDisconnect = false;  // Track user-initiated disconnect
    private volatile boolean serverShutdown = false;  // Track server shutdown

    /**
     * Create a new client connection
     *
     * @param listener Listener for receiving messages and events
     */
    public ClientConnection(MessageListener listener) {
        this.listener = listener;
    }

    /**
     * Connect to the server
     *
     * @param host Server hostname or IP address
     * @param port Server port
     * @throws IOException If connection fails
     */
    public void connect(String host, int port) throws IOException {
        this.serverHost = host;
        this.serverPort = port;
        this.userDisconnect = false;  // Reset user disconnect flag for new connection
        this.serverShutdown = false;  // Reset server shutdown flag for new connection

        Logger.info("Connecting to " + host + ":" + port);

        try {
            // Create socket with connection timeout
            socket = new Socket();
            socket.connect(new java.net.InetSocketAddress(host, port),
                          ProtocolConstants.CONNECTION_TIMEOUT_MS);
            socket.setSoTimeout(ProtocolConstants.READ_TIMEOUT_MS);

            // Setup streams
            out = new PrintWriter(socket.getOutputStream(), true, StandardCharsets.UTF_8);
            in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));

            // Start reader thread
            running = true;
            readerThread = new Thread(this::readerLoop, "ClientConnection-Reader");
            readerThread.setDaemon(true);
            readerThread.start();

            Logger.info("Connected successfully to " + host + ":" + port);

            // Notify listener
            if (listener != null) {
                listener.onConnected();
            }

        } catch (IOException e) {
            Logger.error("Connection failed: " + e.getMessage());
            cleanup();
            throw e;
        }
    }

    /**
     * Send a message to the server
     *
     * @param message Message to send (newline will be appended automatically)
     * @return true if sent successfully, false otherwise
     */
    public boolean sendMessage(String message) {
        if (out == null || !isConnected()) {
            return false;
        }

        try {
            out.println(message);
            return !out.checkError();
        } catch (Exception e) {
            System.err.println("Failed to send message: " + e.getMessage());
            return false;
        }
    }

    /**
     * Check if connected to server
     *
     * @return true if connected, false otherwise
     */
    public boolean isConnected() {
        return socket != null && socket.isConnected() && !socket.isClosed() && running;
    }

    /**
     * Disconnect from server
     */
    public void disconnect() {
        Logger.info("Disconnecting from server (user-initiated)");
        userDisconnect = true;  // Mark as user-initiated disconnect
        running = false;
        cleanup();

        if (listener != null) {
            listener.onDisconnected("User requested disconnect");
        }
    }

    /**
     * Reader thread loop - reads messages from server
     */
    private void readerLoop() {
        try {
            String line;
            while (running && (line = in.readLine()) != null) {
                final String message = line.trim();

                if (!message.isEmpty()) {
                    // Check for server shutdown before passing to listener
                    if (message.startsWith(ProtocolConstants.CMD_SERVER_SHUTDOWN)) {
                        serverShutdown = true;
                        running = false;
                        if (listener != null) {
                            listener.onDisconnected("Server is shutting down");
                        }
                        break;  // Exit read loop
                    }

                    if (listener != null) {
                        listener.onMessageReceived(message);
                    }
                }
            }

            // Connection closed by server (but don't notify if auto-reconnect will handle it)
            if (running) {
                running = false;
                // Only notify disconnect if auto-reconnect won't handle it
                if (listener != null && (!autoReconnect || clientId <= 0 || userDisconnect || serverShutdown)) {
                    listener.onDisconnected("Connection closed by server");
                }
            }

        } catch (SocketTimeoutException e) {
            // Socket timeout means connection is dead - exit loop to trigger reconnect
            Logger.warning("Connection timeout - will attempt reconnect");
            running = false;
            if (listener != null) {
                listener.onError("Connection timeout - attempting reconnect...");
            }

        } catch (IOException e) {
            if (running) {
                Logger.error("Connection error: " + e.getMessage());
                if (listener != null) {
                    listener.onError("Connection lost: " + e.getMessage());
                }
            }
            running = false;  // Ensure we exit the loop

        } finally {
            running = false;
            cleanup();

            // Attempt automatic reconnect if enabled (but NOT if user disconnected or server shut down)
            if (autoReconnect && clientId > 0 && !reconnecting && !userDisconnect && !serverShutdown) {
                Logger.info("Auto-reconnect starting (client ID: " + clientId + ")");
                // Notify user that reconnection is starting
                if (listener != null) {
                    listener.onError("Connection lost. Attempting to reconnect...");
                }

                for (int attempt = 1; attempt <= ProtocolConstants.MAX_RECONNECT_ATTEMPTS; attempt++) {
                    // Wait before each attempt EXCEPT the first one
                    if (attempt > 1) {
                        try {
                            Thread.sleep(ProtocolConstants.RECONNECT_INTERVAL_MS);
                        } catch (InterruptedException ie) {
                            Thread.currentThread().interrupt();
                            break;
                        }
                    }

                    // Notify user about reconnect attempt
                    if (listener != null) {
                        listener.onError("Reconnecting... (attempt " + attempt + "/" + ProtocolConstants.MAX_RECONNECT_ATTEMPTS + ")");
                    }

                    if (reconnect()) {
                        Logger.info("Reconnect successful after " + attempt + " attempts");
                        return;  // Successfully reconnected, exit readerLoop
                    }
                }
                Logger.error("Failed to reconnect after " + ProtocolConstants.MAX_RECONNECT_ATTEMPTS + " attempts");
                if (listener != null) {
                    listener.onDisconnected("Failed to reconnect after " + ProtocolConstants.MAX_RECONNECT_ATTEMPTS + " attempts");
                }
            }
        }
    }

    /**
     * Cleanup resources
     */
    private void cleanup() {
        try {
            if (out != null) {
                out.close();
            }
        } catch (Exception e) {
            // Ignore
        }

        try {
            if (in != null) {
                in.close();
            }
        } catch (Exception e) {
            // Ignore
        }

        try {
            if (socket != null && !socket.isClosed()) {
                socket.close();
            }
        } catch (Exception e) {
            // Ignore
        }
    }

    /**
     * Set message listener (for changing scenes)
     *
     * @param listener New message listener
     */
    public void setMessageListener(MessageListener listener) {
        this.listener = listener;
    }

    /**
     * Get server host
     */
    public String getServerHost() {
        return serverHost;
    }

    /**
     * Get server port
     */
    public int getServerPort() {
        return serverPort;
    }

    /**
     * Set client ID (after receiving WELCOME)
     */
    public void setClientId(int clientId) {
        this.clientId = clientId;
        this.autoReconnect = (clientId > 0);  // Enable auto-reconnect after authentication
        Logger.info("Client authenticated (ID: " + clientId + "), auto-reconnect enabled");
    }

    /**
     * Get client ID
     */
    public int getClientId() {
        return clientId;
    }

    /**
     * Attempt reconnection with previous client ID
     *
     * @return true if reconnect successful, false otherwise
     */
    public boolean reconnect() {
        if (clientId <= 0 || reconnecting) {
            return false;
        }

        reconnecting = true;

        try {
            // Close old connection
            cleanup();

            // Create new socket
            socket = new Socket();
            socket.connect(new java.net.InetSocketAddress(serverHost, serverPort),
                          ProtocolConstants.CONNECTION_TIMEOUT_MS);
            socket.setSoTimeout(ProtocolConstants.READ_TIMEOUT_MS);

            // Setup streams
            out = new PrintWriter(socket.getOutputStream(), true, StandardCharsets.UTF_8);
            in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));

            // Send RECONNECT command
            out.println("RECONNECT " + clientId);

            // Wait for response
            String response = in.readLine();

            if (response != null && response.startsWith("WELCOME")) {
                // Reconnect successful, restart reader thread
                running = true;
                userDisconnect = false;
                readerThread = new Thread(this::readerLoop, "ClientConnection-Reader");
                readerThread.setDaemon(true);
                readerThread.start();

                if (listener != null) {
                    listener.onConnected();
                }

                reconnecting = false;
                return true;
            } else if (response != null && response.contains("already connected")) {
                // Server says we're already connected
                cleanup();
                reconnecting = false;
                autoReconnect = false;

                if (listener != null) {
                    listener.onDisconnected("Already connected on another session. Please close other clients and reconnect.");
                }
                return false;
            } else if (response != null && response.contains("Client not found") ||
                      (response != null && response.contains("session expired"))) {
                // Client was removed from server after long disconnect timeout
                // Stop auto-reconnect and require manual login
                System.err.println("Session expired - client was removed from server after timeout");
                cleanup();
                reconnecting = false;
                autoReconnect = false;  // DISABLE auto-reconnect permanently
                clientId = -1;  // Clear client ID

                if (listener != null) {
                    listener.onDisconnected("Your session has expired. Please login again manually.");
                }
                return false;  // This will exit the auto-reconnect loop
            } else {
                System.err.println("Reconnect failed: " + response);
                cleanup();
                reconnecting = false;
                return false;
            }

        } catch (Exception e) {
            System.err.println("Reconnect error: " + e.getMessage());
            cleanup();
            reconnecting = false;
            return false;
        }
    }

    /**
     * Check if currently attempting to reconnect
     */
    public boolean isReconnecting() {
        return reconnecting;
    }
}
