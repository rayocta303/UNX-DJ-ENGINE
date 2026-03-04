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
        engine->Decks[i].OutlinedRate = 1.0f;
        engine->Decks[i].Pitch = 10000;
        engine->Decks[i].Trim = 1.0f;
    }
}

void DeckAudio_LoadTrack(DeckAudioState *deck, const char *filePath) {
    if (deck->PCMBuffer) {
        free(deck->PCMBuffer);
        deck->PCMBuffer = NULL;
    }
    deck->TotalSamples = 0;
    deck->Position = 0;
    deck->IsPlaying = false;

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
    if (deck->IsPlaying) {
        // Simple trim update logic placeholder
        // Convert integer pitch to floating rate
        // 10000 = 100% -> BaseRate 1.0
        deck->BaseRate = (float)deck->Pitch / 10000.0f;
    } else {
        deck->BaseRate = 0.0f;
    }

    // Handle scratch inertia
    if (deck->IsScratching) {
        // User is holding jog. ScratchSpeed is populated externally per-frame.
        deck->OutlinedRate = deck->ScratchSpeed;
    } else {
        // Vinyl Glide / Release logic down to BaseRate
        deck->ScratchSpeed += (deck->BaseRate - deck->ScratchSpeed) * 0.12f;
        deck->OutlinedRate = deck->ScratchSpeed;
        
        if (fabs(deck->ScratchSpeed - deck->BaseRate) < 0.02f) {
            deck->ScratchSpeed = deck->BaseRate;
            deck->OutlinedRate = deck->BaseRate; // Done braking/starting
        }
    }
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
    deck->IsPlaying = true;
}

void DeckAudio_Pause(DeckAudioState *deck) {
    deck->IsPlaying = false;
}

// Delta arrives from UI dragging the waveform or jog wheel
void DeckAudio_Scratch(DeckAudioState *deck, double delta) {
    deck->IsScratching = true;
    deck->Position += delta;
    if (deck->Position < 0) deck->Position = 0;
    
    // Convert position dx to a relative speed for inertia
    deck->ScratchSpeed = delta * 0.4;
}

void DeckAudio_SetPitch(DeckAudioState *deck, uint16_t pitch) {
    deck->TargetPitch = pitch;
    deck->Pitch = pitch; // Assume instant update for now unless we need the TRIM smoother
}
