#ifndef COLORFX_NOISE_H
#define COLORFX_NOISE_H

#include "../dsp_utils.h"

typedef struct {
    BiquadFilter filter;
} ColorFX_Noise;

void Noise_Init(ColorFX_Noise* fx);
void Noise_Free(ColorFX_Noise* fx);
void Noise_Process(ColorFX_Noise* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate);

#endif
