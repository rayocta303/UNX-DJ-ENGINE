#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "raylib.h"


#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER
    #define Rectangle WinRectangle
    #define CloseWindow WinCloseWindow
    #define ShowCursor WinShowCursor
    #define DrawText WinDrawText
    #include <windows.h>
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor
    #undef DrawText
    #include <psapi.h>
    static CRITICAL_SECTION g_logLock;
    static bool g_lockInitialized = false;
#elif defined(__ANDROID__)
    #include <android/log.h>
    #include <unistd.h>
    #include <pthread.h>
    static pthread_mutex_t g_logLock = PTHREAD_MUTEX_INITIALIZER;
#else
    #include <unistd.h>
    #include <pthread.h>
    #if defined(__APPLE__)
        #include <mach/mach.h>
    #endif
    static pthread_mutex_t g_logLock = PTHREAD_MUTEX_INITIALIZER;
#endif

static FILE* g_logFile = NULL;

void Log_Init(void) {
#if defined(_WIN32)
    if (!g_lockInitialized) {
        InitializeCriticalSection(&g_logLock);
        g_lockInitialized = true;
    }
#endif

    const char* logPath = "unx.log";

#if defined(__ANDROID__)
    static char androidPath[512];
    // Attempt 1: Accessible SD Card path
    strncpy(androidPath, "/sdcard/unx.log", sizeof(androidPath)-1);
    FILE *testFile = fopen(androidPath, "a");
    if (testFile) {
        fclose(testFile);
    } else {
        // Attempt 2: Try creating via "w" in case "a" is restricted on new file
        testFile = fopen(androidPath, "w");
        if (testFile) {
            fclose(testFile);
        } else {
            // Attempt 3: Fallback to internal private storage (Always works)
            const char* appDir = GetApplicationDirectory();
            if (appDir && appDir[0] != '\0') {
                snprintf(androidPath, sizeof(androidPath), "%s/unx.log", appDir);
            } else {
                // Last resort fallback
                strncpy(androidPath, "unx.log", sizeof(androidPath)-1);
            }
        }
    }
    logPath = androidPath;
    __android_log_print(ANDROID_LOG_INFO, "XDJ-UNX", "[LOGGER] Final Path: %s", logPath);
#elif defined(PLATFORM_IOS)
    // On iOS, we write to the Documents folder so we can retrieve it via iTunes/Files app
    extern const char* ios_get_documents_path(const char* filename);
    logPath = ios_get_documents_path("unx.log");
#endif

    g_logFile = fopen(logPath, "a");
    if (g_logFile) {
        time_t now = time(NULL);
        char* timeStr = ctime(&now);
        if (timeStr) fprintf(g_logFile, "\n--- SESSION START: %s", timeStr);
        fflush(g_logFile);
    }
    
    UNX_LOG_INFO("Logger initialized on %s", 
#if defined(_WIN32)
        "Windows"
#elif defined(PLATFORM_IOS)
        "iOS"
#elif defined(__ANDROID__)
        "Android"
#else
        "Linux/Unix"
#endif
    );
}

void Log_Close(void) {
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = NULL;
    }
#if defined(_WIN32)
    if (g_lockInitialized) {
        DeleteCriticalSection(&g_logLock);
        g_lockInitialized = false;
    }
#endif
}

void Log_Flush(void) {
    if (g_logFile) {
#if defined(_WIN32)
        EnterCriticalSection(&g_logLock);
#else
        pthread_mutex_lock(&g_logLock);
#endif
        fflush(g_logFile);
#if defined(_WIN32)
        LeaveCriticalSection(&g_logLock);
#else
        pthread_mutex_unlock(&g_logLock);
#endif
    }
}

void Log_Write(LogLevel level, const char* fmt, ...) {
    const char* levelStr = "INFO";
    switch (level) {
        case UNX_LEVEL_WARNING: levelStr = "WARN"; break;
        case UNX_LEVEL_ERROR:   levelStr = "ERR "; break;
        case UNX_LEVEL_DEBUG:   levelStr = "DEBG"; break;
        default: break;
    }

#if defined(_WIN32)
    if (!g_lockInitialized) return;
    EnterCriticalSection(&g_logLock);
#else
    pthread_mutex_lock(&g_logLock);
#endif

    char timestamp[32];
    time_t now = time(NULL);
    struct tm tm_info;
#if defined(_WIN32)
    localtime_s(&tm_info, &now);
#else
    localtime_r(&now, &tm_info);
#endif
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &tm_info);

    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Print to console
    printf("[%s] [%s] %s\n", timestamp, levelStr, buffer);

#if defined(__ANDROID__)
    int priority = ANDROID_LOG_INFO;
    if (level == UNX_LEVEL_WARNING) priority = ANDROID_LOG_WARN;
    else if (level == UNX_LEVEL_ERROR) priority = ANDROID_LOG_ERROR;
    else if (level == UNX_LEVEL_DEBUG) priority = ANDROID_LOG_DEBUG;
    __android_log_print(priority, "XDJ-UNX", "[%s] %s", levelStr, buffer);
#endif

    // Print to file
    if (g_logFile) {
        fprintf(g_logFile, "[%s] [%s] %s\n", timestamp, levelStr, buffer);
        fflush(g_logFile); // Always flush so we don't lose logs on crash
    }

#if defined(_WIN32)
    LeaveCriticalSection(&g_logLock);
#else
    pthread_mutex_unlock(&g_logLock);
#endif
}

float Log_GetRAMUsage(void) {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return (float)pmc.PrivateUsage / (1024.0f * 1024.0f);
    }
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        return (float)info.resident_size / (1024.0f * 1024.0f);
    }
#else
    // Linux fallback via /proc/self/statm
    FILE* f = fopen("/proc/self/statm", "r");
    if (f) {
        long pages;
        if (fscanf(f, "%ld", &pages) == 1) {
            fclose(f);
            return (float)(pages * sysconf(_SC_PAGESIZE)) / (1024.0f * 1024.0f);
        }
        fclose(f);
    }
#endif
    return 0.0f;
}



