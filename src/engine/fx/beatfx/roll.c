#include "roll.h"

void Roll_Init(BeatFX_Roll* fx) {
    DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
    DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
    fx->currentMs = 0.0f;
    fx->holding = false;
}

void Roll_Free(BeatFX_Roll* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void Roll_Process(BeatFX_Roll* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate, bool isFxOn) {
    (void)beatMs;
    (void)sampleRate;
    
    // In Pioneer FX, Roll grabs the audio at the moment it's turned ON and loops exactly that chunk.
    if (isFxOn && !fx->holding) {
        // Just turned ON
        fx->holding = true;
        fx->currentMs = beatMs;
    } else if (!isFxOn && fx->holding) {
        // Turned OFF
        fx->holding = false;
        DelayLine_Clear(&fx->delayL);
        DelayLine_Clear(&fx->delayR);
    }

    if (!fx->holding) {
        // Write continuously when OFF so we always have the past audio ready to loop
        DelayLine_Write(&fx->delayL, inL);
        DelayLine_Write(&fx->delayR, inR);
        *outL = inL;
        *outR = inR;
        return;
    }

    // When ON, we loop the captured buffer
    float readSamples = (fx->currentMs / 1000.0f) * sampleRate;
    float dl = DelayLine_Read(&fx->delayL, readSamples);
    float dr = DelayLine_Read(&fx->delayR, readSamples);

    // Keep writing what we read to sustain the loop
    DelayLine_Write(&fx->delayL, dl);
    DelayLine_Write(&fx->delayR, dr);

    // Mix dry/wet
    *outL = inL * (1.0f - levelDepth) + dl * levelDepth;
    *outR = inR * (1.0f - levelDepth) + dr * levelDepth;
}
