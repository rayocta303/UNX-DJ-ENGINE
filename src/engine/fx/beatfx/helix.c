#include "helix.h"

void Helix_Init(BeatFX_Helix* fx) {
    DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
    DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
    fx->currentMs = 0.0f;
    fx->holding = false;
}

void Helix_Free(BeatFX_Helix* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void Helix_Process(BeatFX_Helix* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate, bool isFxOn) {
    // Helix is like Slip Roll but with a pitch-up or pitch-down characteristic during the tail.
    // For a simplified Helix, we capture a loop and play it back with a slightly decreasing delay time (Pitch Up).
    
    if (isFxOn && !fx->holding) {
        fx->holding = true;
        fx->currentMs = beatMs;
    } else if (!isFxOn && fx->holding) {
        fx->holding = false;
        // Upon release, the captured sound usually trails off while accelerating/decelerating
    }

    if (!fx->holding) {
        DelayLine_Write(&fx->delayL, inL);
        DelayLine_Write(&fx->delayR, inR);
        *outL = inL;
        *outR = inR;
        return;
    }

    // Shrinking loop size simulates pitch up and acceleration
    fx->currentMs *= 0.999f; // Gradually accelerate the loop
    if (fx->currentMs < 10.0f) fx->currentMs = 10.0f; // Limit

    float readSamples = (fx->currentMs / 1000.0f) * sampleRate;
    float dl = DelayLine_Read(&fx->delayL, readSamples);
    float dr = DelayLine_Read(&fx->delayR, readSamples);

    DelayLine_Write(&fx->delayL, dl * 0.98f);
    DelayLine_Write(&fx->delayR, dr * 0.98f);

    *outL = inL * (1.0f - levelDepth) + dl * levelDepth;
    *outR = inR * (1.0f - levelDepth) + dr * levelDepth;
}
