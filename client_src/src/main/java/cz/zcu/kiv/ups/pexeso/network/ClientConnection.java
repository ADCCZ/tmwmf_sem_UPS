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
}
