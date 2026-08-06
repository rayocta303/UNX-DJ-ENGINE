#ifndef ASRC_RESAMPLER_H
#define ASRC_RESAMPLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASRC_NUM_TAPS 16
#define ASRC_POLYPHASE_STAGES 32

typedef struct ASRCResampler {
    double sourceSampleRate;
    double targetSampleRate;
    double ratio;             // sourceSampleRate / targetSampleRate
    double phaseAccumulator;  // 64-bit fractional phase accumulator to prevent drift
    bool active;              // True if ratio != 1.0 (resampling needed)

    // Pre-calculated Sinc Polyphase Filter Table
    float polyphaseTable[ASRC_POLYPHASE_STAGES][ASRC_NUM_TAPS];
} ASRCResampler;

void ASRCResampler_Init(ASRCResampler *asrc, double sourceRate, double targetRate);
void ASRCResampler_SetRates(ASRCResampler *asrc, double sourceRate, double targetRate);
void ASRCResampler_Reset(ASRCResampler *asrc);

// High-fidelity bandlimited anti-aliasing sample interpolation
void ASRCResampler_GetSample(ASRCResampler *asrc, const void *buffer, double pos,
                            int bitDepth, uint32_t totalSamples,
                            float *outL, float *outR);

#ifdef __cplusplus
}
#endif

#endif // ASRC_RESAMPLER_H
