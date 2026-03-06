#include "pingpong.h"

void PingPong_Init(BeatFX_PingPong* fx) {
    DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
    DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
}

void PingPong_Free(BeatFX_PingPong* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void PingPong_Process(BeatFX_PingPong* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate) {
    float delaySamples = (beatMs / 1000.0f) * sampleRate;
    
    // Read 
    float dl = DelayLine_Read(&fx->delayL, delaySamples);
    float dr = DelayLine_Read(&fx->delayR, delaySamples);

    float feedback = levelDepth * 0.85f;

    // Cross feedback for Ping Pong effect
    DelayLine_Write(&fx->delayL, inL + dr * feedback);
    DelayLine_Write(&fx->delayR, inR + dl * feedback);

    *outL = inL + dl * levelDepth;
    *outR = inR + dr * levelDepth;
}
