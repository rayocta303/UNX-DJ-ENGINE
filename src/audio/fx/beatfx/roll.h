#ifndef BEATFX_ROLL_H
#define BEATFX_ROLL_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    float currentMs;
    // For Roll, we capture the audio into the buffer and loop it.
    bool holding;
} BeatFX_Roll;

void Roll_Init(BeatFX_Roll* fx);
void Roll_Free(BeatFX_Roll* fx);
void Roll_Process(BeatFX_Roll* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate, bool isFxOn);

#endif
