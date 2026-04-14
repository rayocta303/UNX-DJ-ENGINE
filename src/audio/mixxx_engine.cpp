#include "mixxx_engine.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Ported from Mixxx enginebufferscale.cpp
float Mixxx_InterpolateHermite4(float frac_pos, float xm1, float x0, float x1, float x2) {
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    return ((((a * frac_pos) - b_neg) * frac_pos + c) * frac_pos + x0);
}

// Internal Helper: Standard Biquad Coefficients (Butterworth)
static void Mixxx_CalcButterworthCoeffs(MixxxBiquad* b, float freq, float sampleRate, bool highpass) {
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

void MixxxLR4_Init(MixxxLR4* filter) {
    memset(filter, 0, sizeof(MixxxLR4));
}

void MixxxLR4_SetLowpass(MixxxLR4* filter, float freq, float sampleRate) {
    Mixxx_CalcButterworthCoeffs(&filter->stages[0], freq, sampleRate, false);
    Mixxx_CalcButterworthCoeffs(&filter->stages[1], freq, sampleRate, false);
}

void MixxxLR4_SetHighpass(MixxxLR4* filter, float freq, float sampleRate) {
    Mixxx_CalcButterworthCoeffs(&filter->stages[0], freq, sampleRate, true);
    Mixxx_CalcButterworthCoeffs(&filter->stages[1], freq, sampleRate, true);
}

float MixxxLR4_Process(MixxxLR4* filter, float in) {
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
