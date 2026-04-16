#ifndef COLORFX_CRUSH_H
#define COLORFX_CRUSH_H

#include "../dsp_utils.h"

typedef struct {
    BiquadFilter lpfL, lpfR, hpfL, hpfR;
    float phase;
    float phaseInc;
    float holdL;
    float holdR;
} ColorFX_Crush;

void Crush_Init(ColorFX_Crush* fx);
void Crush_Free(ColorFX_Crush* fx);
void Crush_Process(ColorFX_Crush* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate);

#endif
