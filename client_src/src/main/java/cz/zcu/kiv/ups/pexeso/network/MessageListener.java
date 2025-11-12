package cz.zcu.kiv.ups.pexeso.network;

/**
 * Interface for receiving messages from the server
 */
public interface MessageListener {

    /**
     * Called when a message is received from the server
     * This is called from the network thread, use Platform.runLater for UI updates
     *
     * @param message The received message (without newline)
     */
    void onMessageReceived(String message);

    /**
     * Called when connected to the server
     */
    void onConnected();

    /**
     * Called when disconnected from the server
     *
     * @param reason Reason for disconnection
     */
    void onDisconnected(String reason);

    /**
     * Called when a connection error occurs
     *
     * @param error Error message
     */
    void onError(String error);
}
