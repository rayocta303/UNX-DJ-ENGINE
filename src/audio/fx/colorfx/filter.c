#include "filter.h"
#include <math.h>

void Filter_Init(ColorFX_Filter* fx) {
    Biquad_Init(&fx->lpfL); Biquad_Init(&fx->lpfR);
    Biquad_Init(&fx->hpfL); Biquad_Init(&fx->hpfR);
}

void Filter_Free(ColorFX_Filter* fx) {
    (void)fx;
}

void Filter_Process(ColorFX_Filter* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate) {
    if (colorVal == 0.0f) {
        *outL = inL; *outR = inR; return;
    }

    float absVal = fabsf(colorVal);

    if (colorVal < 0.0f) { // Left: LPF
        // Sweep from 20kHz down to ~150Hz
        float freq = 20000.0f * powf(0.0075f, absVal);
        Biquad_SetLowpass(&fx->lpfL, freq, 1.5f + (1.0f * absVal), sampleRate); // Increase Q for resonance
        Biquad_SetLowpass(&fx->lpfR, freq, 1.5f + (1.0f * absVal), sampleRate);
        *outL = Biquad_Process(&fx->lpfL, inL);
        *outR = Biquad_Process(&fx->lpfR, inR);
    } else { // Right: HPF
        // Sweep from 20Hz up to 10kHz
        float freq = 20.0f * powf(500.0f, absVal);
        Biquad_SetHighpass(&fx->hpfL, freq, 1.5f + (1.0f * absVal), sampleRate);
        Biquad_SetHighpass(&fx->hpfR, freq, 1.5f + (1.0f * absVal), sampleRate);
        *outL = Biquad_Process(&fx->hpfL, inL);
        *outR = Biquad_Process(&fx->hpfR, inR);
    } // Soft clipping for resonance containment
    *outL = tanhf(*outL);
    *outR = tanhf(*outR);
}
