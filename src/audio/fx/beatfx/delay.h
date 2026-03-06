#ifndef BEATFX_DELAY_H
#define BEATFX_DELAY_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
} BeatFX_Delay;

void Delay_Init(BeatFX_Delay* fx);
void Delay_Free(BeatFX_Delay* fx);
void Delay_Process(BeatFX_Delay* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate);

#endif
