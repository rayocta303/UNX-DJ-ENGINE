#include "crush.h"
#include <math.h>

void Crush_Init(ColorFX_Crush* fx) {
    Biquad_Init(&fx->lpfL); Biquad_Init(&fx->lpfR);
    Biquad_Init(&fx->hpfL); Biquad_Init(&fx->hpfR);
    fx->phase = 0.0f;
    fx->phaseInc = 0.0f;
    fx->holdL = 0.0f;
    fx->holdR = 0.0f;
}

void Crush_Free(ColorFX_Crush* fx) {
    // Nothing to free
}

void Crush_Process(ColorFX_Crush* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate) {
    if (colorVal == 0.0f) {
        *outL = inL; *outR = inR; return;
    }

    float absVal = fabsf(colorVal);

    // Target sample rate reduction (Down to ~1000Hz max crush)
    float targetHz = sampleRate - (sampleRate - 1000.0f) * absVal;
    fx->phaseInc = targetHz / sampleRate;

    fx->phase += fx->phaseInc;
    if (fx->phase >= 1.0f || fx->phaseInc >= 1.0f) {
        fx->phase -= 1.0f;
        // Sample new value
        fx->holdL = inL;
        fx->holdR = inR;

        // Bit reduction (Quantization)
        float bits = 16.0f - (14.0f * absVal); // Down to 2 bits
        float steps = powf(2.0f, bits);
        fx->holdL = floorf(fx->holdL * steps) / steps;
        fx->holdR = floorf(fx->holdR * steps) / steps;
    }

    float crushedL = fx->holdL;
    float crushedR = fx->holdR;

    if (colorVal < 0.0f) { // Left: Crush + Lowpass for darker tone
        Biquad_SetLowpass(&fx->lpfL, 500.0f + 10000.0f * (1.0f - absVal), 0.707f, sampleRate);
        Biquad_SetLowpass(&fx->lpfR, 500.0f + 10000.0f * (1.0f - absVal), 0.707f, sampleRate);
        crushedL = Biquad_Process(&fx->lpfL, crushedL);
        crushedR = Biquad_Process(&fx->lpfR, crushedR);
    } else { // Right: Crush + Highpass for harsh transient tone
        Biquad_SetHighpass(&fx->hpfL, 1000.0f * absVal, 0.707f, sampleRate);
        Biquad_SetHighpass(&fx->hpfR, 1000.0f * absVal, 0.707f, sampleRate);
        crushedL = Biquad_Process(&fx->hpfL, crushedL);
        crushedR = Biquad_Process(&fx->hpfR, crushedR);
    }

    // Blend
    *outL = inL * (1.0f - absVal) + crushedL * absVal;
    *outR = inR * (1.0f - absVal) + crushedR * absVal;
}
