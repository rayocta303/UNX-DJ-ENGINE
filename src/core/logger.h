#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>

typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

// Initializes logging to file and console
void Log_Init(void);

// Closes logging
void Log_Close(void);

// Standard formatted logging
void Log_Write(LogLevel level, const char* fmt, ...);

// Get current RAM usage in MB
float Log_GetRAMUsage(void);

// Log current RAM usage and system state
void Log_Heartbeat(void);

#define UNX_LOG_INFO(...)  Log_Write(LOG_INFO, __VA_ARGS__)
#define UNX_LOG_WARN(...)  Log_Write(LOG_WARNING, __VA_ARGS__)
#define UNX_LOG_ERR(...)   Log_Write(LOG_ERROR, __VA_ARGS__)
#define UNX_LOG_DEBUG(...) Log_Write(LOG_DEBUG, __VA_ARGS__)

#endif // LOGGER_H
