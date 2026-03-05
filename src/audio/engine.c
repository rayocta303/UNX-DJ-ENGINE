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
    deck->TotalSamples = 0;
    deck->Position = 0;
    deck->IsPlaying = false;
    deck->IsMotorOn = false;
    deck->OutlinedRate = 0;
    deck->ScratchSpeed = 0;

    if (!filePath || strlen(filePath) == 0) return;

    // Load file using minimp3
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    int res = mp3dec_load(&mp3d, filePath, &info, NULL, NULL);

    if (res == 0) {
        // Successfully loaded
        deck->PCMBuffer = info.buffer; // info.buffer is allocated by malloc internally in minimp3
        deck->TotalSamples = info.samples;
        
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

    // 2. Determine Local Target Rate based on Motor/Scratch state
    double targetRate = 0.0;
    float accel = 0.08f; 

    if (deck->IsScratching) {
        // Hand is on the platter. Follow mouse speed exactly.
        targetRate = deck->ScratchSpeed;
        accel = 1.0f; // Instant "sticky" follow
    } else {
        // Hand is off.
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

// Linear interpolation resampling directly from PCM Buffer into output
// Based heavily on Mixxx's EngineBufferScaleLinear logic
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
        double currentFrame = deck->Position;
        
        // Integer frame index points
        int currentFrameFloor = (int)floor(currentFrame);
        int sampleIndex = currentFrameFloor * CHANNELS;

        float frac = (float)(currentFrame - floor(currentFrame));

        float l_sample = 0.0f;
        float r_sample = 0.0f;

        // Ensure we are within bounds
        if (sampleIndex >= 0 && sampleIndex + CHANNELS + 1 < deck->TotalSamples) {
            float floor_l = deck->PCMBuffer[sampleIndex];
            float floor_r = deck->PCMBuffer[sampleIndex + 1];

            float ceil_l = deck->PCMBuffer[sampleIndex + 2];
            float ceil_r = deck->PCMBuffer[sampleIndex + 3];

            // Linear interpolation
            l_sample = floor_l + frac * (ceil_l - floor_l);
            r_sample = floor_r + frac * (ceil_r - floor_r);
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

// ScratchSpeed is driven by UI delta to match mouse movement
void DeckAudio_Scratch(DeckAudioState *deck, double rate) {
    deck->IsScratching = true;
    deck->ScratchSpeed = rate;
}

void DeckAudio_SetScratch(DeckAudioState *deck, bool scratching) {
    if (scratching && !deck->IsScratching) {
        // Touch start: set initial speed to current speed for seamless transition
        deck->ScratchSpeed = deck->OutlinedRate;
    } else if (!scratching && deck->IsScratching) {
        // Release: maintenance the last scratch speed for inertia glide
    }
    deck->IsScratching = scratching;
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
