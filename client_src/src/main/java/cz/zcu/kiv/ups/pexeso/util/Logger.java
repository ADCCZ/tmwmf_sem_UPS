package cz.zcu.kiv.ups.pexeso.util;

import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

/**
 * Simple logger for client application
 */
public class Logger {
    private static final String LOG_FILE = "client.log";
    private static final DateTimeFormatter TIME_FORMAT = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");
    private static PrintWriter writer;
    private static boolean initialized = false;

    /**
     * Initialize logger (truncate log file on start)
     */
    public static synchronized void init() {
        if (initialized) {
            return;
        }

        try {
            writer = new PrintWriter(new FileWriter(LOG_FILE, false), true);
            initialized = true;
            info("Logger initialized");
        } catch (IOException e) {
            System.err.println("Failed to initialize logger: " + e.getMessage());
        }
    }

    /**
     * Log info message
     */
    public static synchronized void info(String message) {
        log("INFO", message);
    }

    /**
     * Log warning message
     */
    public static synchronized void warning(String message) {
        log("WARN", message);
    }

    /**
     * Log error message
     */
    public static synchronized void error(String message) {
        log("ERROR", message);
    }

    /**
     * Internal log method
     */
    private static void log(String level, String message) {
        if (!initialized) {
            init();
        }

        String timestamp = LocalDateTime.now().format(TIME_FORMAT);
        String logMessage = String.format("[%s] [%s] %s", timestamp, level, message);

        // Write to file
        if (writer != null) {
            writer.println(logMessage);
            writer.flush();
        }

        // Also print to console for development
        System.out.println(logMessage);
    }

    /**
     * Close logger
     */
    public static synchronized void close() {
        if (writer != null) {
            info("Logger shutting down");
            writer.close();
            writer = null;
            initialized = false;
        }
    }
}
