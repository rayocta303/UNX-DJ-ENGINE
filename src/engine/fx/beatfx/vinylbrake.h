#ifndef BEATFX_VINYLBRAKE_H
#define BEATFX_VINYLBRAKE_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
    float currentSpeed;
    bool breaking;
} BeatFX_VinylBrake;

void VinylBrake_Init(BeatFX_VinylBrake* fx);
void VinylBrake_Free(BeatFX_VinylBrake* fx);
void VinylBrake_Process(BeatFX_VinylBrake* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate, bool isFxOn);

#endif
