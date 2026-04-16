#ifndef COLORFX_MANAGER_H
#define COLORFX_MANAGER_H

#include "space.h"
#include "dub_echo.h"
#include "sweep.h"
#include "noise.h"
#include "crush.h"
#include "filter.h"

typedef enum {
    COLORFX_NONE = -1,
    COLORFX_SPACE = 0,
    COLORFX_DUBECHO = 1,
    COLORFX_SWEEP = 2,
    COLORFX_NOISE = 3,
    COLORFX_CRUSH = 4,
    COLORFX_FILTER = 5
} ColorFXType;

typedef struct {
    ColorFXType activeFX;
    float colorValue; // -1.0 to 1.0 (Knob position)
    float parameter;  // 0.0 to 1.0 (Secondary adjustment)
    
    // States for all FX (Memory is pre-allocated per channel)
    ColorFX_Space space;
    ColorFX_DubEcho dubEcho;
    ColorFX_Sweep sweep;
    ColorFX_Noise noise;
    ColorFX_Crush crush;
    ColorFX_Filter filter;
} ColorFXManager;

void ColorFXManager_Init(ColorFXManager* mgr);
void ColorFXManager_Free(ColorFXManager* mgr);
void ColorFXManager_SetFX(ColorFXManager* mgr, ColorFXType type);
void ColorFXManager_Process(ColorFXManager* mgr, float* outL, float* outR, float inL, float inR, float sampleRate);

#endif
