#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define MAX_DECKS 2

// 1 half-frame = 1/150 s at 44.1kHz = 294 samples
#define SAMPLES_PER_HALF_FRAME 294 

typedef enum {
    CHANGE_SPEED_NO_CHANGE = 0,
    CHANGE_SPEED_NEED_UP = 1,
    CHANGE_SPEED_NEED_DOWN = 2
} SpeedChangeState;

typedef struct DeckAudioState {
    float *PCMBuffer;           // Full track audio decoded (interleaved L/R)
    uint32_t TotalSamples;      // Total samples in buffer
    
    // Core playback state (from XDJ-X firmware concept)
    double Position;            // Read head position (exact fractional sample index)
    uint32_t PlayAddressCUE;    // Integer frame base for CUE (usually 294 * CUE_ADR)
    
    // Physics and Scratch
    uint16_t Pitch;             // 10000 = 100% pitch
    uint16_t TargetPitch;       // Where the fader is currently set
    double ScratchSpeed;        // Inertia value for vinyl mode
    bool IsScratching;          // User is actively touching jog
    bool IsPlaying;             // Deck is in play mode
    bool IsReverse;             // Reverse playback active
    
    SpeedChangeState SpeedState; // Need ramp up/down
    
    // Mixxx EngineBufferScaleLinear state
    float BaseRate;             // Determined by pitch slider
    float OutlinedRate;         // Final calculated rate including scratch offsets
    bool MasterTempoActive;     // Key lock

    float Trim;
} DeckAudioState;

typedef struct AudioEngine {
    DeckAudioState Decks[MAX_DECKS];
} AudioEngine;

void AudioEngine_Init(AudioEngine *engine);
void AudioEngine_Process(AudioEngine *engine, float *outBuffer, int frames);

void DeckAudio_LoadTrack(DeckAudioState *deck, const char *filePath);
void DeckAudio_Play(DeckAudioState *deck);
void DeckAudio_Pause(DeckAudioState *deck);
// Called when jog wheel is moved during touch
void DeckAudio_Scratch(DeckAudioState *deck, double delta); 
void DeckAudio_SetPitch(DeckAudioState *deck, uint16_t pitch);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_ENGINE_H
