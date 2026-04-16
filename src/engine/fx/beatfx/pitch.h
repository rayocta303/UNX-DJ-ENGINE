#ifndef BEATFX_PITCH_H
#define BEATFX_PITCH_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    float phase;
} BeatFX_Pitch;

void Pitch_Init(BeatFX_Pitch* fx);
void Pitch_Free(BeatFX_Pitch* fx);
void Pitch_Process(BeatFX_Pitch* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate);

#endif
