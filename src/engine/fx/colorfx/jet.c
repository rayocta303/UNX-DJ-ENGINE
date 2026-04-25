#include "jet.h"
#include <math.h>

void Jet_Init(ColorFX_Jet* fx) {
    fx->phase = 0.0f;
    fx->lfoSpeed = 0.1f; // Slow jet sweep
    fx->depth = 0.002f;  // 2ms depth
    DelayLine_Init(&fx->delayL, 44100 / 10); // 100ms max buffer
    DelayLine_Init(&fx->delayR, 44100 / 10);
}

void Jet_Free(ColorFX_Jet* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void Jet_Process(ColorFX_Jet* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate) {
    if (colorVal == 0.0f) {
        *outL = inL; *outR = inR; return;
    }

    float absVal = fabsf(colorVal);
    
    // LFO: Sinusoidal sweep
    fx->phase += (fx->lfoSpeed / sampleRate) * 2.0f * M_PI;
    if (fx->phase > 2.0f * M_PI) fx->phase -= 2.0f * M_PI;

    float lfo = (sinf(fx->phase) + 1.0f) * 0.5f; // 0.0 to 1.0
    
    // Delay time: 0.1ms to 5ms based on LFO and knob
    float delayMs = 0.1f + lfo * 5.0f * absVal;
    float delaySamples = (delayMs / 1000.0f) * sampleRate;

    float dl = DelayLine_Read(&fx->delayL, delaySamples);
    float dr = DelayLine_Read(&fx->delayR, delaySamples);

    DelayLine_Write(&fx->delayL, inL);
    DelayLine_Write(&fx->delayR, inR);

    // Jet usually has negative feedback for that "whoosh" hollow sound
    float feedback = -0.7f * absVal;
    
    // Mix: Dry + Wet (Flanging)
    *outL = inL + dl * 0.7f * absVal;
    *outR = inR + dr * 0.7f * absVal;
}
