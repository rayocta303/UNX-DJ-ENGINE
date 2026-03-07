#include "bfilter.h"
#include <math.h>

void BFilter_Init(BeatFX_BFilter* fx) {
    Biquad_Init(&fx->filterL);
    Biquad_Init(&fx->filterR);
    LFO_Init(&fx->lfo);
    fx->lfo.type = LFO_SINE;
}

void BFilter_Free(BeatFX_BFilter* fx) {
    (void)fx;
}

void BFilter_Process(BeatFX_BFilter* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate) {
    float freq = 1000.0f / beatMs;
    LFO_SetFreq(&fx->lfo, freq, sampleRate);
    float mod = LFO_Process(&fx->lfo); // -1.0 to 1.0

    // Modulate cutoff from 100Hz to 10kHz
    float cutoff = 200.0f * powf(50.0f, (mod + 1.0f) * 0.5f);

    Biquad_SetLowpass(&fx->filterL, cutoff, 1.5f + levelDepth, sampleRate);
    Biquad_SetLowpass(&fx->filterR, cutoff, 1.5f + levelDepth, sampleRate);

    float wetL = Biquad_Process(&fx->filterL, inL);
    float wetR = Biquad_Process(&fx->filterR, inR);

    // Filter FX often mixes 100% wet when fully engaged, but we use Level/Depth to crossfade
    *outL = inL * (1.0f - levelDepth) + wetL * levelDepth;
    *outR = inR * (1.0f - levelDepth) + wetR * levelDepth;
}
