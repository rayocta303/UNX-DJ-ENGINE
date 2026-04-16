#include "dub_echo.h"
#include <math.h>

void DubEcho_Init(ColorFX_DubEcho* fx) {
    // 3/4 beat at 120BPM = ~375ms. Let's make it a fixed 375ms tape delay
    DelayLine_Init(&fx->delayL, 44100); 
    DelayLine_Init(&fx->delayR, 44100);
    Biquad_Init(&fx->filterL);
    Biquad_Init(&fx->filterR);
    fx->feedback = 0.0f;
    fx->timeDelay = 375.0f; 
}

void DubEcho_Free(ColorFX_DubEcho* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void DubEcho_Process(ColorFX_DubEcho* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate) {
    if (colorVal == 0.0f) {
        *outL = inL; *outR = inR; return;
    }

    float absVal = fabsf(colorVal);
    fx->feedback = 0.5f + 0.45f * absVal; // High feedback

    // Filters based on knob side
    if (colorVal < 0.0f) { // Left: Dub Echo (Lowpass)
        Biquad_SetLowpass(&fx->filterL, 400.0f + 2000.0f * (1.0f - absVal), 0.707f, sampleRate);
        Biquad_SetLowpass(&fx->filterR, 400.0f + 2000.0f * (1.0f - absVal), 0.707f, sampleRate);
    } else { // Right: Tape Echo (Highpass)
        Biquad_SetHighpass(&fx->filterL, 1000.0f * absVal, 0.707f, sampleRate);
        Biquad_SetHighpass(&fx->filterR, 1000.0f * absVal, 0.707f, sampleRate);
    }

    float delaySamples = (fx->timeDelay / 1000.0f) * sampleRate;

    float dl = DelayLine_Read(&fx->delayL, delaySamples);
    float dr = DelayLine_Read(&fx->delayR, delaySamples);

    dl = Biquad_Process(&fx->filterL, dl);
    dr = Biquad_Process(&fx->filterR, dr);

    float wl = inL + dl * fx->feedback;
    float wr = inR + dr * fx->feedback;

    // Saturation (Tape sim)
    wl = tanhf(wl);
    wr = tanhf(wr);

    DelayLine_Write(&fx->delayL, wl);
    DelayLine_Write(&fx->delayR, wr);

    *outL = inL + dl * absVal;
    *outR = inR + dr * absVal;
}
