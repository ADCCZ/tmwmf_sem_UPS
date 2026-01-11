#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int logger_init(const char *filename) {
    pthread_mutex_lock(&log_mutex);

    if (filename != NULL) {
        // Use "w" mode to truncate (clear) the log file on each server start
        // This ensures clean logs for each test run
        log_file = fopen(filename, "w");
        if (log_file == NULL) {
            pthread_mutex_unlock(&log_mutex);
            perror("Failed to open log file");
            return -1;
        }
    }

    pthread_mutex_unlock(&log_mutex);
    logger_log(LOG_INFO, "Logger initialized");
    return 0;
}

void logger_log(log_level_t level, const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    // Determine level string
    const char *level_str;
    switch (level) {
        case LOG_INFO:    level_str = "INFO"; break;
        case LOG_WARNING: level_str = "WARN"; break;
        case LOG_ERROR:   level_str = "ERROR"; break;
        default:          level_str = "UNKNOWN"; break;
    }

    // Print to stdout
    printf("[%s] [%s] ", time_buf, level_str);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);

    // Print to file if available
    if (log_file != NULL) {
        fprintf(log_file, "[%s] [%s] ", time_buf, level_str);
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

void logger_shutdown(void) {
    pthread_mutex_lock(&log_mutex);

    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }

    pthread_mutex_unlock(&log_mutex);
}
