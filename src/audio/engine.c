#include "audio/engine.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

void AudioEngine_Init(AudioEngine *engine) {
    memset(engine, 0, sizeof(AudioEngine));

    for (int i=0; i<MAX_DECKS; i++) {
        engine->Decks[i].BaseRate = 1.0f;
        engine->Decks[i].OutlinedRate = 0.0f; // Motor is off initially
        engine->Decks[i].Pitch = 10000;
        engine->Decks[i].Trim = 1.0f;
        engine->Decks[i].IsMotorOn = false;
        engine->Decks[i].IsPlaying = false;
    }
}

void DeckAudio_LoadTrack(DeckAudioState *deck, const char *filePath) {
    if (deck->PCMBuffer) {
        float *oldBuf = deck->PCMBuffer;
        deck->PCMBuffer = NULL; // Stop audio thread first
        deck->TotalSamples = 0;
        free(oldBuf);
    }
    deck->IsPlaying = false;
    deck->SampleRate = 44100;
    deck->IsMotorOn = false;
    deck->IsTouching = false;
    deck->VinylModeEnabled = true; // Default to Vinyl
    deck->OutlinedRate = 0;
    deck->JogRate = 0;
    deck->MTOffset = 0;
    deck->MTSampleCount = 0;

    if (!filePath || strlen(filePath) == 0) return;

    // Load file using minimp3
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    int res = mp3dec_load(&mp3d, filePath, &info, NULL, NULL);

    if (res == 0) {
        // Successfully loaded
        deck->PCMBuffer = info.buffer; // info.buffer is allocated by malloc internally in minimp3
        deck->TotalSamples = info.samples;
        deck->SampleRate = info.hz;
        
        // If mono, we should duplicate to stereo, but assuming Rekordbox tracks are mostly stereo
        // We'll leave it as is for now, but realistically we should resample/rechannel here
        // if info.channels == 1.
    } else {
        printf("Failed to load audio file: %s (Error %d)\n", filePath, res);
    }
}

static void ProcessDeckPhysics(DeckAudioState *deck) {
    // 1. Determine Logical Base Rate (Target speed if motor is at 100%)
    deck->BaseRate = (float)deck->Pitch / 10000.0f;

    // 2. Determine Local Target Rate based on Motor/Jog state
    double targetRate = 0.0;
    float accel = 0.08f; 

    if (deck->IsTouching) {
        if (deck->VinylModeEnabled) {
            // Vinyl Mode + Touch: Follow hand (Jog/Scratch)
            targetRate = deck->JogRate;
            accel = 1.0f; 
        } else {
            // CDJ Nudge: Offset original speed
            targetRate = deck->BaseRate + deck->JogRate;
            accel = 0.4f; // Nudge is slightly smoothed
        }
    } else {
        // Hand is off
        if (deck->IsMotorOn) {
            targetRate = deck->BaseRate;
            accel = 0.12f; // Motor startup
        } else {
            targetRate = 0.0;
            accel = 0.015f; // Platter braking (slow glide to stop)
        }
    }

    // 3. Glide OutlinedRate towards targetRate
    if (deck->OutlinedRate != targetRate) {
        double diff = targetRate - deck->OutlinedRate;
        deck->OutlinedRate += diff * accel;

        if (fabs(diff) < 0.001) {
            deck->OutlinedRate = targetRate;
        }
    }

    deck->IsPlaying = (fabs(deck->OutlinedRate) > 0.001);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Cubic Hermite interpolation (4-point) for high-fidelity resampling
// Inspired by Mixxx/MusicDSP: clear highs and smooth bass.
static inline float hermite4(float frac_pos, float xm1, float x0, float x1, float x2) {
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    return ((((a * frac_pos) - b_neg) * frac_pos + c) * frac_pos + x0);
}

// Resampling directly from PCM Buffer into output
static void ProcessDeckAudio(DeckAudioState *deck, float *outBuffer, int framesToProcess) {
    if (!deck->PCMBuffer || deck->TotalSamples == 0) return;

    ProcessDeckPhysics(deck);

    double rate = deck->OutlinedRate;
    if (deck->IsReverse) {
        rate = -rate;
    }

    if (rate == 0.0) return;

    // We process frame by frame
    for (int i = 0; i < framesToProcess; i++) {
        double readPos = deck->Position;
        float mtWeight = 1.0f;
        // MT is active if Key Lock is ON, not scratching, motor is running, and speed is not precisely 1.0
        bool mtActive = deck->MasterTempoActive && !deck->IsTouching && deck->IsMotorOn && fabs(rate - 1.0) > 0.005;

        if (mtActive) {
            readPos = deck->Position + deck->MTOffset;
            deck->MTOffset += (1.0 - rate);
            deck->MTSampleCount++;
            
            // Grain boundaries: 4096 samples total, 512 samples crossfade
            const int GRAIN_SIZE = 4096;
            const int XFADE_SIZE = 512;
            const int START_XFADE = GRAIN_SIZE - XFADE_SIZE;

            if (deck->MTSampleCount > START_XFADE) {
                float t = (float)(deck->MTSampleCount - START_XFADE) / (float)XFADE_SIZE;
                // Sine window for constant-power crossfade: sin(t * PI/2)
                mtWeight = cosf(t * (float)M_PI * 0.5f); 
            }
            if (deck->MTSampleCount >= GRAIN_SIZE) {
                deck->MTSampleCount = 0;
                deck->MTOffset = 0;
            }
        }

        float l_sample = 0.0f;
        float r_sample = 0.0f;

        // --- Helper for 4-point Hermite interpolation ---
        #define INTERP_SAMPLES_HI(pos, l, r) { \
            int sf = (int)floor(pos); \
            float fr = (float)(pos - floor(pos)); \
            l = 0; r = 0; \
            if (sf >= 1 && (uint32_t)((sf * 2) + 5) < deck->TotalSamples) { \
                int idx = sf * 2; \
                l = hermite4(fr, deck->PCMBuffer[idx-2], deck->PCMBuffer[idx], deck->PCMBuffer[idx+2], deck->PCMBuffer[idx+4]); \
                r = hermite4(fr, deck->PCMBuffer[idx-1], deck->PCMBuffer[idx+1], deck->PCMBuffer[idx+3], deck->PCMBuffer[idx+5]); \
            } else if (sf >= 0 && (uint32_t)((sf * 2) + 1) < deck->TotalSamples) { \
                /* Fallback to linear at track start/end boundaries */ \
                int idx = sf * 2; \
                l = deck->PCMBuffer[idx] + fr * (deck->PCMBuffer[idx + 2] - deck->PCMBuffer[idx]); \
                r = deck->PCMBuffer[idx + 1] + fr * (deck->PCMBuffer[idx + 3] - deck->PCMBuffer[idx + 1]); \
            } \
        }

        INTERP_SAMPLES_HI(readPos, l_sample, r_sample);

        // If MT is active and in crossfade zone, mix in the secondary grain
        if (mtActive && mtWeight < 1.0f) {
            float l2 = 0.0f, r2 = 0.0f;
            INTERP_SAMPLES_HI(deck->Position, l2, r2); 
            l_sample = l_sample * mtWeight + l2 * (1.0f - mtWeight);
            r_sample = r_sample * mtWeight + r2 * (1.0f - mtWeight);
        }

        // Apply Trim Volume
        l_sample *= deck->Trim;
        r_sample *= deck->Trim;

        // Mix into output buffer
        outBuffer[i * CHANNELS] += l_sample;
        outBuffer[i * CHANNELS + 1] += r_sample;

        // Advance playhead
        deck->Position += rate;

        // Loop / End track boundaries
        if (deck->Position < 0) deck->Position = 0;
        if (deck->Position * CHANNELS >= (double)deck->TotalSamples) {
            // Track end
            deck->IsPlaying = false;
            deck->Position = (double)(deck->TotalSamples / CHANNELS) - 1.0;
        }
    }
}

void AudioEngine_Process(AudioEngine *engine, float *outBuffer, int frames) {
    // Clear output buffer since we mix additively
    memset(outBuffer, 0, frames * CHANNELS * sizeof(float));

    for (int i = 0; i < MAX_DECKS; i++) {
        ProcessDeckAudio(&engine->Decks[i], outBuffer, frames);
    }
}

void DeckAudio_Play(DeckAudioState *deck) {
    deck->IsMotorOn = true;
}

void DeckAudio_Pause(DeckAudioState *deck) {
    deck->IsMotorOn = false;
}

void DeckAudio_SetPlaying(DeckAudioState *deck, bool playing) {
    deck->IsMotorOn = playing;
}

void DeckAudio_InstantPlay(DeckAudioState *deck) {
    deck->IsMotorOn = true;
    deck->BaseRate = (float)deck->Pitch / 10000.0f;
    deck->OutlinedRate = deck->BaseRate;
}

// JogRate is driven by UI delta to match mouse movement
void DeckAudio_SetJogRate(DeckAudioState *deck, double rate) {
    if (!deck->IsTouching) return;
    deck->JogRate = rate;
}

void DeckAudio_SetJogTouch(DeckAudioState *deck, bool touching) {
    deck->IsTouching = touching;
    if (!touching) {
        deck->JogRate = deck->OutlinedRate;
    }
}

void DeckAudio_JumpToMs(DeckAudioState *deck, uint32_t ms) {
    deck->Position = (double)ms * (44100.0 / 1000.0);
    if (deck->Position < 0) deck->Position = 0;
    if (deck->Position * CHANNELS >= (double)deck->TotalSamples) {
        deck->Position = (double)(deck->TotalSamples / CHANNELS) - 1.0;
    }
}

void DeckAudio_SetPitch(DeckAudioState *deck, uint16_t pitch) {
    deck->TargetPitch = pitch;
    deck->Pitch = pitch; // Assume instant update for now unless we need the TRIM smoother
}
