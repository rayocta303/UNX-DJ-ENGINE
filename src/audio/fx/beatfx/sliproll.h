#ifndef BEATFX_SLIPROLL_H
#define BEATFX_SLIPROLL_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    float currentMs;
    bool holding;
} BeatFX_SlipRoll;

void SlipRoll_Init(BeatFX_SlipRoll* fx);
void SlipRoll_Free(BeatFX_SlipRoll* fx);
void SlipRoll_Process(BeatFX_SlipRoll* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate, bool isFxOn);

#endif
