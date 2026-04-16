#include "audio/scalers.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void WSOLA_Init(WSOLA* wsola, uint32_t sampleRate) {
    memset(wsola, 0, sizeof(WSOLA));
    wsola->sampleRate = sampleRate;
    wsola->searchTrigger[0] = true;
    wsola->searchTrigger[1] = true;
}

// WSOLA logic extracted and refined for high-fidelity
void WSOLA_Process(WSOLA* wsola, float* input, float* output, uint32_t frames, double tempoRatio, 
                  void (*getSample)(void* ctx, double pos, float* l, float* r), void* ctx, double currentPos) {
    
    if (fabs(tempoRatio - 1.0) < 0.001) {
        // Pass-thru optimization
        for (uint32_t i = 0; i < frames; i++) {
            getSample(ctx, currentPos + i, &output[i*2], &output[i*2+1]);
        }
        return;
    }

    wsola->offset += (1.0 - tempoRatio);
    // 50ms period is standard for high-quality WSOLA (SoundTouch)
    const double period = (double)wsola->sampleRate * 0.05; 
    
    double phase[2];
    phase[0] = fmod(wsola->offset, period); if (phase[0] < 0) phase[0] += period;
    phase[1] = fmod(wsola->offset + period * 0.5, period); if (phase[1] < 0) phase[1] += period;

    for (int k = 0; k < 2; k++) {
        if (phase[k] < (double)frames && wsola->searchTrigger[k]) {
            int other = 1 - k;
            double refP = currentPos + (phase[other] - period * 0.5) + wsola->phaseOffset[other];
            
            #define S_WIN 512
            #define S_RA  1024
            
            float refM[S_WIN], refE = 0.001f;
            for (int j = 0; j < S_WIN; j++) {
                float l, r; getSample(ctx, refP - (S_WIN/2) + j, &l, &r);
                refM[j] = l + r; refE += fabsf(refM[j]);
            }
            
            float sCache[S_RA * 2 + S_WIN];
            double aStart = currentPos - (S_WIN/2) - S_RA;
            for (int j = 0; j < S_RA * 2 + S_WIN; j++) {
                float l, r; getSample(ctx, aStart + j, &l, &r);
                sCache[j] = l + r;
            }
            
            float bestScore = 1e30f; int bestOff = S_RA;
            for (int o = 0; o < S_RA * 2; o++) {
                float sad = 0;
                for (int j = 0; j < S_WIN; j++) {
                    sad += fabsf(sCache[o + j] - refM[j]);
                }
                float dist = (float)abs(o - S_RA) / (float)S_RA;
                float score = sad * (1.0f + 0.4f * dist * dist); // Stronger center preference
                
                if (score < bestScore) {
                    bestScore = score;
                    bestOff = o;
                }
            }
            
            float newOff = (float)(bestOff - S_RA);
            wsola->phaseOffset[k] = wsola->phaseOffset[k] * 0.4f + newOff * 0.6f;
            wsola->searchTrigger[k] = false;
            
            #undef S_WIN
            #undef S_RA
        } else if (phase[k] > (period * 0.45)) {
            wsola->searchTrigger[k] = true;
        }
    }

    for (uint32_t i = 0; i < frames; i++) {
        double p0 = fmod(wsola->offset + i * (1.0 - tempoRatio), period); if (p0 < 0) p0 += period;
        double p1 = fmod(p0 + period * 0.5, period); if (p1 < 0) p1 += period;

        double x = p0 / period;
        double w0 = 0.5 * (1.0 - cos(2.0 * M_PI * x)); // Hann Window
        double w1 = 1.0 - w0;
        
        double rp0 = currentPos + i + (p0 - period * 0.5) + wsola->phaseOffset[0];
        double rp1 = currentPos + i + (p1 - period * 0.5) + wsola->phaseOffset[1];
        
        float l0, r0, l1, r1;
        getSample(ctx, rp0, &l0, &r0);
        getSample(ctx, rp1, &l1, &r1);
        
        output[i*2] = (l0 * (float)w0 + l1 * (float)w1);
        output[i*2+1] = (r0 * (float)w0 + r1 * (float)w1);
    }
}
