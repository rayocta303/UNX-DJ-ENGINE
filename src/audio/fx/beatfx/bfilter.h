#ifndef BEATFX_BFILTER_H
#define BEATFX_BFILTER_H

#include "../dsp_utils.h"

typedef struct {
    BiquadFilter filterL;
    BiquadFilter filterR;
    LFO lfo;
} BeatFX_BFilter;

void BFilter_Init(BeatFX_BFilter* fx);
void BFilter_Free(BeatFX_BFilter* fx);
void BFilter_Process(BeatFX_BFilter* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate);

#endif
