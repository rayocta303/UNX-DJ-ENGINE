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

// Registration for crash and error handlers
void Log_RegisterCrashHandlers(void);

// Heartbeat for deadlock detection
void Log_Heartbeat(void);

// Detailed hardware/software info logging
void Log_LogDeviceInfo(const char* gpuModel);


// Standard formatted logging
void Log_Write(LogLevel level, const char* fmt, ...);

// Get current RAM usage in MB
float Log_GetRAMUsage(void);



// Memory management wrappers for OOM tracking
void* Log_Malloc(size_t size, const char* file, int line);
void* Log_Calloc(size_t nmemb, size_t size, const char* file, int line);
void* Log_Realloc(void* ptr, size_t size, const char* file, int line);
void  Log_Free(void* ptr);

#define UNX_MALLOC(s)    Log_Malloc(s, __FILE__, __LINE__)
#define UNX_CALLOC(n, s) Log_Calloc(n, s, __FILE__, __LINE__)
#define UNX_REALLOC(p, s) Log_Realloc(p, s, __FILE__, __LINE__)
#define UNX_FREE(p)      Log_Free(p)

#define UNX_LOG_INFO(...)  Log_Write(UNX_LEVEL_INFO, __VA_ARGS__)
#define UNX_LOG_WARN(...)  Log_Write(UNX_LEVEL_WARNING, __VA_ARGS__)
#define UNX_LOG_ERR(...)   Log_Write(UNX_LEVEL_ERROR, __VA_ARGS__)
#define UNX_LOG_DEBUG(...) Log_Write(UNX_LEVEL_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H
