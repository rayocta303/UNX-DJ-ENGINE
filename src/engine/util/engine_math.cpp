#include "engine_math.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Ported from Engine enginebufferscale.cpp
float Engine_InterpolateHermite4(float frac_pos, float xm1, float x0, float x1, float x2) {
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    return ((((a * frac_pos) - b_neg) * frac_pos + c) * frac_pos + x0);
}

// Internal Helper: Standard Biquad Coefficients (Butterworth)
static void Engine_CalcButterworthCoeffs(EngineBiquad* b, float freq, float sampleRate, bool highpass) {
    float omega = 2.0f * (float)M_PI * freq / sampleRate;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / (2.0f * 0.70710678118f); // Q = 1/sqrt(2)

    float a0 = 1.0f + alpha;
    if (highpass) {
        b->b0 = (1.0f + cs) / 2.0f / a0;
        b->b1 = -(1.0f + cs) / a0;
        b->b2 = (1.0f + cs) / 2.0f / a0;
    } else {
        b->b0 = (1.0f - cs) / 2.0f / a0;
        b->b1 = (1.0f - cs) / a0;
        b->b2 = (1.0f - cs) / 2.0f / a0;
    }
    b->a1 = -2.0f * cs / a0;
    b->a2 = (1.0f - alpha) / a0;
}

void EngineLR4_Init(EngineLR4* filter) {
    memset(filter, 0, sizeof(EngineLR4));
}

void EngineLR4_SetLowpass(EngineLR4* filter, float freq, float sampleRate) {
    Engine_CalcButterworthCoeffs(&filter->stages[0], freq, sampleRate, false);
    Engine_CalcButterworthCoeffs(&filter->stages[1], freq, sampleRate, false);
}

void EngineLR4_SetHighpass(EngineLR4* filter, float freq, float sampleRate) {
    Engine_CalcButterworthCoeffs(&filter->stages[0], freq, sampleRate, true);
    Engine_CalcButterworthCoeffs(&filter->stages[1], freq, sampleRate, true);
}

float EngineLR4_Process(EngineLR4* filter, float in) {
    float out = in;
    for(int i=0; i<2; i++) {
        float next = filter->stages[i].b0 * out + filter->stages[i].b1 * filter->stages[i].x1 + filter->stages[i].b2 * filter->stages[i].x2 
                     - filter->stages[i].a1 * filter->stages[i].y1 - filter->stages[i].a2 * filter->stages[i].y2;
        
        // Denormal protection
        if (fabsf(next) < 1e-18f) next = 0.0f;

        filter->stages[i].x2 = filter->stages[i].x1;
        filter->stages[i].x1 = out;
        filter->stages[i].y2 = filter->stages[i].y1;
        filter->stages[i].y1 = next;
        out = next;
    }
    return out;
}

float Engine_GetCrossfaderGain(float crossfader, int deckIdx) {
    // Constant power curve: gain = cos(x * PI/2)
    // crossfader is -1.0 (Left/DeckA) to 1.0 (Right/DeckB)
    if (deckIdx == 0) { // Deck A
        float x = (crossfader + 1.0f) * 0.5f; // -1->0, 1->1
        if (x < 0.0f) x = 0.0f;
        if (x > 1.0f) x = 1.0f;
        return cosf(x * (float)M_PI * 0.5f);
    } else { // Deck B
        float x = (1.0f - crossfader) * 0.5f; // 1->0, -1->1
        if (x < 0.0f) x = 0.0f;
        if (x > 1.0f) x = 1.0f;
        return cosf(x * (float)M_PI * 0.5f);
    }
}
