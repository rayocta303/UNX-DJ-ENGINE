#ifndef BEATFX_PINGPONG_H
#define BEATFX_PINGPONG_H

#include "../dsp_utils.h"

typedef struct {
    DelayLine delayL;
    DelayLine delayR;
} BeatFX_PingPong;

void PingPong_Init(BeatFX_PingPong* fx);
void PingPong_Free(BeatFX_PingPong* fx);
void PingPong_Process(BeatFX_PingPong* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate);

#endif
