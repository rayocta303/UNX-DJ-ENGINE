#ifndef COLORFX_SWEEP_H
#define COLORFX_SWEEP_H

#include "../dsp_utils.h"

typedef struct {
    BiquadFilter bpfL;
    BiquadFilter bpfR;
} ColorFX_Sweep;

void Sweep_Init(ColorFX_Sweep* fx);
void Sweep_Free(ColorFX_Sweep* fx);
void Sweep_Process(ColorFX_Sweep* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate);

#endif
