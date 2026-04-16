#ifndef BEATFX_REVERB_H
#define BEATFX_REVERB_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL[4];
    DelayLine delayR[4];
    BiquadFilter lpfL, lpfR, hpfL, hpfR;
} BeatFX_Reverb;

void Reverb_Init(BeatFX_Reverb* fx);
void Reverb_Free(BeatFX_Reverb* fx);
void Reverb_Process(BeatFX_Reverb* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float scrubVal, float sampleRate);

#endif
