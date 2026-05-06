#ifndef MEMORY_GUARD_H
#define MEMORY_GUARD_H

#include <stdbool.h>

typedef enum {
    MEM_MODE_NORMAL,   // Full features
    MEM_MODE_ECO,      // Reduced viewport, less pre-roll
    MEM_MODE_LITE,     // Minimal waveform, no dynamic loading
    MEM_MODE_CRITICAL  // Block all loading, show warning
} MemoryLevel;

void MemoryGuard_Update(void);
MemoryLevel MemoryGuard_GetLevel(void);
const char* MemoryGuard_GetStatusString(void);

#endif
