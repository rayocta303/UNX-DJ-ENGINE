#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <stdbool.h>

typedef struct {
    float cpuUsage;   // 0.0 to 1.0
    int   cpuCores;   // e.g. 4, 8 cores
    float cpuMhz;     // Clock speed in MHz (e.g. 2400.0)
    float ramUsageMB; // Global System RAM
    float ramAppMB;   // App Process RAM
    float ramTotalMB;
    float ramFreeMB;
    float batteryLevel; // 0.0 to 1.0, -1.0 if no battery
    bool isCharging;
} SystemStats;

SystemStats GetSystemStats(void);
void System_ShowKeyboard(bool show);

#endif
