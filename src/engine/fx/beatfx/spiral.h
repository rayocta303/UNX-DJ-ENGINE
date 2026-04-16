#ifndef BEATFX_SPIRAL_H
#define BEATFX_SPIRAL_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    float currentFeedback;
    float targetFeedback;
} BeatFX_Spiral;

void Spiral_Init(BeatFX_Spiral* fx);
void Spiral_Free(BeatFX_Spiral* fx);
void Spiral_Process(BeatFX_Spiral* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate);

#endif
