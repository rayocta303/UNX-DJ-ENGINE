#ifndef COLORFX_SPACE_H
#define COLORFX_SPACE_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL[4];
    DelayLine delayR[4];
    BiquadFilter lpfL, lpfR, hpfL, hpfR;
    float feedback;
} ColorFX_Space;

void Space_Init(ColorFX_Space* fx);
void Space_Free(ColorFX_Space* fx);
void Space_Process(ColorFX_Space* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate);

#endif
