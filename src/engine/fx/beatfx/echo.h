#ifndef BEATFX_ECHO_H
#define BEATFX_ECHO_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    float lastOutL;
    float lastOutR;
} BeatFX_Echo;

void Echo_Init(BeatFX_Echo* fx);
void Echo_Free(BeatFX_Echo* fx);
void Echo_Process(BeatFX_Echo* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate);

#endif
