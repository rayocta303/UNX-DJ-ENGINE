#ifndef COLORFX_GATE_H
#define COLORFX_GATE_H

#include "../dsp_utils.h"

typedef struct {
    float threshold;
    float attack;
    float release;
    float envelope;
} ColorFX_Gate;

void Gate_Init(ColorFX_Gate* fx);
void Gate_Process(ColorFX_Gate* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate);

#endif
