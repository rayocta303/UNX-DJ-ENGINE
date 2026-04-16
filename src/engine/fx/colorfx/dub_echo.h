#ifndef COLORFX_DUBECHO_H
#define COLORFX_DUBECHO_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    BiquadFilter filterL;
    BiquadFilter filterR;
    float feedback;
    float timeDelay; // ms
} ColorFX_DubEcho;

void DubEcho_Init(ColorFX_DubEcho* fx);
void DubEcho_Free(ColorFX_DubEcho* fx);
void DubEcho_Process(ColorFX_DubEcho* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate);

#endif
