#ifndef COLORFX_JET_H
#define COLORFX_JET_H

#include "../dsp_utils.h"

typedef struct {
    float phase;
    float lfoSpeed;
    float depth;
    DelayLine delayL;
    DelayLine delayR;
} ColorFX_Jet;

void Jet_Init(ColorFX_Jet* fx);
void Jet_Free(ColorFX_Jet* fx);
void Jet_Process(ColorFX_Jet* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate);

#endif
