#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UNX_LEVEL_INFO,
    UNX_LEVEL_WARNING,
    UNX_LEVEL_ERROR,
    UNX_LEVEL_DEBUG
} LogLevel;

// Initializes logging to file and console
void Log_Init(void);

// Closes logging
void Log_Close(void);
void Log_Flush(void);


// Standard formatted logging
void Log_Write(LogLevel level, const char* fmt, ...);

// Get current RAM usage in MB
float Log_GetRAMUsage(void);



#define UNX_LOG_INFO(...)  Log_Write(UNX_LEVEL_INFO, __VA_ARGS__)
#define UNX_LOG_WARN(...)  Log_Write(UNX_LEVEL_WARNING, __VA_ARGS__)
#define UNX_LOG_ERR(...)   Log_Write(UNX_LEVEL_ERROR, __VA_ARGS__)
#define UNX_LOG_DEBUG(...) Log_Write(UNX_LEVEL_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H
