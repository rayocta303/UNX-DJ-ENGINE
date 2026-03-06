#include "noise.h"
#include <math.h>

void Noise_Init(ColorFX_Noise* fx) {
    Biquad_Init(&fx->filter);
}

void Noise_Free(ColorFX_Noise* fx) {
    // Nothing to free
}

void Noise_Process(ColorFX_Noise* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate) {
    if (colorVal == 0.0f) {
        *outL = inL; *outR = inR; return;
    }

    float absVal = fabsf(colorVal);
    float noise = WhiteNoise_Process() * absVal; // Scale noise volume

    if (colorVal < 0.0f) { // Left: Lowpass Noise
        float cutoff = 200.0f + 5000.0f * (1.0f - absVal);
        Biquad_SetLowpass(&fx->filter, cutoff, 0.707f, sampleRate);
    } else { // Right: Highpass Noise
        float cutoff = 2000.0f * powf(10.0f, absVal);
        Biquad_SetHighpass(&fx->filter, cutoff, 0.707f, sampleRate);
    }

    // Filter the noise
    noise = Biquad_Process(&fx->filter, noise);

    // Mix the filtered noise with the clean input signal
    *outL = inL + noise;
    *outR = inR + noise;
}
