#ifndef MIXXX_ENGINE_H
#define MIXXX_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Professional Mixxx-Style Resampling (Hermite 4) ---
float Mixxx_InterpolateHermite4(float frac_pos, float xm1, float x0, float x1, float x2);

// --- professional Mixxx-Style Linkwitz-Riley Filters ---
typedef struct {
    float x1, x2, y1, y2;
    float b0, b1, b2, a1, a2;
} MixxxBiquad;

typedef struct {
    MixxxBiquad stages[2]; // LR4 is two cascaded stages
} MixxxLR4;

void MixxxLR4_Init(MixxxLR4* filter);
void MixxxLR4_SetLowpass(MixxxLR4* filter, float freq, float sampleRate);
void MixxxLR4_SetHighpass(MixxxLR4* filter, float freq, float sampleRate);
float MixxxLR4_Process(MixxxLR4* filter, float in);

#ifdef __cplusplus
}
#endif

#endif // MIXXX_ENGINE_H
