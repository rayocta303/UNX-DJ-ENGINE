#include "memory_guard.h"
#include "logger.h"

static MemoryLevel g_currentLevel = MEM_MODE_NORMAL;

void MemoryGuard_Update(void) {
    float freeMB = Log_GetFreeRAM();
    
    // Thresholds tuned for devices with 2GB-4GB RAM (Android)
    if (freeMB < 80.0f) {
        g_currentLevel = MEM_MODE_CRITICAL;
    } else if (freeMB < 160.0f) {
        g_currentLevel = MEM_MODE_LITE;
    } else if (freeMB < 350.0f) {
        g_currentLevel = MEM_MODE_ECO;
    } else {
        g_currentLevel = MEM_MODE_NORMAL;
    }
}

MemoryLevel MemoryGuard_GetLevel(void) {
    return g_currentLevel;
}

const char* MemoryGuard_GetStatusString(void) {
    switch (g_currentLevel) {
        case MEM_MODE_NORMAL:   return "NORMAL";
        case MEM_MODE_ECO:      return "ECO MODE";
        case MEM_MODE_LITE:     return "LITE MODE";
        case MEM_MODE_CRITICAL: return "LOW MEMORY";
        default:                return "UNKNOWN";
    }
}
