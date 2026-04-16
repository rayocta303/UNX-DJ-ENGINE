#ifndef BEATFX_TRANS_H
#define BEATFX_TRANS_H

#include "../dsp_utils.h"

typedef struct {
    LFO lfo;
} BeatFX_Trans;

void Trans_Init(BeatFX_Trans* fx);
void Trans_Free(BeatFX_Trans* fx);
void Trans_Process(BeatFX_Trans* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate);

#endif
