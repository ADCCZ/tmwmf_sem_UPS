package cz.zcu.kiv.ups.pexeso.network;

import cz.zcu.kiv.ups.pexeso.protocol.ProtocolConstants;

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

            // Notify listener
            if (listener != null) {
                listener.onConnected();
            }

        } catch (IOException e) {
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

                if (!message.isEmpty() && listener != null) {
                    listener.onMessageReceived(message);
                }
            }

            // Connection closed by server
            if (running) {
                running = false;
                if (listener != null) {
                    listener.onDisconnected("Connection closed by server");
                }
            }

        } catch (SocketTimeoutException e) {
            System.err.println("Socket timeout: " + e.getMessage());
            if (running && listener != null) {
                listener.onError("Connection timeout");
            }

        } catch (IOException e) {
            if (running) {
                System.err.println("Connection error: " + e.getMessage());
                if (listener != null) {
                    listener.onError("Connection lost: " + e.getMessage());
                }
            }

        } finally {
            running = false;
            cleanup();

            // Attempt automatic reconnect if enabled
            if (autoReconnect && clientId > 0 && !reconnecting) {
                System.out.println("Auto-reconnect triggered");

                for (int attempt = 1; attempt <= ProtocolConstants.MAX_RECONNECT_ATTEMPTS; attempt++) {
                    System.out.println("Reconnect attempt " + attempt + "/" + ProtocolConstants.MAX_RECONNECT_ATTEMPTS);

                    if (reconnect()) {
                        System.out.println("Auto-reconnect successful!");
                        return;  // Successfully reconnected, exit readerLoop
                    }

                    // Wait before next attempt
                    if (attempt < ProtocolConstants.MAX_RECONNECT_ATTEMPTS) {
                        try {
                            Thread.sleep(ProtocolConstants.RECONNECT_INTERVAL_MS);
                        } catch (InterruptedException ie) {
                            Thread.currentThread().interrupt();
                            break;
                        }
                    }
                }

                System.err.println("Auto-reconnect failed after " + ProtocolConstants.MAX_RECONNECT_ATTEMPTS + " attempts");
                if (listener != null) {
                    listener.onDisconnected("Failed to reconnect");
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
        System.out.println("Client ID set to: " + clientId + ", auto-reconnect enabled");
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
        if (clientId <= 0) {
            System.err.println("Cannot reconnect: no client ID");
            return false;
        }

        if (reconnecting) {
            System.err.println("Already reconnecting");
            return false;
        }

        reconnecting = true;
        System.out.println("Attempting reconnect with client ID: " + clientId);

        try {
            // Close old connection
            cleanup();
            Thread.sleep(ProtocolConstants.RECONNECT_INTERVAL_MS);

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
            System.out.println("Reconnect response: " + response);

            if (response != null && response.startsWith("WELCOME")) {
                // Reconnect successful, restart reader thread
                running = true;
                readerThread = new Thread(this::readerLoop, "ClientConnection-Reader");
                readerThread.setDaemon(true);
                readerThread.start();

                if (listener != null) {
                    listener.onConnected();
                }

                reconnecting = false;
                System.out.println("Reconnect successful!");
                return true;
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
