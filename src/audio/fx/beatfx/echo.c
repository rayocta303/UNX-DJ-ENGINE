#include "echo.h"

void Echo_Init(BeatFX_Echo* fx) {
    DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
    DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
}

void Echo_Free(BeatFX_Echo* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void Echo_Process(BeatFX_Echo* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate) {
    float delaySamples = (beatMs / 1000.0f) * sampleRate;
    
    // Read 
    float dl = DelayLine_Read(&fx->delayL, delaySamples);
    float dr = DelayLine_Read(&fx->delayR, delaySamples);

    // Feedback decay scales with Level/Depth. Max ~0.9
    float feedback = levelDepth * 0.9f;

    // Write back with feedback
    DelayLine_Write(&fx->delayL, inL + dl * feedback);
    DelayLine_Write(&fx->delayR, inR + dr * feedback);

    // XDJ-style echo is an insert effect that trails off when turned off,
    // but the levelDepth mainly controls the dry/wet of the first repeat and feedback
    *outL = inL + dl * levelDepth;
    *outR = inR + dr * levelDepth;
}
