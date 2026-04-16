#ifndef COLORFX_FILTER_H
#define COLORFX_FILTER_H

#include "../dsp_utils.h"

typedef struct {
    BiquadFilter lpfL, lpfR, hpfL, hpfR;
} ColorFX_Filter;

void Filter_Init(ColorFX_Filter* fx);
void Filter_Free(ColorFX_Filter* fx);
void Filter_Process(ColorFX_Filter* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate);

#endif
