#include "core/system_info.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#ifdef __ANDROID__
#include <jni.h>
// Note: android_app must be accessible or GetAndroidApp() must be available
// For this implementation, we assume we can link against Raylib's internal state or use a setter
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
extern float ios_get_battery_level(void);
extern bool ios_is_battery_charging(void);
#endif
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors = 0;
static HANDLE self = NULL;
static bool initialized = false;

static void InitSystemInfo() {
    SYSTEM_INFO sysInfo;
    FILETIME ftime, fsys, fuser;

    GetSystemInfo(&sysInfo);
    numProcessors = (int)sysInfo.dwNumberOfProcessors;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastCPU, &ftime, sizeof(FILETIME));

    self = GetCurrentProcess();
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
    memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
    
    initialized = true;
}

SystemStats GetSystemStats() {
    SystemStats stats = {0};
    if (!initialized) InitSystemInfo();

    // CPU Usage
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    double percent;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));

    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));

    if (now.QuadPart > lastCPU.QuadPart) {
        percent = (double)((sys.QuadPart - lastSysCPU.QuadPart) +
                           (user.QuadPart - lastUserCPU.QuadPart));
        percent /= (now.QuadPart - lastCPU.QuadPart);
        percent /= numProcessors;
        
        lastCPU = now;
        lastSysCPU = sys;
        lastUserCPU = user;
        
        if (percent < 0) percent = 0;
        if (percent > 1.0) percent = 1.0;
        stats.cpuUsage = (float)percent;
    } else {
        stats.cpuUsage = 0.0f;
    }

    // RAM Usage
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(self, &pmc, sizeof(pmc))) {
        stats.ramUsageMB = (float)pmc.WorkingSetSize / (1024.0f * 1024.0f);
    }
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        stats.ramTotalMB = (float)memInfo.ullTotalPhys / (1024.0f * 1024.0f);
    }
    
    // Battery Info
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryLifePercent != 255) {
            stats.batteryLevel = (float)sps.BatteryLifePercent / 100.0f;
        } else {
            stats.batteryLevel = -1.0f; // No battery
        }
        stats.isCharging = (sps.ACLineStatus == 1);
    }

    return stats;
}
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <stdio.h>

SystemStats GetSystemStats() {
    SystemStats stats = {0};
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    
    stats.ramTotalMB = (float)(memInfo.totalram * memInfo.mem_unit) / (1024.0f * 1024.0f);
    
    // Very simplified Linux CPU usage (reading /proc/stat would be better but complex for a quick fix)
    FILE* file = fopen("/proc/loadavg", "r");
    if (file) {
        float load;
        if (fscanf(file, "%f", &load) > 0) {
            stats.cpuUsage = load / 4.0f; // Assume 4 cores for normalized display
            if (stats.cpuUsage > 1.0f) stats.cpuUsage = 1.0f;
        }
        fclose(file);
    }
    
    // RAM usage (self)
    file = fopen("/proc/self/status", "r");
    if (file) {
        char line[128];
        while (fgets(line, 128, file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                int rss;
                sscanf(line + 6, "%d", &rss);
                stats.ramUsageMB = (float)rss / 1024.0f;
                break;
            }
        }
        fclose(file);
    }

    // Battery Info
    FILE* bfile = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (!bfile) bfile = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (bfile) {
        int cap;
        if (fscanf(bfile, "%d", &cap) > 0) {
            stats.batteryLevel = (float)cap / 100.0f;
        }
        fclose(bfile);
        
        bfile = fopen("/sys/class/power_supply/BAT0/status", "r");
        if (!bfile) bfile = fopen("/sys/class/power_supply/battery/status", "r");
        if (bfile) {
            char status[32];
            if (fscanf(bfile, "%31s", status) > 0) {
                stats.isCharging = (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0);
            }
            fclose(bfile);
        }
    } else {
        stats.batteryLevel = -1.0f;
    }

    return stats;
}
#elif defined(__ANDROID__)
#include <android/native_app_glue.h>

// Forward declaration if not using a global header
extern struct android_app *GetAndroidApp(void);

SystemStats GetSystemStats() {
    SystemStats stats = {0.05f, 150.0f, 8192.0f, -1.0f, false};
    struct android_app *app = GetAndroidApp();

    if (app && app->activity) {
        JavaVM *vm = app->activity->vm;
        JNIEnv *env = NULL;
        (*vm)->AttachCurrentThread(vm, &env, NULL);

        jobject activity = app->activity->clazz;
        jclass cls = (*env)->GetObjectClass(env, activity);

        jmethodID midLvl = (*env)->GetMethodID(env, cls, "getBatteryLevel", "()F");
        if (midLvl)
            stats.batteryLevel = (*env)->CallFloatMethod(env, activity, midLvl);

        jmethodID midChg = (*env)->GetMethodID(env, cls, "isBatteryCharging", "()Z");
        if (midChg)
            stats.isCharging = (*env)->CallBooleanMethod(env, activity, midChg);

        (*vm)->DetachCurrentThread(vm);
    }
    return stats;
}
#elif defined(__APPLE__) && TARGET_OS_IPHONE
SystemStats GetSystemStats() {
    SystemStats stats = {0.05f, 150.0f, 8192.0f, -1.0f, false};
    stats.batteryLevel = ios_get_battery_level();
    stats.isCharging = ios_is_battery_charging();
    return stats;
}
#else
SystemStats GetSystemStats() {
    SystemStats stats = {0.05f, 150.0f, 8192.0f, -1.0f, false};
    return stats;
}
#endif
