#include "trans.h"
#include <math.h>

void Trans_Init(BeatFX_Trans* fx) {
    LFO_Init(&fx->lfo);
    fx->lfo.type = LFO_SQUARE; 
}

void Trans_Free(BeatFX_Trans* fx) {
    (void)fx;
}

void Trans_Process(BeatFX_Trans* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate) {
    float freq = 1000.0f / beatMs;
    LFO_SetFreq(&fx->lfo, freq, sampleRate);
    
    // Square wave from -1 to 1 -> map to 0 or 1
    float gate = (LFO_Process(&fx->lfo) > 0.0f) ? 1.0f : 0.0f;
    
    // Level depth controls how deep the gating is (0 = fully passed, 1 = fully gated)
    float volume = 1.0f - (levelDepth * (1.0f - gate));

    *outL = inL * volume;
    *outR = inR * volume;
}
