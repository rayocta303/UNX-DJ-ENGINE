#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <psapi.h>
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #include <mach/mach.h>
#else
    #include <unistd.h>
#endif

static FILE* g_logFile = NULL;

void Log_Init(void) {
    const char* logPath = "xdjunx.log";
    
#if defined(PLATFORM_IOS)
    // On iOS, we write to the Documents folder so we can retrieve it via iTunes/Files app
    extern const char* ios_get_documents_path(const char* filename);
    logPath = ios_get_documents_path("xdjunx.log");
#endif

    g_logFile = fopen(logPath, "a");
    if (g_logFile) {
        time_t now = time(NULL);
        fprintf(g_logFile, "\n--- SESSION START: %s", ctime(&now));
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
}

void Log_Write(LogLevel level, const char* fmt, ...) {
    const char* levelStr = "INFO";
    switch (level) {
        case UNX_LEVEL_WARNING: levelStr = "WARN"; break;
        case UNX_LEVEL_ERROR:   levelStr = "ERR "; break;
        case UNX_LEVEL_DEBUG:   levelStr = "DEBG"; break;
        default: break;
    }

    char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Print to console
    printf("[%s] [%s] %s\n", timestamp, levelStr, buffer);

    // Print to file
    if (g_logFile) {
        fprintf(g_logFile, "[%s] [%s] %s\n", timestamp, levelStr, buffer);
        fflush(g_logFile);
    }
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

void Log_Heartbeat(void) {
    static double lastHeartbeat = 0;
    // We can't use GetTime() here without raylib.h, so we'll just log when called.
    // In main.c we will call this periodically.
    float ram = Log_GetRAMUsage();
    UNX_LOG_INFO("Heartbeat - RAM Usage: %.2f MB", ram);
}
