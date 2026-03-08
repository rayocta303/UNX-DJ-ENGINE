#include "audio/engine.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif

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
        memset(&engine->Decks[i].EqLowStateL, 0, sizeof(BiquadState));
        memset(&engine->Decks[i].EqLowStateR, 0, sizeof(BiquadState));
        memset(&engine->Decks[i].EqMidStateL, 0, sizeof(BiquadState));
        memset(&engine->Decks[i].EqMidStateR, 0, sizeof(BiquadState));
        memset(&engine->Decks[i].EqHighStateL, 0, sizeof(BiquadState));
        memset(&engine->Decks[i].EqHighStateR, 0, sizeof(BiquadState));
        ColorFXManager_Init(&engine->Decks[i].ColorFX);
        engine->Decks[i].IsMotorOn = false;
        engine->Decks[i].IsPlaying = false;
        engine->Decks[i].MTSearchTrigger[0] = true;
        engine->Decks[i].MTSearchTrigger[1] = true;
    }
    
    BeatFXManager_Init(&engine->BeatFX);
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

// Simple Biquad Filter implementation for EQ (Isolator style)
// Processes a single sample through a biquad filter
static inline float processBiquad(float in, float b0, float b1, float b2, float a1, float a2, BiquadState *state) {
    float out = b0 * in + b1 * state->x1 + b2 * state->x2 - a1 * state->y1 - a2 * state->y2;
    state->x2 = state->x1;
    state->x1 = in;
    state->y2 = state->y1;
    state->y1 = out;
    return out;
}

// Calculate biquad coefficients for a Linkwitz-Riley crossover (used for DJ Isolators)
// Type: 0 = Lowpass, 1 = Highpass
static void calcCrossoverCoeffs(int type, float freq, float sampleRate, float *b0, float *b1, float *b2, float *a1, float *a2) {
    float omega = 2.0f * (float)M_PI * freq / sampleRate;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / (2.0f * 0.707f); // Q = 0.707 (Butterworth squared -> LR)

    float a0 = 1.0f + alpha;
    
    if (type == 0) { // Lowpass
        *b0 = (1.0f - cs) / 2.0f / a0;
        *b1 = (1.0f - cs) / a0;
        *b2 = (1.0f - cs) / 2.0f / a0;
    } else { // Highpass
        *b0 = (1.0f + cs) / 2.0f / a0;
        *b1 = -(1.0f + cs) / a0;
        *b2 = (1.0f + cs) / 2.0f / a0;
    }
    
    *a1 = -2.0f * cs / a0;
    *a2 = (1.0f - alpha) / a0;
}

// Resampling directly from PCM
static void ProcessDeckAudio(DeckAudioState *deck, float *outBuffer, int frames, AudioEngine *engine, int deckIndex) {
    if (!deck->PCMBuffer) return;

    // Handle Quantize Buffering Logic
    if (deck->HasQueuedJump) {
        if (deck->QueuedWaitSamples >= (uint32_t)frames) {
            deck->QueuedWaitSamples -= frames;
        } else {
            // Trigger exact jump & play once wait is depleted
            DeckAudio_JumpToMs(deck, deck->QueuedJumpMs);
            deck->IsMotorOn = true;
            deck->BaseRate = (float)deck->Pitch / 10000.0f;
            deck->OutlinedRate = deck->BaseRate;
            
            deck->HasQueuedJump = false;
            deck->QueuedWaitSamples = 0;
        }
    }

    ProcessDeckPhysics(deck);

    if (!deck->IsPlaying) return;

    double rate = deck->OutlinedRate;
    if (deck->IsReverse) {
        rate = -rate;
    }

    if (rate == 0.0) return;

    // --- EQ Coefficient Setup ---
    // Crossover frequencies (typical DJ mixer: Low/Mid split at 250Hz, Mid/High split at 2500Hz)
    float b0_lp1, b1_lp1, b2_lp1, a1_lp1, a2_lp1; // Low band (LPF @ 250Hz)
    float b0_hp1, b1_hp1, b2_hp1, a1_hp1, a2_hp1; // Mid band lower (HPF @ 250Hz)
    float b0_lp2, b1_lp2, b2_lp2, a1_lp2, a2_lp2; // Mid band upper (LPF @ 2500Hz)
    float b0_hp2, b1_hp2, b2_hp2, a1_hp2, a2_hp2; // High band (HPF @ 2500Hz)

    calcCrossoverCoeffs(0, 250.0f, deck->SampleRate, &b0_lp1, &b1_lp1, &b2_lp1, &a1_lp1, &a2_lp1);
    calcCrossoverCoeffs(1, 250.0f, deck->SampleRate, &b0_hp1, &b1_hp1, &b2_hp1, &a1_hp1, &a2_hp1);
    calcCrossoverCoeffs(0, 2500.0f, deck->SampleRate, &b0_lp2, &b1_lp2, &b2_lp2, &a1_lp2, &a2_lp2);
    calcCrossoverCoeffs(1, 2500.0f, deck->SampleRate, &b0_hp2, &b1_hp2, &b2_hp2, &a1_hp2, &a2_hp2);

    // Map 0.0-1.0 knob range to gain: 0.5 is 1x (0dB), 0.0 is 0x (-inf dB), 1.0 is ~3x (+9dB)
    float gainLow = (deck->EqLow < 0.5f) ? (deck->EqLow * 2.0f) : (1.0f + (deck->EqLow - 0.5f) * 4.0f);
    float gainMid = (deck->EqMid < 0.5f) ? (deck->EqMid * 2.0f) : (1.0f + (deck->EqMid - 0.5f) * 4.0f);
    float gainHigh = (deck->EqHigh < 0.5f) ? (deck->EqHigh * 2.0f) : (1.0f + (deck->EqHigh - 0.5f) * 4.0f);
    
    // Smooth kill curves: square the gain for sharper drop-off when turning down
    if (gainLow < 1.0f) gainLow *= gainLow;
    if (gainMid < 1.0f) gainMid *= gainMid;
    if (gainHigh < 1.0f) gainHigh *= gainHigh;

    // We process frame by frame
    for (int i = 0; i < frames; i++) {
        double readPos = deck->Position;
        // MT is active if Key Lock is ON, not scratching, motor is running, and speed is not precisely 1.0
        bool mtActive = deck->MasterTempoActive && !deck->IsTouching && deck->IsMotorOn && fabs(rate - 1.0) > 0.0005;

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

        if (mtActive) {
            deck->MTOffset += (1.0 - rate);
            
            if (deck->MTOffset > 65536.0) deck->MTOffset -= 65536.0;
            if (deck->MTOffset < -65536.0) deck->MTOffset += 65536.0;

            double period = 2048.0;
            double drift = deck->MTOffset;
            
            double phase[2];
            phase[0] = fmod(drift, period); if (phase[0] < 0) phase[0] += period;
            phase[1] = fmod(drift + period * 0.5, period); if (phase[1] < 0) phase[1] += period;

            // Synchronization Trigger: Only search ONCE when grain is silent (phase close to 0)
            for (int k = 0; k < 2; k++) {
                if (phase[k] < 16.0 && deck->MTSearchTrigger[k]) {
                    float bestCorr = -1e30f;
                    float bestOff = 0.0f;
                    // Search window: match against the OTHER grain which is currently at its peak (50% phase)
                    int other = 1 - k;
                    double otherRp = deck->Position + (phase[other] - period * 0.5) + deck->MTPhaseOffset[other];
                    
                    for (float o = -128.0f; o <= 128.0f; o += 2.0f) {
                        float corr = 0;
                        for (int j = -24; j < 24; j++) {
                            double tPos = deck->Position - (period * 0.5) + o + j;
                            if (tPos < 0) continue;
                            float tl, tr, bl, br;
                            INTERP_SAMPLES_HI(tPos, tl, tr);
                            INTERP_SAMPLES_HI(otherRp + j, bl, br);
                            corr += tl * bl + tr * br;
                        }
                        if (corr > bestCorr) { bestCorr = corr; bestOff = o; }
                    }
                    deck->MTPhaseOffset[k] = bestOff;
                    deck->MTSearchTrigger[k] = false; // Latched
                } else if (phase[k] > period * 0.2) {
                    deck->MTSearchTrigger[k] = true; // Rearm when grain is well into its life
                }
            }

            // Power-constant cross-fade for perfect smoothness (sin/cos windows)
            // w0 + w1 = 1.0 (approx) for amplitude, but sin^2 + cos^2 = 1.0 for power
            double w0 = pow(sin(M_PI * phase[0] / period), 2.0);
            double w1 = 1.0 - w0;

            double rp0 = deck->Position + (phase[0] - period * 0.5) + deck->MTPhaseOffset[0];
            double rp1 = deck->Position + (phase[1] - period * 0.5) + deck->MTPhaseOffset[1];

            if (rp0 < 0) rp0 = 0; if (rp1 < 0) rp1 = 0;
            
            float l0, r0, l1, r1;
            INTERP_SAMPLES_HI(rp0, l0, r0);
            INTERP_SAMPLES_HI(rp1, l1, r1);
            
            l_sample = (float)(l0 * w0 + l1 * w1);
            r_sample = (float)(r0 * w0 + r1 * w1);
        } else {
            deck->MTOffset = 0.0;
            deck->MTPhaseOffset[0] = 0; deck->MTPhaseOffset[1] = 0;
            deck->MTSearchTrigger[0] = true; deck->MTSearchTrigger[1] = true;
            if (readPos < 0) readPos = 0;
            INTERP_SAMPLES_HI(readPos, l_sample, r_sample);
        }

        // Apply Trim Volume
        l_sample *= deck->Trim;
        r_sample *= deck->Trim;

        // --- Apply EQ (Isolator Mode) ---
        // Right now we approximate the 3 bands. 
        // 1. Extract Low band
        float lowL = processBiquad(l_sample, b0_lp1, b1_lp1, b2_lp1, a1_lp1, a2_lp1, &deck->EqLowStateL);
        float lowR = processBiquad(r_sample, b0_lp1, b1_lp1, b2_lp1, a1_lp1, a2_lp1, &deck->EqLowStateR);
        
        // 2. Extract High band
        float highL = processBiquad(l_sample, b0_hp2, b1_hp2, b2_hp2, a1_hp2, a2_hp2, &deck->EqHighStateL);
        float highR = processBiquad(r_sample, b0_hp2, b1_hp2, b2_hp2, a1_hp2, a2_hp2, &deck->EqHighStateR);
        
        // 3. Extract Mid band (Original - Low - High)
        // This is a phase-accurate way to extract mids if crossovers are perfectly aligned,
        // preventing phase cancellation weirdness when knobs are at 12 o'clock.
        float midL = l_sample - lowL - highL;
        float midR = r_sample - lowR - highR;

        // Mix back
        l_sample = (lowL * gainLow) + (midL * gainMid) + (highL * gainHigh);
        r_sample = (lowR * gainLow) + (midR * gainMid) + (highR * gainHigh);

        // --- Sound Color FX ---
        ColorFXManager_Process(&deck->ColorFX, &l_sample, &r_sample, l_sample, r_sample, deck->SampleRate);

        // --- Beat FX (Per-Deck Routing) ---
        if (engine->BeatFX.targetChannel == deckIndex + 1) {
            BeatFXManager_Process(&engine->BeatFX, &l_sample, &r_sample, l_sample, r_sample, SAMPLE_RATE);
        }

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
        ProcessDeckAudio(&engine->Decks[i], outBuffer, frames, engine, i);
    }
    
    // Process Beat FX on Master 
    for (int s = 0; s < frames; s++) {
        float lL = outBuffer[s * CHANNELS];
        float lR = outBuffer[s * CHANNELS + 1];
        
        if (engine->BeatFX.targetChannel == 0) {
            BeatFXManager_Process(&engine->BeatFX, &lL, &lR, lL, lR, SAMPLE_RATE);
        }
        
        // Hard clipper logic to prevent master out overload
        if (lL > 1.0f) lL = 1.0f;
        if (lL < -1.0f) lL = -1.0f;
        if (lR > 1.0f) lR = 1.0f;
        if (lR < -1.0f) lR = -1.0f;

        outBuffer[s * CHANNELS] = lL;
        outBuffer[s * CHANNELS + 1] = lR;
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

void DeckAudio_QueueJumpMs(DeckAudioState *deck, uint32_t targetMs, uint32_t waitMs) {
    deck->QueuedJumpMs = targetMs;
    // Translate waitMs to purely audio sample frames to be processed later
    deck->QueuedWaitSamples = (uint32_t)((double)waitMs * (44100.0 / 1000.0));
    deck->HasQueuedJump = true;
}
