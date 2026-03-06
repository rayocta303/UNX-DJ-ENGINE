#include "sweep.h"
#include <math.h>

void Sweep_Init(ColorFX_Sweep* fx) {
    Biquad_Init(&fx->bpfL);
    Biquad_Init(&fx->bpfR);
}

void Sweep_Free(ColorFX_Sweep* fx) {
    // Nothing to free
}

void Sweep_Process(ColorFX_Sweep* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate) {
    if (colorVal == 0.0f) {
        *outL = inL; *outR = inR; return;
    }

    float absVal = fabsf(colorVal);

    if (colorVal < 0.0f) { // Left: Sweep Down (Notch)
        float freq = 20000.0f * powf(0.005f, absVal); // Sweeps 20kHz down to 100Hz
        Biquad_SetNotch(&fx->bpfL, freq, 2.0f, sampleRate);
        Biquad_SetNotch(&fx->bpfR, freq, 2.0f, sampleRate);
        *outL = Biquad_Process(&fx->bpfL, inL);
        *outR = Biquad_Process(&fx->bpfR, inR);
    } else { // Right: Sweep Up (Bandpass)
        float freq = 100.0f * powf(200.0f, absVal); // Sweeps 100Hz up to 20kHz
        Biquad_SetBandpass(&fx->bpfL, freq, 1.5f, sampleRate);
        Biquad_SetBandpass(&fx->bpfR, freq, 1.5f, sampleRate);
        // Mix dry/wet for bandpass
        float wetL = Biquad_Process(&fx->bpfL, inL);
        float wetR = Biquad_Process(&fx->bpfR, inR);
        *outL = inL + wetL * 2.0f; // Boost the BP slightly
        *outR = inR + wetR * 2.0f;
    }
}
