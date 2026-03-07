#include "phaser.h"
#include <math.h>

void Phaser_Init(BeatFX_Phaser* fx) {
    for (int i=0; i<4; i++) {
        Biquad_Init(&fx->apfL[i]);
        Biquad_Init(&fx->apfR[i]);
    }
    LFO_Init(&fx->lfo);
    fx->lfo.type = LFO_SINE;
    fx->feedback = 0.0f;
}

void Phaser_Free(BeatFX_Phaser* fx) {
    (void)fx;
}

static inline void setAllpass(BiquadFilter* filter, float param, float sampleRate) {
    // Approximate an all-pass sweep
    float freq = 200.0f * powf(40.0f, param); // Sweeps between 200Hz to 8kHz
    float w0 = 2.0f * 3.14159f * freq / sampleRate;
    float alpha = sinf(w0) / (2.0f * 0.707f);
    float cs = cosf(w0);

    float a0 = 1.0f + alpha;
    filter->b0 = (1.0f - alpha) / a0;
    filter->b1 = -2.0f * cs / a0;
    filter->b2 = (1.0f + alpha) / a0;
    filter->a1 = -2.0f * cs / a0;
    filter->a2 = (1.0f - alpha) / a0;
}

void Phaser_Process(BeatFX_Phaser* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate) {
    float freq = 1000.0f / beatMs;
    LFO_SetFreq(&fx->lfo, freq, sampleRate);
    float mod = (LFO_Process(&fx->lfo) + 1.0f) * 0.5f; // 0.0 to 1.0

    for (int i=0; i<4; i++) {
        setAllpass(&fx->apfL[i], mod, sampleRate);
        setAllpass(&fx->apfR[i], mod, sampleRate);
    }

    float wetL = inL + fx->feedback;
    float wetR = inR + fx->feedback;

    for (int i=0; i<4; i++) {
        wetL = Biquad_Process(&fx->apfL[i], wetL);
        wetR = Biquad_Process(&fx->apfR[i], wetR);
    }

    // Heavy feedback
    fx->feedback = wetL * 0.5f * levelDepth;

    *outL = inL + wetL * levelDepth;
    *outR = inR + wetR * levelDepth;
}
