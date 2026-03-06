#include "pitch.h"
#include <math.h>

void Pitch_Init(BeatFX_Pitch* fx) {
    DelayLine_Init(&fx->delayL, 4096);
    DelayLine_Init(&fx->delayR, 4096);
    fx->phase = 0.0f;
}

void Pitch_Free(BeatFX_Pitch* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void Pitch_Process(BeatFX_Pitch* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate) {
    // Simple delay-doppler shift (single grain moving head)
    // Actually, a true pitch shift requires two overlapping delay lines with crossfading.
    // For simplicity, we'll do a single moving read head with a window to hide clicks.
    
    // beatMs controls the pitch ratio.
    // Let's sweep pitch roughly +/- 1 octave based on beatMs vs 1000ms.
    float ratio = 1000.0f / beatMs; // 1.0 = no shift. 2.0 = octave up. 0.5 = octave down.
    
    float windowSize = 2048.0f;
    float phaseInc = (ratio - 1.0f) * (windowSize / sampleRate);
    
    fx->phase += phaseInc;
    if (fx->phase >= windowSize) fx->phase -= windowSize;
    if (fx->phase < 0.0f) fx->phase += windowSize;

    // Crossfade (Cosine window)
    float t = fx->phase / windowSize;
    float win = sinf(t * 3.14159f);

    float readL = DelayLine_Read(&fx->delayL, fx->phase);
    float readR = DelayLine_Read(&fx->delayR, fx->phase);

    DelayLine_Write(&fx->delayL, inL);
    DelayLine_Write(&fx->delayR, inR);

    // Full wet mix for Pitch FX
    *outL = inL * (1.0f - levelDepth) + (readL * win) * levelDepth;
    *outR = inR * (1.0f - levelDepth) + (readR * win) * levelDepth;
}
