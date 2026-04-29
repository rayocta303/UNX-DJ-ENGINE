#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <stdbool.h>

typedef struct {
    float cpuUsage; // 0.0 to 1.0
    float ramUsageMB;
    float ramTotalMB;
    float batteryLevel; // 0.0 to 1.0, -1.0 if no battery
    bool isCharging;
} SystemStats;

SystemStats GetSystemStats(void);

#endif
