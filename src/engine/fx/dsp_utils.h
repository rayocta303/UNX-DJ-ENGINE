#ifndef DSP_UTILS_H
#define DSP_UTILS_H

#include "engine/util/engine_math.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DELAY_SAMPLES                                                      \
  (192000 * 4) // Support 4 seconds even at 192kHz or 8s at 96kHz

// --- Biquad Filter ---
typedef struct BiquadFilter {
  float x1, x2;
  float y1, y2;
  float b0, b1, b2, a1, a2;
} BiquadFilter;

void Biquad_Init(BiquadFilter *filter);
void Biquad_SetLowpass(BiquadFilter *filter, float cutoff, float q,
                       float sampleRate);
void Biquad_SetHighpass(BiquadFilter *filter, float cutoff, float q,
                        float sampleRate);
void Biquad_SetBandpass(BiquadFilter *filter, float cutoff, float q,
                        float sampleRate);
void Biquad_SetPeak(BiquadFilter *filter, float cutoff, float q, float gainDb,
                    float sampleRate);
void Biquad_SetNotch(BiquadFilter *filter, float cutoff, float q,
                     float sampleRate);
float Biquad_Process(BiquadFilter *filter, float in);

// --- LR4 Crossover ---
typedef struct EngineLR4 CrossoverLR4;
void Crossover_Init(CrossoverLR4 *lr, float freq, float sampleRate,
                    bool highpass);
float Crossover_Process(CrossoverLR4 *lr, float in);

// --- Delay Line ---
typedef struct DelayLine {
  float *buffer;
  uint32_t length;
  uint32_t writePos;
} DelayLine;

bool DelayLine_Init(DelayLine *delay, uint32_t maxLength);
void DelayLine_Free(DelayLine *delay);
void DelayLine_Clear(DelayLine *delay);
void DelayLine_Write(DelayLine *delay, float sample);
float DelayLine_Read(DelayLine *delay, float delaySamples); // Fractional delay

// --- LFO ---
typedef enum { LFO_SINE, LFO_TRIANGLE, LFO_SAW, LFO_SQUARE } LFOType;

typedef struct LFO {
  LFOType type;
  float phase;
  float phaseInc;
} LFO;

void LFO_Init(LFO *lfo);
void LFO_SetFreq(LFO *lfo, float freq, float sampleRate);
float LFO_Process(LFO *lfo); // Returns -1.0 to 1.0

// --- Noise Generator ---
float WhiteNoise_Process(void);

#ifdef __cplusplus
}
#endif

#endif // DSP_UTILS_H
