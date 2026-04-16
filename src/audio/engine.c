#include "audio/engine.h"
#include "engine/util/engine_math.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_API static
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif

#define DRWAV_API static
#define DRWAV_PRIVATE static
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

void AudioEngine_Init(AudioEngine *engine) {
    memset(engine, 0, sizeof(AudioEngine));

    for (int i = 0; i < MAX_DECKS; i++) {
        engine->Decks[i].BaseRate = 1.0f;
        engine->Decks[i].OutlinedRate = 0.0f; // Motor is off initially
        engine->Decks[i].Pitch = 10000;
        engine->Decks[i].Trim = 1.0f;
        engine->Decks[i].EqLow = 0.5f;
        engine->Decks[i].EqMid = 0.5f;
        engine->Decks[i].EqHigh = 0.5f;
        EngineLR4_Init(&engine->Decks[i].EqLowStateL);
        EngineLR4_Init(&engine->Decks[i].EqLowStateR);
        EngineLR4_Init(&engine->Decks[i].EqHighStateL);
        EngineLR4_Init(&engine->Decks[i].EqHighStateR);
        ColorFXManager_Init(&engine->Decks[i].ColorFX);
        engine->Decks[i].IsMotorOn = false;
        engine->Decks[i].IsPlaying = false;
        engine->Decks[i].MTSearchTrigger[0] = true;
        engine->Decks[i].MTSearchTrigger[1] = true;
    }
    
    BeatFXManager_Init(&engine->BeatFX);
    engine->Crossfader = 0.0f;
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
    deck->MTSearchTrigger[0] = true;
    deck->MTSearchTrigger[1] = true;
    deck->MTPhaseOffset[0] = 0;
    deck->MTPhaseOffset[1] = 0;

    if (!filePath || strlen(filePath) == 0) return;

    // Check file extension
    const char *ext = strrchr(filePath, '.');
    bool isWav = (ext && (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".aif") == 0 || strcasecmp(ext, ".aiff") == 0));

    if (isWav) {
        // Load file using dr_wav
        unsigned int channels;
        unsigned int sampleRate;
        drwav_uint64 totalPCMFrameCount;
        float *pSampleData = drwav_open_file_and_read_pcm_frames_f32(filePath, &channels, &sampleRate, &totalPCMFrameCount, NULL);

        if (pSampleData) {
            if (channels == 1) {
                // Duplicate mono to stereo
                float *stereoBuf = (float *)malloc(totalPCMFrameCount * 2 * sizeof(float));
                for (drwav_uint64 i = 0; i < totalPCMFrameCount; i++) {
                    stereoBuf[i*2] = pSampleData[i];
                    stereoBuf[i*2 + 1] = pSampleData[i];
                }
                drwav_free(pSampleData, NULL);
                deck->PCMBuffer = stereoBuf;
                deck->TotalSamples = totalPCMFrameCount * 2;
            } else {
                deck->PCMBuffer = pSampleData;
                deck->TotalSamples = totalPCMFrameCount * channels;
            }
            deck->SampleRate = sampleRate;
        } else {
            printf("Failed to load WAV/AIFF file: %s\n", filePath);
        }
    } else {
        // Load file using minimp3
        mp3dec_t mp3d;
        mp3dec_file_info_t info;
        int res = mp3dec_load(&mp3d, filePath, &info, NULL, NULL);

        if (res == 0) {
            // Successfully loaded MP3
            if (info.channels == 1) {
                // Duplicate mono to stereo
                float *stereoBuf = (float *)malloc(info.samples * 2 * sizeof(float));
                for (size_t i = 0; i < info.samples; i++) {
                    stereoBuf[i*2] = info.buffer[i];
                    stereoBuf[i*2 + 1] = info.buffer[i];
                }
                free(info.buffer);
                deck->PCMBuffer = stereoBuf;
                deck->TotalSamples = info.samples * 2;
            } else {
                deck->PCMBuffer = info.buffer; // info.buffer is allocated by malloc internally in minimp3
                deck->TotalSamples = info.samples;
            }
            deck->SampleRate = info.hz;
        } else {
            printf("Failed to load audio file: %s (Error %d)\n", filePath, res);
        }
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
            accel = deck->VinylStartAccel > 0 ? deck->VinylStartAccel : 0.12f; // Motor startup
        } else {
            targetRate = 0.0;
            accel = deck->VinylStopAccel > 0 ? deck->VinylStopAccel : 0.015f; // Platter braking
        }
    }

    // 3. Glide OutlinedRate towards targetRate (Linear Ramping)
    if (deck->OutlinedRate != targetRate) {
        float diff = targetRate - (float)deck->OutlinedRate;
        
        if (fabs(diff) < accel) {
            deck->OutlinedRate = targetRate;
        } else {
            if (diff > 0) deck->OutlinedRate += accel;
            else deck->OutlinedRate -= accel;
        }
    }

    deck->IsPlaying = (fabs(deck->OutlinedRate) > 0.001);
}

// Engine Hermite4 and LR4 are now provided via Engine_engine.h

static inline void AudioEngine_GetSample(DeckAudioState* deck, double pos, float* l, float* r) {
    if (pos < 2.0) { *l = 0; *r = 0; return; }
    int i = (int)pos;
    float f = (float)(pos - i);
    if (i >= (int)((deck->TotalSamples / 2) - 3)) { *l = 0; *r = 0; return; }

    *l = Engine_InterpolateHermite4(f, deck->PCMBuffer[(i - 1) * 2], deck->PCMBuffer[i * 2], deck->PCMBuffer[(i + 1) * 2], deck->PCMBuffer[(i + 2) * 2]);
    *r = Engine_InterpolateHermite4(f, deck->PCMBuffer[(i - 1) * 2 + 1], deck->PCMBuffer[i * 2 + 1], deck->PCMBuffer[(i + 1) * 2 + 1], deck->PCMBuffer[(i + 2) * 2 + 1]);
}

#define INTERP_SAMPLES_PRO(pos, l, r) AudioEngine_GetSample(deck, pos, &l, &r)

static void ProcessDeckAudio(DeckAudioState* deck, float* outMaster, float* outCue, int frames, AudioEngine* engine, int deckIndex) {
    bool noiseActive = (deck->ColorFX.activeFX == COLORFX_NOISE && deck->ColorFX.colorValue != 0.0f);
    if (!deck->PCMBuffer && !noiseActive) return;

    // Handle Quantize / Queued Jumps
    if (deck->HasQueuedJump) {
        if (deck->QueuedWaitSamples >= (uint32_t)frames) {
            deck->QueuedWaitSamples -= (uint32_t)frames;
        } else {
            DeckAudio_JumpToMs(deck, deck->QueuedJumpMs);
            deck->IsMotorOn = true;
            deck->BaseRate = (float)deck->Pitch / 10000.0f;
            deck->OutlinedRate = deck->BaseRate;
            deck->HasQueuedJump = false;
        }
    }

    ProcessDeckPhysics(deck);

    if (!deck->IsPlaying && !noiseActive) {
        deck->VuMeterL *= 0.95f; deck->VuMeterR *= 0.95f;
        return;
    }

    bool mtActive = deck->MasterTempoActive;
    float fs = (deck->SampleRate > 0) ? (float)deck->SampleRate : (float)SAMPLE_RATE;

    // --- Rate Smoothing (Engine Port) ---
    if (deck->LastRate == 0) deck->LastRate = deck->OutlinedRate;
    double currentRate = deck->LastRate;
    double targetRate = deck->OutlinedRate;
    double rateDelta = (targetRate - currentRate) / (double)frames;

    // --- Linkwitz-Riley 4th Order EQ Crossovers (Engine Standard) ---
    EngineLR4_SetLowpass(&deck->EqLowStateL, 350.0f, 44100);
    EngineLR4_SetLowpass(&deck->EqLowStateR, 350.0f, 44100);
    EngineLR4_SetHighpass(&deck->EqHighStateL, 2500.0f, 44100);
    EngineLR4_SetHighpass(&deck->EqHighStateR, 2500.0f, 44100);

    float maxL = 0, maxR = 0;

    for (int i = 0; i < frames; i++) {
        currentRate += rateDelta;
        double rate = currentRate;
        float l_sample = 0, r_sample = 0;

        if (mtActive && fabs(rate) > 0.05) {
            deck->MTOffset += (1.0 - rate);
            const double period = 1323.0; // 30ms @ 44.1kHz
            const double drift = deck->MTOffset;
            double phase[2];
            phase[0] = fmod(drift, period); if (phase[0] < 0) phase[0] += period;
            phase[1] = fmod(drift + period * 0.5, period); if (phase[1] < 0) phase[1] += period;

            for (int k = 0; k < 2; k++) {
                if (phase[k] < 12.0 && deck->MTSearchTrigger[k]) {
                    int other = 1 - k;
                    double refP = deck->Position + (phase[other] - period * 0.5) + deck->MTPhaseOffset[other];
                    #define S_WIN 256
                    #define S_RA 640
                    float refM[S_WIN], refE = 0.001f;
                    for (int j = 0; j < S_WIN; j++) {
                        float l, r; INTERP_SAMPLES_PRO(refP + j, l, r);
                        refM[j] = l + r; refE += fabsf(refM[j]);
                    }
                    static float sCache[S_RA * 2 + S_WIN];
                    double aStart = deck->Position - (period * 0.5) - S_RA;
                    for (int j = 0; j < S_RA * 2 + S_WIN; j++) {
                        float l, r; INTERP_SAMPLES_PRO(aStart + j, l, r);
                        sCache[j] = l + r;
                    }
                    float bestScore = 1e30f; int bestOff = S_RA;
                    for (int o = 0; o < S_RA * 2; o++) {
                        float sad = 0; for (int j = 0; j < S_WIN; j++) sad += fabsf(sCache[o + j] - refM[j]);
                        float score = (sad * (1.0f + 0.15f * (float)abs(o - S_RA) / (float)S_RA)) / refE;
                        if (score < bestScore) { bestScore = score; bestOff = o; }
                    }
                    deck->MTPhaseOffset[k] = (float)(bestOff - S_RA);
                    deck->MTSearchTrigger[k] = false;
                    #undef S_WIN
                    #undef S_RA
                } else if (phase[k] > period * 0.3) {
                    deck->MTSearchTrigger[k] = true;
                }
            }
            double s0 = sin(M_PI * phase[0] / period);
            double w0 = s0 * s0; double w1 = 1.0 - w0;
            double rp0 = deck->Position + (phase[0] - period * 0.5) + deck->MTPhaseOffset[0];
            double rp1 = deck->Position + (phase[1] - period * 0.5) + deck->MTPhaseOffset[1];
            if (rp0 < 0) rp0 = 0; if (rp1 < 0) rp1 = 0;
            float l0, r0, l1, r1;
            INTERP_SAMPLES_PRO(rp0, l0, r0); INTERP_SAMPLES_PRO(rp1, l1, r1);
            l_sample = (l0 * (float)w0 + l1 * (float)w1);
            r_sample = (r0 * (float)w0 + r1 * (float)w1);
        } else {
            deck->MTOffset = 0;
            double readPos = deck->Position;
            if (readPos < 0) readPos = 0;
            INTERP_SAMPLES_PRO(readPos, l_sample, r_sample);
        }

        // EQ Gains
        float gainL = (deck->EqLow < 0.5f) ? (deck->EqLow * 2.0f) : (1.0f + (deck->EqLow - 0.5f) * 4.0f);
        float gainM = (deck->EqMid < 0.5f) ? (deck->EqMid * 2.0f) : (1.0f + (deck->EqMid - 0.5f) * 4.0f);
        float gainH = (deck->EqHigh < 0.5f) ? (deck->EqHigh * 2.0f) : (1.0f + (deck->EqHigh - 0.5f) * 4.0f);

        // professional Engine Linkwitz-Riley EQ Extraction
        float lowL = EngineLR4_Process(&deck->EqLowStateL, l_sample);
        float highL = EngineLR4_Process(&deck->EqHighStateL, l_sample);
        float midL = l_sample - lowL - highL;
        l_sample = (lowL * gainL) + (midL * gainM) + (highL * gainH);

        float lowR = EngineLR4_Process(&deck->EqLowStateR, r_sample);
        float highR = EngineLR4_Process(&deck->EqHighStateR, r_sample);
        float midR = r_sample - lowR - highR;
        r_sample = (lowR * gainL) + (midR * gainM) + (highR * gainH);

        l_sample *= deck->Trim; r_sample *= deck->Trim;

        // Color FX & Beat FX
        ColorFXManager_Process(&deck->ColorFX, &l_sample, &r_sample, l_sample, r_sample, fs);
        if (engine->BeatFX.targetChannel == deckIndex + 1) {
            BeatFXManager_Process(&engine->BeatFX, &l_sample, &r_sample, l_sample, r_sample, SAMPLE_RATE);
        }

        maxL = fmaxf(maxL, fabsf(l_sample)); maxR = fmaxf(maxR, fabsf(r_sample));
        
        // Output to Master if volume fader is up (assuming deck index or separate fader logic)
        // For now, let's assume fader is handled in Trim or separate mixer logic
        outMaster[i * 2] += l_sample; 
        outMaster[i * 2 + 1] += r_sample;

        // Output to Cue (Headphones) if Cue is active
        if (deck->IsCueActive) {
            outCue[i * 2] += l_sample;
            outCue[i * 2 + 1] += r_sample;
        }

        deck->Position += rate;
    }
    deck->LastRate = targetRate;
    deck->VuMeterL = deck->VuMeterL * 0.9f + maxL * 0.1f;
    deck->VuMeterR = deck->VuMeterR * 0.9f + maxR * 0.1f;
}

void AudioEngine_Process(AudioEngine *engine, float *outBuffer, int frames) {
    // outBuffer is expected to be interleaved 4-channel if PLATFORM_DRM is used, 
    // or we handle downmixing for stereo platforms.
    
    // Internal temporary buffers for mixing
    static float masterMix[4096 * 2];
    static float cueMix[4096 * 2];
    
    memset(masterMix, 0, frames * 2 * sizeof(float));
    memset(cueMix, 0, frames * 2 * sizeof(float));

    for (int i = 0; i < MAX_DECKS; i++) {
        ProcessDeckAudio(&engine->Decks[i], masterMix, cueMix, frames, engine, i);
    }

    // Apply Master FX
    for (int s = 0; s < frames; s++) {
        if (engine->BeatFX.targetChannel == 0) {
            BeatFXManager_Process(&engine->BeatFX, &masterMix[s*2], &masterMix[s*2+1], masterMix[s*2], masterMix[s*2+1], SAMPLE_RATE);
        }
        
        // Interleave to output
        // Channel 1-2: Master
        outBuffer[s*4] = fmaxf(-1.0f, fminf(1.0f, masterMix[s*2]));
        outBuffer[s*4 + 1] = fmaxf(-1.0f, fminf(1.0f, masterMix[s*2+1]));
        
        // Channel 3-4: Cue (Headphones)
        outBuffer[s*4 + 2] = fmaxf(-1.0f, fminf(1.0f, cueMix[s*2]));
        outBuffer[s*4 + 3] = fmaxf(-1.0f, fminf(1.0f, cueMix[s*2+1]));
    }
}

void DeckAudio_Play(DeckAudioState *deck) { deck->IsMotorOn = true; }
void DeckAudio_Stop(DeckAudioState *deck) { deck->IsMotorOn = false; }
void DeckAudio_SetPitch(DeckAudioState *deck, uint16_t pitch) { deck->Pitch = pitch; }
void DeckAudio_QueueJumpMs(DeckAudioState *deck, uint32_t targetMs, uint32_t waitMs) {
    deck->QueuedJumpMs = targetMs;
    deck->QueuedWaitSamples = (uint32_t)((float)waitMs * 44.1f);
    deck->HasQueuedJump = true;
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

void DeckAudio_JumpToMs(DeckAudioState *deck, int64_t ms) {
    deck->Position = (double)ms * ((double)deck->SampleRate / 1000.0);
    if (deck->Position * 2 >= (double)deck->TotalSamples) {
        deck->Position = (double)(deck->TotalSamples / 2) - 1.0;
    }
}
