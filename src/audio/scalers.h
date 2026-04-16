#ifndef AUDIO_SCALERS_H
#define AUDIO_SCALERS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// A professional-grade WSOLA time-stretcher (SoundTouch style)
typedef struct WSOLA {
    float* buffer;
    uint32_t bufferSize;
    uint32_t sampleRate;
    double offset;
    float phaseOffset[2];
    bool searchTrigger[2];
} WSOLA;

void WSOLA_Init(WSOLA* wsola, uint32_t sampleRate);
void WSOLA_Process(WSOLA* wsola, float* input, float* output, uint32_t frames, double tempoRatio, 
                  void (*getSample)(void* ctx, double pos, float* l, float* r), void* ctx, double currentPos);

#ifdef __cplusplus
}
#endif

#endif
