#include "space.h"
#include <math.h>

void Space_Init(ColorFX_Space* fx) {
    // 4 parallel delays for pseudo-reverb
    DelayLine_Init(&fx->delayL[0], 2000); // prime numbers 
    DelayLine_Init(&fx->delayL[1], 3001);
    DelayLine_Init(&fx->delayL[2], 4111);
    DelayLine_Init(&fx->delayL[3], 5227);
    DelayLine_Init(&fx->delayR[0], 2111);
    DelayLine_Init(&fx->delayR[1], 3181);
    DelayLine_Init(&fx->delayR[2], 4217);
    DelayLine_Init(&fx->delayR[3], 5323);
    Biquad_Init(&fx->lpfL); Biquad_Init(&fx->lpfR);
    Biquad_Init(&fx->hpfL); Biquad_Init(&fx->hpfR);
    fx->feedback = 0.0f;
}

void Space_Free(ColorFX_Space* fx) {
    for (int i=0; i<4; i++) {
        DelayLine_Free(&fx->delayL[i]);
        DelayLine_Free(&fx->delayR[i]);
    }
}

void Space_Process(ColorFX_Space* fx, float* outL, float* outR, float inL, float inR, float colorVal, float sampleRate) {
    if (colorVal == 0.0f) {
        *outL = inL; *outR = inR; return;
    }

    // colorVal < 0 = lowpass + short decay
    // colorVal > 0 = highpass + long decay
    float absVal = fabsf(colorVal);
    float decay = (colorVal < 0.0f) ? 0.4f + 0.5f * absVal : 0.5f + 0.45f * absVal;
    
    // Filters based on knob side
    if (colorVal < 0.0f) { // Left: Darker
        Biquad_SetLowpass(&fx->lpfL, 500.0f + 3000.0f * (1.0f - absVal), 0.707f, sampleRate);
        Biquad_SetLowpass(&fx->lpfR, 500.0f + 3000.0f * (1.0f - absVal), 0.707f, sampleRate);
    } else { // Right: Brighter
        Biquad_SetHighpass(&fx->hpfL, 1500.0f * absVal, 0.707f, sampleRate);
        Biquad_SetHighpass(&fx->hpfR, 1500.0f * absVal, 0.707f, sampleRate);
    }

    float wetL = 0, wetR = 0;

    for (int i=0; i<4; i++) {
        float dl = DelayLine_Read(&fx->delayL[i], fx->delayL[i].length - 1);
        float dr = DelayLine_Read(&fx->delayR[i], fx->delayR[i].length - 1);

        // Simple feedback matrix (Householder approx)
        wetL += dl;
        wetR += dr;

        float wl = inL + dl * decay;
        float wr = inR + dr * decay;

        DelayLine_Write(&fx->delayL[i], wl);
        DelayLine_Write(&fx->delayR[i], wr);
    }

    wetL /= 4.0f;
    wetR /= 4.0f;

    if (colorVal < 0.0f) {
        wetL = Biquad_Process(&fx->lpfL, wetL);
        wetR = Biquad_Process(&fx->lpfR, wetR);
    } else {
        wetL = Biquad_Process(&fx->hpfL, wetL);
        wetR = Biquad_Process(&fx->hpfR, wetR);
    }

    *outL = inL + wetL * absVal;
    *outR = inR + wetR * absVal;
}
