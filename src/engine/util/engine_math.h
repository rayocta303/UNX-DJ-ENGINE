#ifndef Engine_ENGINE_H
#define Engine_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Professional Engine-Style Resampling (Hermite 4) ---
float Engine_InterpolateHermite4(float frac_pos, float xm1, float x0, float x1, float x2);

// --- professional Engine-Style Linkwitz-Riley Filters ---
typedef struct EngineBiquad {
    float x1, x2, y1, y2;
    float b0, b1, b2, a1, a2;
} EngineBiquad;

typedef struct EngineLR4 {
    EngineBiquad stages[2]; // LR4 is two cascaded stages
} EngineLR4;

void EngineLR4_Init(EngineLR4* filter);
void EngineLR4_SetLowpass(EngineLR4* filter, float freq, float sampleRate);
void EngineLR4_SetHighpass(EngineLR4* filter, float freq, float sampleRate);
float EngineLR4_Process(EngineLR4* filter, float in);
float Engine_GetCrossfaderGain(float crossfader, int deckIdx);

#ifdef __cplusplus
}
#endif

#endif // Engine_ENGINE_H
