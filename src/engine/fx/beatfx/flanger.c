#include "flanger.h"
#include <math.h>

void Flanger_Init(BeatFX_Flanger* fx) {
    DelayLine_Init(&fx->delayL, 2048); // Short delay for flanger
    DelayLine_Init(&fx->delayR, 2048);
    LFO_Init(&fx->lfo);
    fx->lfo.type = LFO_TRIANGLE;
}

void Flanger_Free(BeatFX_Flanger* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void Flanger_Process(BeatFX_Flanger* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float scrubVal, bool isScrubbing, float sampleRate) {
    // Flanger sweep time (beatMs) determines LFO frequency
    float freq = 1000.0f / beatMs;
    LFO_SetFreq(&fx->lfo, freq, sampleRate);

    float mod = 0.0f;
    if (isScrubbing) {
        // Manual scrub overrides LFO
        mod = scrubVal; // -1 to 1
    } else {
        mod = LFO_Process(&fx->lfo);
    }

    // Delay time modulates between 1ms and 10ms
    float baseDelayMs = 5.5f;
    float depthMs = 4.5f;
    float currentDelayMs = baseDelayMs + (mod * depthMs);
    float delaySamples = (currentDelayMs / 1000.0f) * sampleRate;

    float dl = DelayLine_Read(&fx->delayL, delaySamples);
    float dr = DelayLine_Read(&fx->delayR, delaySamples);

    float feedback = 0.7f; // High feedback for jet-plane sound
    DelayLine_Write(&fx->delayL, inL + dl * feedback);
    DelayLine_Write(&fx->delayR, inR + dr * feedback);

    *outL = inL + dl * levelDepth;
    *outR = inR + dr * levelDepth;
}
