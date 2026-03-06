#ifndef BEATFX_PHASER_H
#define BEATFX_PHASER_H

#include "../dsp_utils.h"

typedef struct {
    BiquadFilter apfL[4];
    BiquadFilter apfR[4];
    LFO lfo;
    float feedback;
} BeatFX_Phaser;

void Phaser_Init(BeatFX_Phaser* fx);
void Phaser_Free(BeatFX_Phaser* fx);
void Phaser_Process(BeatFX_Phaser* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate);

#endif
