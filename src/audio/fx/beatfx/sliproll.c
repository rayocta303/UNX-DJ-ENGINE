#include "sliproll.h"

void SlipRoll_Init(BeatFX_SlipRoll* fx) {
    DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
    DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
    fx->currentMs = 0.0f;
    fx->holding = false;
}

void SlipRoll_Free(BeatFX_SlipRoll* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void SlipRoll_Process(BeatFX_SlipRoll* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate, bool isFxOn) {
    // Slip Roll also captures buffer, but constantly rewrites from the incoming stream when ON.
    if (isFxOn && !fx->holding) {
        fx->holding = true;
        fx->currentMs = beatMs;
    } else if (!isFxOn && fx->holding) {
        fx->holding = false;
    }

    if (!fx->holding) {
        DelayLine_Write(&fx->delayL, inL);
        DelayLine_Write(&fx->delayR, inR);
        *outL = inL;
        *outR = inR;
        return;
    }

    // Read the sampled loop
    float readSamples = (fx->currentMs / 1000.0f) * sampleRate;
    float dl = DelayLine_Read(&fx->delayL, readSamples);
    float dr = DelayLine_Read(&fx->delayR, readSamples);

    // Unlike Roll, Slip Roll lets the track continue recording underneath (delay lines keep updating)
    // Wait, pioneer Slip Roll actually rewrites the buffer with the incoming audio?
    // No, Slip Roll means the background track keeps playing while the loop is stuttering over it.
    // So the delay buffer keeps recording. Oh right, in XDJ Slip Roll captures a rhythm and loops it, 
    // but updates the buffer continuously? Actually, Slip Roll grabs a slice and loops it as long as held.
    // Then releases exactly at the background time.
    DelayLine_Write(&fx->delayL, inL + dl * 0.95f); // High feedback to sustain the stutter
    DelayLine_Write(&fx->delayR, inR + dr * 0.95f);

    *outL = inL * (1.0f - levelDepth) + dl * levelDepth;
    *outR = inR * (1.0f - levelDepth) + dr * levelDepth;
}
