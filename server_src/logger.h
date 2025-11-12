#ifndef LOGGER_H
#define LOGGER_H

/**
 * Logger module - thread-safe logging with timestamps
 */

typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

/**
 * Initialize logger
 * @param filename Log file path (NULL for stdout only)
 * @return 0 on success, -1 on error
 */
int logger_init(const char *filename);

/**
 * Log a message
 * @param level Log level
 * @param format Printf-style format string
 */
void logger_log(log_level_t level, const char *format, ...);

/**
 * Shutdown logger and close file
 */
void logger_shutdown(void);

#endif /* LOGGER_H */
