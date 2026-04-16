#include "reverb.h"
#include <math.h>

void Reverb_Init(BeatFX_Reverb* fx) {
    DelayLine_Init(&fx->delayL[0], 2000); 
    DelayLine_Init(&fx->delayL[1], 3001);
    DelayLine_Init(&fx->delayL[2], 4111);
    DelayLine_Init(&fx->delayL[3], 5227);
    DelayLine_Init(&fx->delayR[0], 2111);
    DelayLine_Init(&fx->delayR[1], 3181);
    DelayLine_Init(&fx->delayR[2], 4217);
    DelayLine_Init(&fx->delayR[3], 5323);
    Biquad_Init(&fx->lpfL); Biquad_Init(&fx->lpfR);
    Biquad_Init(&fx->hpfL); Biquad_Init(&fx->hpfR);
}

void Reverb_Free(BeatFX_Reverb* fx) {
    for (int i=0; i<4; i++) {
        DelayLine_Free(&fx->delayL[i]);
        DelayLine_Free(&fx->delayR[i]);
    }
}

void Reverb_Process(BeatFX_Reverb* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float scrubVal, float sampleRate) {
    // Room size (decay) governed by beat selection (beatMs)
    float decay = 0.5f + (beatMs / 4000.0f) * 0.45f; // Up to 0.95 feedback
    if (decay > 0.98f) decay = 0.98f;

    float wetL = 0, wetR = 0;

    for (int i=0; i<4; i++) {
        float dl = DelayLine_Read(&fx->delayL[i], fx->delayL[i].length - 1);
        float dr = DelayLine_Read(&fx->delayR[i], fx->delayR[i].length - 1);

        wetL += dl;
        wetR += dr;

        DelayLine_Write(&fx->delayL[i], inL + dl * decay);
        DelayLine_Write(&fx->delayR[i], inR + dr * decay);
    }

    wetL /= 4.0f;
    wetR /= 4.0f;

    // Filter processing via X-PAD scrub
    if (scrubVal < -0.05f) { // LPF
        float cutoff = 20000.0f * powf(0.01f, fabsf(scrubVal));
        Biquad_SetLowpass(&fx->lpfL, cutoff, 0.707f, sampleRate);
        Biquad_SetLowpass(&fx->lpfR, cutoff, 0.707f, sampleRate);
        wetL = Biquad_Process(&fx->lpfL, wetL);
        wetR = Biquad_Process(&fx->lpfR, wetR);
    } else if (scrubVal > 0.05f) { // HPF
        float cutoff = 20.0f * powf(400.0f, fabsf(scrubVal));
        Biquad_SetHighpass(&fx->hpfL, cutoff, 0.707f, sampleRate);
        Biquad_SetHighpass(&fx->hpfR, cutoff, 0.707f, sampleRate);
        wetL = Biquad_Process(&fx->hpfL, wetL);
        wetR = Biquad_Process(&fx->hpfR, wetR);
    }

    *outL = inL + wetL * levelDepth;
    *outR = inR + wetR * levelDepth;
}
