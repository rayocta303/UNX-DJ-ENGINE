#include "audio/asrc_resampler.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float SampleToFloat(const void *buffer, int index, int bitDepth) {
    if (bitDepth == 24) {
        return (float)((const int32_t *)buffer)[index] / 2147483648.0f;
    } else {
        return (float)((const int16_t *)buffer)[index] / 32768.0f;
    }
}

// Sinc function: sinc(x) = sin(pi * x) / (pi * x)
static inline double Sinc(double x) {
    if (fabs(x) < 1e-9) return 1.0;
    double pix = M_PI * x;
    return sin(pix) / pix;
}

// Blackman window function for high out-of-band attenuation (> 74 dB)
static inline double BlackmanWindow(double n, int N) {
    if (n < 0 || n >= N) return 0.0;
    double a0 = 0.42;
    double a1 = 0.50;
    double a2 = 0.08;
    return a0 - a1 * cos((2.0 * M_PI * n) / (N - 1)) + a2 * cos((4.0 * M_PI * n) / (N - 1));
}

static void BuildPolyphaseTable(ASRCResampler *asrc) {
    double ratio = asrc->ratio;
    // Anti-aliasing cutoff frequency (0.45 = 90% of Nyquist to prevent spectral imaging)
    double cutoff = (ratio > 1.0) ? (0.45 / ratio) : 0.45;
    
    int halfTaps = ASRC_NUM_TAPS / 2;

    for (int s = 0; s < ASRC_POLYPHASE_STAGES; s++) {
        double subPhase = (double)s / (double)ASRC_POLYPHASE_STAGES;
        double sum = 0.0;

        for (int t = 0; t < ASRC_NUM_TAPS; t++) {
            double tapOffset = (double)(t - halfTaps + 1) - subPhase;
            double sincVal = Sinc(tapOffset * 2.0 * cutoff) * (2.0 * cutoff);
            double winVal = BlackmanWindow(t, ASRC_NUM_TAPS);
            double coeff = sincVal * winVal;
            
            asrc->polyphaseTable[s][t] = (float)coeff;
            sum += coeff;
        }

        // Normalize filter coefficients for unity gain
        if (fabs(sum) > 1e-6) {
            float invSum = 1.0f / (float)sum;
            for (int t = 0; t < ASRC_NUM_TAPS; t++) {
                asrc->polyphaseTable[s][t] *= invSum;
            }
        }
    }
}

void ASRCResampler_Init(ASRCResampler *asrc, double sourceRate, double targetRate) {
    if (!asrc) return;
    memset(asrc, 0, sizeof(ASRCResampler));
    ASRCResampler_SetRates(asrc, sourceRate, targetRate);
}

void ASRCResampler_SetRates(ASRCResampler *asrc, double sourceRate, double targetRate) {
    if (!asrc) return;
    if (sourceRate <= 0.0) sourceRate = 44100.0;
    if (targetRate <= 0.0) targetRate = 44100.0;

    asrc->sourceSampleRate = sourceRate;
    asrc->targetSampleRate = targetRate;
    asrc->ratio = sourceRate / targetRate;
    asrc->active = (fabs(asrc->ratio - 1.0) > 0.0001);

    BuildPolyphaseTable(asrc);
}

void ASRCResampler_Reset(ASRCResampler *asrc) {
    if (!asrc) return;
    asrc->phaseAccumulator = 0.0;
}

void ASRCResampler_GetSample(ASRCResampler *asrc, const void *buffer, double pos,
                            int bitDepth, uint32_t totalSamples,
                            float *outL, float *outR) {
    if (!buffer || totalSamples < 4) {
        *outL = 0.0f;
        *outR = 0.0f;
        return;
    }

    int totalFrames = (int)(totalSamples / 2);
    int intPos = (int)pos;
    double fracPos = pos - (double)intPos;

    // Polyphase stage selection
    int stage = (int)(fracPos * (double)ASRC_POLYPHASE_STAGES);
    if (stage < 0) stage = 0;
    if (stage >= ASRC_POLYPHASE_STAGES) stage = ASRC_POLYPHASE_STAGES - 1;

    const float *coeffs = asrc->polyphaseTable[stage];
    int halfTaps = ASRC_NUM_TAPS / 2;
    int startFrame = intPos - halfTaps + 1;

    float sumL = 0.0f;
    float sumR = 0.0f;

    for (int t = 0; t < ASRC_NUM_TAPS; t++) {
        int frameIdx = startFrame + t;
        
        // Clamp boundary frames
        if (frameIdx < 0) frameIdx = 0;
        if (frameIdx >= totalFrames) frameIdx = totalFrames - 1;

        float l = SampleToFloat(buffer, frameIdx * 2, bitDepth);
        float r = SampleToFloat(buffer, frameIdx * 2 + 1, bitDepth);
        float c = coeffs[t];

        sumL += l * c;
        sumR += r * c;
    }

    *outL = sumL;
    *outR = sumR;
}
