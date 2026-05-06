#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "raylib.h"
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

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
    #include <direct.h>
    static CRITICAL_SECTION g_logLock;
    static bool g_lockInitialized = false;
#elif defined(__ANDROID__) || defined(__linux__)
    #if defined(__ANDROID__)
        #include <android/log.h>
    #endif
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/utsname.h>
    #include <sys/sysinfo.h>
    static pthread_mutex_t g_logLock = PTHREAD_MUTEX_INITIALIZER;
#elif defined(__APPLE__)
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/utsname.h>
    #include <mach/mach.h>
    #include <sys/sysctl.h>
    static pthread_mutex_t g_logLock = PTHREAD_MUTEX_INITIALIZER;
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/utsname.h>
    static pthread_mutex_t g_logLock = PTHREAD_MUTEX_INITIALIZER;
#endif

// Watchdog state
static time_t g_lastHeartbeat = 0;
static bool g_watchdogRunning = false;
#if defined(_WIN32)
static HANDLE g_watchdogThread = NULL;
#else
static pthread_t g_watchdogThread;
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
    // Attempt 1: Accessible SD Card path (Older Android / STB focus)
    // We try to create a folder to be more organized
    const char* sdDir = "/sdcard/XDJ-UNX";
#if !defined(_WIN32)
    mkdir(sdDir, 0777);
#endif
    snprintf(androidPath, sizeof(androidPath), "%s/unx.log", sdDir);
    
    FILE *testFile = fopen(androidPath, "a");
    if (testFile) {
        fclose(testFile);
    } else {
        // Attempt 2: Try direct /sdcard/unx.log
        strncpy(androidPath, "/sdcard/unx.log", sizeof(androidPath)-1);
        testFile = fopen(androidPath, "a");
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
    if (!g_logFile) {
        // Desktop Fallback: If local directory is read-only, try User AppData/Home
#if defined(_WIN32)
        const char* appData = getenv("APPDATA");
        if (appData) {
            static char fallbackPath[512];
            snprintf(fallbackPath, sizeof(fallbackPath), "%s\\XDJ-UNX", appData);
            _mkdir(fallbackPath);
            strncat(fallbackPath, "\\unx.log", sizeof(fallbackPath) - strlen(fallbackPath) - 1);
            g_logFile = fopen(fallbackPath, "a");
            if (g_logFile) logPath = fallbackPath;
        }
#elif !defined(__ANDROID__) && !defined(PLATFORM_IOS)
        const char* home = getenv("HOME");
        if (home) {
            static char fallbackPath[512];
            snprintf(fallbackPath, sizeof(fallbackPath), "%s/.xdj-unx", home);
            mkdir(fallbackPath, 0777);
            strncat(fallbackPath, "/unx.log", sizeof(fallbackPath) - strlen(fallbackPath) - 1);
            g_logFile = fopen(fallbackPath, "a");
            if (g_logFile) logPath = fallbackPath;
        }
#endif
    }

    if (g_logFile) {
        time_t now = time(NULL);
        char* timeStr = ctime(&now);
        if (timeStr) fprintf(g_logFile, "\n--- SESSION START: %s", timeStr);
        fflush(g_logFile);
    }
    
    UNX_LOG_INFO("Logger initialized at: %s", logPath);
    UNX_LOG_INFO("Platform: %s", 
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

// --- Watchdog Thread ---
#if defined(_WIN32)
static DWORD WINAPI WatchdogProc(LPVOID lpParam) {
    (void)lpParam;
#else
static void* WatchdogProc(void* arg) {
    (void)arg;
#endif
    UNX_LOG_INFO("[WATCHDOG] Thread started.");
    while (g_watchdogRunning) {
#if defined(_WIN32)
        Sleep(2000);
#else
        sleep(2);
#endif
        time_t now = time(NULL);
        if (g_lastHeartbeat > 0 && (now - g_lastHeartbeat) > 5) {
            UNX_LOG_ERR("!!! [DEADLOCK DETECTED] Main thread has not responded for %d seconds!", (int)(now - g_lastHeartbeat));
            // In a real deadlock, we might want to force a crash dump or exit
            // For now, we just log it repeatedly.
        }
    }
    return 0;
}

void Log_Heartbeat(float fps, float frameTime) {
    g_lastHeartbeat = time(NULL);

    // Periodic Performance Logging (every 5 seconds)
    static time_t lastPerfLog = 0;
    if (g_lastHeartbeat - lastPerfLog >= 5) {
        float ram = Log_GetRAMUsage();
        UNX_LOG_INFO("[PERF] Heartbeat - FPS: %.1f, Frame: %.2f ms, RAM: %.2f MB", fps, frameTime * 1000.0f, ram);
        lastPerfLog = g_lastHeartbeat;
    }
}

// --- Signal Handlers ---
static void SignalHandler(int sig) {
    const char* sigName = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: sigName = "SEGMENTATION FAULT (SIGSEGV)"; break;
        case SIGFPE:  sigName = "FLOATING POINT ERROR (SIGFPE)"; break;
        case SIGILL:  sigName = "ILLEGAL INSTRUCTION (SIGILL)"; break;
        case SIGABRT: sigName = "ABORTED (SIGABRT)"; break;
#if !defined(_WIN32)
        case SIGBUS:  sigName = "BUS ERROR (SIGBUS)"; break;
#endif
    }
    
    UNX_LOG_ERR("!!! [CRASH] Fatal signal received: %s !!!", sigName);
    Log_Flush();
    
    // Default action (usually termination)
    signal(sig, SIG_DFL);
    raise(sig);
}

#if defined(_WIN32)
static LONG WINAPI UnhandledExceptionFilterEx(EXCEPTION_POINTERS* info) {
    UNX_LOG_ERR("!!! [CRASH] Unhandled Exception: 0x%08X at address 0x%p !!!", 
                info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);
    Log_Flush();
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void Log_RegisterCrashHandlers(void) {
    signal(SIGSEGV, SignalHandler);
    signal(SIGFPE,  SignalHandler);
    signal(SIGILL,  SignalHandler);
    signal(SIGABRT, SignalHandler);
#if !defined(_WIN32)
    signal(SIGBUS,  SignalHandler);
#else
    SetUnhandledExceptionFilter(UnhandledExceptionFilterEx);
#endif

    // Start Watchdog
    g_watchdogRunning = true;
    g_lastHeartbeat = time(NULL);
#if defined(_WIN32)
    g_watchdogThread = CreateThread(NULL, 0, WatchdogProc, NULL, 0, NULL);
#else
    pthread_create(&g_watchdogThread, NULL, WatchdogProc, NULL);
#endif
}

// --- Device Info Logging ---
void Log_LogDeviceInfo(const char* gpuModel) {
    UNX_LOG_INFO("=== DEVICE INFO ===");
    
    // OS & Arch
#if defined(_WIN32)
    UNX_LOG_INFO("OS          : Windows (x64)");
#elif defined(__ANDROID__)
    struct utsname un;
    uname(&un);
    UNX_LOG_INFO("OS          : Android (%s %s)", un.sysname, un.release);
    UNX_LOG_INFO("Kernel      : %s", un.version);
    UNX_LOG_INFO("Arch        : %s", un.machine);
    
    // Root status check
    bool isRooted = (access("/system/bin/su", F_OK) == 0 || access("/system/xbin/su", F_OK) == 0);
    UNX_LOG_INFO("Root Status : %s", isRooted ? "Rooted" : "Not Rooted");
#elif defined(__APPLE__)
    UNX_LOG_INFO("OS          : iOS / macOS");
#else
    struct utsname un;
    uname(&un);
    UNX_LOG_INFO("OS          : %s %s", un.sysname, un.release);
    UNX_LOG_INFO("Arch        : %s", un.machine);
#endif

    // CPU Info
    char cpuModel[128] = "Unknown";
#if defined(_WIN32)
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(cpuModel);
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpuModel, &size);
        RegCloseKey(hKey);
    }
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    const char* arch = "Unknown";
    switch(sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: arch = "ARM64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
        case PROCESSOR_ARCHITECTURE_ARM:   arch = "ARM"; break;
    }
    UNX_LOG_INFO("Processor   : %s", cpuModel);
    UNX_LOG_INFO("Arch        : %s", arch);
    UNX_LOG_INFO("CPU Cores   : %d", (int)sysInfo.dwNumberOfProcessors);
#else
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0 || strncmp(line, "Hardware", 8) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    strncpy(cpuModel, colon + 2, sizeof(cpuModel) - 1);
                    cpuModel[strlen(cpuModel)-1] = '\0'; // Remove newline
                    break;
                }
            }
        }
        fclose(f);
    }
    UNX_LOG_INFO("Processor   : %s", cpuModel);
    UNX_LOG_INFO("CPU Cores   : %ld", sysconf(_SC_NPROCESSORS_ONLN));
#endif

    // RAM
    float totalRAM = 0, freeRAM = 0;
#if defined(_WIN32)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        totalRAM = (float)memInfo.ullTotalPhys / (1024.0f * 1024.0f);
        freeRAM = (float)memInfo.ullAvailPhys / (1024.0f * 1024.0f);
    }
#elif defined(__APPLE__)
    int64_t total_mem = 0;
    size_t len = sizeof(total_mem);
    if (sysctlbyname("hw.memsize", &total_mem, &len, NULL, 0) == 0) {
        totalRAM = (float)total_mem / (1024.0f * 1024.0f);
    }
    vm_statistics_data_t vm_stats;
    mach_msg_type_number_t info_count = HOST_VM_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vm_stats, &info_count) == KERN_SUCCESS) {
        freeRAM = (float)vm_stats.free_count * (float)sysconf(_SC_PAGESIZE) / (1024.0f * 1024.0f);
    }
#elif defined(__ANDROID__) || defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        totalRAM = (float)((uint64_t)si.totalram * si.mem_unit) / (1024.0f * 1024.0f);
        freeRAM = (float)((uint64_t)si.freeram * si.mem_unit) / (1024.0f * 1024.0f);
    }
#endif
    UNX_LOG_INFO("Total RAM   : %.0f MB (%.2f GB)", totalRAM, totalRAM / 1024.0f);
    float freePercent = (totalRAM > 0) ? (freeRAM / totalRAM) * 100.0f : 0;
    UNX_LOG_INFO("Free RAM    : %.0f MB (%.1f%%) %s", freeRAM, freePercent, (freeRAM < 256.0f) ? "!!! LOW MEMORY !!!" : "");

    // Display
    UNX_LOG_INFO("Display     : %dx%d @ %dHz", GetScreenWidth(), GetScreenHeight(), GetMonitorRefreshRate(GetCurrentMonitor()));

    // GPU
    UNX_LOG_INFO("GPU Model   : %s", gpuModel ? gpuModel : "Unknown");

    UNX_LOG_INFO("=== END DEVICE INFO ===");
}

// --- Memory Wrappers ---
void* Log_Malloc(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        UNX_LOG_ERR("[OOM] Failed to allocate %zu bytes at %s:%d (Current RAM: %.2f MB)", size, file, line, Log_GetRAMUsage());
        Log_Flush();
    }
    return ptr;
}

void* Log_Calloc(size_t nmemb, size_t size, const char* file, int line) {
    void* ptr = calloc(nmemb, size);
    if (!ptr && nmemb > 0 && size > 0) {
        UNX_LOG_ERR("[OOM] Failed to calloc %zu bytes at %s:%d (Current RAM: %.2f MB)", nmemb * size, file, line, Log_GetRAMUsage());
        Log_Flush();
    }
    return ptr;
}

void* Log_Realloc(void* ptr, size_t size, const char* file, int line) {
    void* newPtr = realloc(ptr, size);
    if (!newPtr && size > 0) {
        UNX_LOG_ERR("[OOM] Failed to realloc %zu bytes at %s:%d (Current RAM: %.2f MB)", size, file, line, Log_GetRAMUsage());
        Log_Flush();
    }
    return newPtr;
}

void Log_Free(void* ptr) {
    free(ptr);
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
        long total_pages, rss_pages;
        if (fscanf(f, "%ld %ld", &total_pages, &rss_pages) == 2) {
            fclose(f);
            return (float)((uint64_t)rss_pages * sysconf(_SC_PAGESIZE)) / (1024.0f * 1024.0f);
        }
        fclose(f);
    }
#endif
    return 0.0f;
}

float Log_GetFreeRAM(void) {
    float freeRAM = 0;
#if defined(_WIN32)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        freeRAM = (float)memInfo.ullAvailPhys / (1024.0f * 1024.0f);
    }
#elif defined(__APPLE__)
    vm_statistics_data_t vm_stats;
    mach_msg_type_number_t info_count = HOST_VM_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vm_stats, &info_count) == KERN_SUCCESS) {
        freeRAM = (float)vm_stats.free_count * (float)sysconf(_SC_PAGESIZE) / (1024.0f * 1024.0f);
    }
#elif defined(__ANDROID__) || defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        freeRAM = (float)(si.freeram * si.mem_unit) / (1024.0f * 1024.0f);
    }
#endif
    return freeRAM;
}

bool Log_IsMemoryCritical(void) {
    return Log_GetFreeRAM() < 128.0f; // Critical below 128MB
}



