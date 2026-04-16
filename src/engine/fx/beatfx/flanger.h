#ifndef BEATFX_FLANGER_H
#define BEATFX_FLANGER_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    LFO lfo;
} BeatFX_Flanger;

void Flanger_Init(BeatFX_Flanger* fx);
void Flanger_Free(BeatFX_Flanger* fx);
void Flanger_Process(BeatFX_Flanger* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float scrubVal, bool isScrubbing, float sampleRate);

#endif
