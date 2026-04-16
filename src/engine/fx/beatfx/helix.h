#ifndef BEATFX_HELIX_H
#define BEATFX_HELIX_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    float currentMs;
    bool holding;
} BeatFX_Helix;

void Helix_Init(BeatFX_Helix* fx);
void Helix_Free(BeatFX_Helix* fx);
void Helix_Process(BeatFX_Helix* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate, bool isFxOn);

#endif
