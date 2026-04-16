#include "delay.h"

void Delay_Init(BeatFX_Delay* fx) {
    DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
    DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
}

void Delay_Free(BeatFX_Delay* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void Delay_Process(BeatFX_Delay* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate) {
    float delaySamples = (beatMs / 1000.0f) * sampleRate;
    
    // Read 
    float dl = DelayLine_Read(&fx->delayL, delaySamples);
    float dr = DelayLine_Read(&fx->delayR, delaySamples);

    // Single repeat (No feedback)
    DelayLine_Write(&fx->delayL, inL);
    DelayLine_Write(&fx->delayR, inR);

    *outL = inL + dl * levelDepth;
    *outR = inR + dr * levelDepth;
}
