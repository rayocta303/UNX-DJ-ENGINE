#include "audio/engine.h"
#include "engine/util/engine_math.h"
#include "SoundTouch.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>

#define MINIMP3_API static
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

#define DRWAV_API static
#define DRWAV_PRIVATE static
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

using namespace soundtouch;

void AudioEngine_Init(AudioEngine *engine, uint32_t outputSampleRate) {
    memset(engine, 0, sizeof(AudioEngine));
    engine->OutputSampleRate = outputSampleRate;
    engine->MasterVolume = 1.0f;

    for (int i = 0; i < MAX_DECKS; i++) {
        DeckAudioState *deck = &engine->Decks[i];
        deck->BaseRate = 1.0f;
        deck->OutlinedRate = 0.0f; 
        deck->Pitch = 10000;
        deck->Trim = 0.5f; // Calibrated 12 o'clock to -6dB for headroom
        deck->Fader = 1.0f;
        deck->EqLow = 0.5f;
        deck->EqMid = 0.5f;
        deck->EqHigh = 0.5f;
        EngineLR4_Init(&deck->EqLowStateL);
        EngineLR4_Init(&deck->EqLowStateR);
        EngineLR4_Init(&deck->EqHighStateL);
        EngineLR4_Init(&deck->EqHighStateR);
        ColorFXManager_Init(&deck->ColorFX);
        deck->IsMotorOn = false;
        deck->IsPlaying = false;

        // Initialize SoundTouch with Balanced parameters (Optimized for Mobile/DJ)
        SoundTouch *st = new SoundTouch();
        st->setSampleRate(engine->OutputSampleRate);
        st->setChannels(CHANNELS);
        st->setSetting(SETTING_USE_QUICKSEEK, 1);     // Performance boost for mobile
        st->setSetting(SETTING_USE_AA_FILTER, 1);
        st->setSetting(SETTING_SEQUENCE_MS, 40);      // Balanced resolution/latency
        st->setSetting(SETTING_SEEKWINDOW_MS, 15);    // Reduced CPU load for mobile
        st->setSetting(SETTING_OVERLAP_MS, 8);        // Standard overlap for smoothness
        deck->SoundTouchHandle = (void*)st;
    }
    
    BeatFXManager_Init(&engine->BeatFX);
    engine->Crossfader = 0.0f;
}

void AudioEngine_SetOutputSampleRate(AudioEngine *engine, uint32_t sampleRate) {
    if (sampleRate == 0) return;
    engine->OutputSampleRate = sampleRate;
    for (int i = 0; i < MAX_DECKS; i++) {
        if (engine->Decks[i].SoundTouchHandle) {
            ((SoundTouch*)engine->Decks[i].SoundTouchHandle)->setSampleRate(sampleRate);
        }
    }
}

void AudioEngine_SetPCMBitDepth(AudioEngine *engine, int bitDepth) {
    for (int i = 0; i < MAX_DECKS; i++) {
        if (engine->Decks[i].BitDepth != bitDepth) {
            engine->Decks[i].BitDepth = bitDepth;
            // Auto-reinit audio if a track is loaded
            if (engine->Decks[i].FilePath[0] != '\0') {
                double currentPos = engine->Decks[i].Position;
                bool wasPlaying = engine->Decks[i].IsPlaying;
                DeckAudio_LoadTrack(&engine->Decks[i], engine->Decks[i].FilePath);
                engine->Decks[i].Position = currentPos;
                engine->Decks[i].IsPlaying = wasPlaying;
            }
        }
    }
}

void AudioEngine_Destroy(AudioEngine *engine) {
    for (int i = 0; i < MAX_DECKS; i++) {
        if (engine->Decks[i].SoundTouchHandle) {
            delete (SoundTouch*)engine->Decks[i].SoundTouchHandle;
            engine->Decks[i].SoundTouchHandle = NULL;
        }
    }
}

void DeckAudio_LoadTrack(DeckAudioState *deck, const char *filePath) {
    if (deck->PCMBuffer) {
        void *oldBuf = deck->PCMBuffer;
        deck->PCMBuffer = NULL;
        deck->TotalSamples = 0;
        free(oldBuf);
    }
    deck->Position = 0;
    deck->MT_ReadPos = 0;
    deck->IsPlaying = false;
    deck->SampleRate = 44100;
    deck->IsMotorOn = false;
    deck->IsTouching = false;
    deck->VinylModeEnabled = true;
    deck->OutlinedRate = 0;
    deck->JogRate = 0;

    if (deck->SoundTouchHandle) {
        ((SoundTouch*)deck->SoundTouchHandle)->clear();
    }
    strncpy(deck->FilePath, filePath, 511);
    deck->FilePath[511] = '\0';

    if (!filePath || strlen(filePath) == 0) return;

    const char *ext = strrchr(filePath, '.');
    bool isWav = (ext && (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".aif") == 0 || strcasecmp(ext, ".aiff") == 0));

    if (isWav) {
        unsigned int channels;
        unsigned int sampleRate;
        drwav_uint64 totalPCMFrameCount;
        
        if (deck->BitDepth == 24) {
            int32_t *pSampleData = drwav_open_file_and_read_pcm_frames_s32(filePath, &channels, &sampleRate, &totalPCMFrameCount, NULL);
            if (pSampleData) {
                if (channels == 1) {
                    int32_t *stereoBuf = (int32_t *)malloc(totalPCMFrameCount * 2 * sizeof(int32_t));
                    if (stereoBuf) {
                        for (drwav_uint64 i = 0; i < totalPCMFrameCount; i++) {
                            stereoBuf[i*2] = pSampleData[i];
                            stereoBuf[i*2 + 1] = pSampleData[i];
                        }
                        drwav_free(pSampleData, NULL);
                        deck->PCMBuffer = stereoBuf;
                        deck->TotalSamples = totalPCMFrameCount * 2;
                    } else {
                        drwav_free(pSampleData, NULL);
                        deck->PCMBuffer = NULL;
                        deck->TotalSamples = 0;
                    }
                } else {
                    deck->PCMBuffer = pSampleData;
                    deck->TotalSamples = totalPCMFrameCount * channels;
                }
                deck->SampleRate = sampleRate;
            }
        } else {
            int16_t *pSampleData = drwav_open_file_and_read_pcm_frames_s16(filePath, &channels, &sampleRate, &totalPCMFrameCount, NULL);
            if (pSampleData) {
                if (channels == 1) {
                    int16_t *stereoBuf = (int16_t *)malloc(totalPCMFrameCount * 2 * sizeof(int16_t));
                    if (stereoBuf) {
                        for (drwav_uint64 i = 0; i < totalPCMFrameCount; i++) {
                            stereoBuf[i*2] = pSampleData[i];
                            stereoBuf[i*2 + 1] = pSampleData[i];
                        }
                        drwav_free(pSampleData, NULL);
                        deck->PCMBuffer = stereoBuf;
                        deck->TotalSamples = totalPCMFrameCount * 2;
                    } else {
                        drwav_free(pSampleData, NULL);
                        deck->PCMBuffer = NULL;
                        deck->TotalSamples = 0;
                    }
                } else {
                    deck->PCMBuffer = pSampleData;
                    deck->TotalSamples = totalPCMFrameCount * channels;
                }
                deck->SampleRate = sampleRate;
            }
        }
    } else {
        mp3dec_t mp3d;
        mp3dec_file_info_t info;
        int res = mp3dec_load(&mp3d, filePath, &info, NULL, NULL);

        if (res == 0) {
            if (deck->BitDepth == 24) {
                // Convert 16-bit MP3 samples to 24-bit (stored in 32-bit int)
                int32_t *buf24 = (int32_t *)malloc(info.samples * (info.channels == 1 ? 2 : 1) * sizeof(int32_t));
                if (buf24) {
                    if (info.channels == 1) {
                        for (size_t i = 0; i < info.samples; i++) {
                            int32_t s = (int32_t)info.buffer[i] << 16;
                            buf24[i*2] = s;
                            buf24[i*2 + 1] = s;
                        }
                    } else {
                        for (size_t i = 0; i < info.samples; i++) {
                            buf24[i] = (int32_t)info.buffer[i] << 16;
                        }
                    }
                    free(info.buffer);
                    deck->PCMBuffer = buf24;
                    deck->TotalSamples = info.samples * (info.channels == 1 ? 2 : 1);
                } else {
                    free(info.buffer);
                    deck->PCMBuffer = NULL;
                    deck->TotalSamples = 0;
                }
            } else {
                if (info.channels == 1) {
                    int16_t *stereoBuf = (int16_t *)malloc(info.samples * 2 * sizeof(int16_t));
                    if (stereoBuf) {
                        for (size_t i = 0; i < info.samples; i++) {
                            stereoBuf[i*2] = info.buffer[i];
                            stereoBuf[i*2 + 1] = info.buffer[i];
                        }
                        free(info.buffer);
                        deck->PCMBuffer = stereoBuf;
                        deck->TotalSamples = info.samples * 2;
                    } else {
                        free(info.buffer);
                        deck->PCMBuffer = NULL;
                        deck->TotalSamples = 0;
                    }
                } else {
                    deck->PCMBuffer = info.buffer;
                    deck->TotalSamples = info.samples;
                }
            }
            deck->SampleRate = info.hz;
        }
    }
}

static void ProcessDeckPhysics(DeckAudioState *deck) {
    deck->BaseRate = (float)deck->Pitch / 10000.0f;
    double targetRate = 0.0;
    float accel = 0.08f; 

    if (deck->IsTouching) {
        targetRate = (deck->VinylModeEnabled) ? deck->JogRate : (deck->BaseRate + deck->JogRate);
        accel = (deck->VinylModeEnabled) ? 1.0f : 0.4f;
    } else {
        if (deck->IsMotorOn) {
            targetRate = deck->BaseRate;
            accel = deck->VinylStartAccel > 0 ? deck->VinylStartAccel : 0.12f;
        } else {
            targetRate = 0.0;
            accel = deck->VinylStopAccel > 0 ? deck->VinylStopAccel : 0.015f;
        }
    }

    if (deck->OutlinedRate != targetRate) {
        float diff = targetRate - (float)deck->OutlinedRate;
        if (fabs(diff) < accel) deck->OutlinedRate = targetRate;
        else deck->OutlinedRate += (diff > 0) ? accel : -accel;
    }
    deck->IsPlaying = (fabs(deck->OutlinedRate) > 0.001);
}

static inline float SampleToFloat(void* buffer, int index, int bitDepth) {
    if (bitDepth == 24) {
        return (float)((int32_t*)buffer)[index] / 2147483648.0f;
    } else {
        return (float)((int16_t*)buffer)[index] / 32768.0f;
    }
}

static inline void AudioEngine_GetSampleDirect(DeckAudioState* deck, int i, float* l, float* r) {
    if (i < 0 || i >= (int)(deck->TotalSamples / 2)) { *l = 0; *r = 0; return; }
    *l = SampleToFloat(deck->PCMBuffer, i * 2, deck->BitDepth);
    *r = SampleToFloat(deck->PCMBuffer, i * 2 + 1, deck->BitDepth);
}

static inline void AudioEngine_GetSample(DeckAudioState* deck, double pos, float* l, float* r) {
    if (pos < 2.0) { *l = 0; *r = 0; return; }
    int i = (int)pos;
    float f = (float)(pos - i);
    if (i >= (int)((deck->TotalSamples / 2) - 3)) { *l = 0; *r = 0; return; }

    float y0_l = SampleToFloat(deck->PCMBuffer, (i - 1) * 2, deck->BitDepth);
    float y1_l = SampleToFloat(deck->PCMBuffer, i * 2, deck->BitDepth);
    float y2_l = SampleToFloat(deck->PCMBuffer, (i + 1) * 2, deck->BitDepth);
    float y3_l = SampleToFloat(deck->PCMBuffer, (i + 2) * 2, deck->BitDepth);

    float y0_r = SampleToFloat(deck->PCMBuffer, (i - 1) * 2 + 1, deck->BitDepth);
    float y1_r = SampleToFloat(deck->PCMBuffer, i * 2 + 1, deck->BitDepth);
    float y2_r = SampleToFloat(deck->PCMBuffer, (i + 1) * 2 + 1, deck->BitDepth);
    float y3_r = SampleToFloat(deck->PCMBuffer, (i + 2) * 2 + 1, deck->BitDepth);

    if (f < 0.0001f) {
        *l = y1_l;
        *r = y1_r;
    } else {
        *l = Engine_InterpolateHermite4(f, y0_l, y1_l, y2_l, y3_l);
        *r = Engine_InterpolateHermite4(f, y0_r, y1_r, y2_r, y3_r);
    }
}

static void ProcessDeckAudio(DeckAudioState* deck, float* outMaster, float* outCue, int frames, AudioEngine* engine, int deckIndex) {
    bool noiseActive = (deck->ColorFX.activeFX == COLORFX_NOISE && deck->ColorFX.colorValue != 0.0f);
    if (!deck->PCMBuffer && !noiseActive) return;

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
    static bool wasMTActive[MAX_DECKS] = {false};

    bool hasActiveFX = (deck->ColorFX.activeFX != COLORFX_NONE && deck->ColorFX.colorValue != 0.0f) || 
                       (engine->BeatFX.targetChannel == deckIndex + 1 && engine->BeatFX.isFxOn);
    
    // Check for tails: even if FX is OFF or deck is STOPPED, we continue if VU is still audible
    bool hasTails = (deck->VuMeterL > 0.0001f || deck->VuMeterR > 0.0001f);

    if (!deck->IsPlaying && !noiseActive && !hasActiveFX && !hasTails) {
        // Fully silent and stopped
        if (deck->MasterTempoActive && deck->SoundTouchHandle && wasMTActive[deckIndex]) {
            ((SoundTouch*)deck->SoundTouchHandle)->clear();
            wasMTActive[deckIndex] = false;
        }
        return;
    }

    float fs = (float)engine->OutputSampleRate;
    if (deck->LastRate == 0) deck->LastRate = deck->OutlinedRate;
    double targetRate = deck->OutlinedRate;
    
    // EQ Setup
    static float lastFreqL[MAX_DECKS] = {0};
    static float lastFreqH[MAX_DECKS] = {0};
    static uint32_t lastSR[MAX_DECKS] = {0};

    if (lastFreqL[deckIndex] != 350.0f || lastSR[deckIndex] != engine->OutputSampleRate) {
        EngineLR4_SetLowpass(&deck->EqLowStateL, 350.0f, engine->OutputSampleRate);
        EngineLR4_SetLowpass(&deck->EqLowStateR, 350.0f, engine->OutputSampleRate);
        lastFreqL[deckIndex] = 350.0f;
    }
    if (lastFreqH[deckIndex] != 2500.0f || lastSR[deckIndex] != engine->OutputSampleRate) {
        EngineLR4_SetHighpass(&deck->EqHighStateL, 2500.0f, engine->OutputSampleRate);
        EngineLR4_SetHighpass(&deck->EqHighStateR, 2500.0f, engine->OutputSampleRate);
        lastFreqH[deckIndex] = 2500.0f;
    }
    lastSR[deckIndex] = engine->OutputSampleRate;

    float gainL = (deck->EqLow < 0.5f) ? (deck->EqLow * 2.0f) : (1.0f + (deck->EqLow - 0.5f) * 4.0f);
    float gainM = (deck->EqMid < 0.5f) ? (deck->EqMid * 2.0f) : (1.0f + (deck->EqMid - 0.5f) * 4.0f);
    float gainH = (deck->EqHigh < 0.5f) ? (deck->EqHigh * 2.0f) : (1.0f + (deck->EqHigh - 0.5f) * 4.0f);

    SoundTouch *st = (SoundTouch*)deck->SoundTouchHandle;
    float maxL = 0, maxR = 0;
    float sampleRateRatio = (float)deck->SampleRate / (float)engine->OutputSampleRate;

    if (deck->MasterTempoActive && !deck->IsTouching && st && fabs(targetRate) > 0.01) {
        // --- MASTER TEMPO MODE (SOUNDTOUCH) ---
        // Note: MT_ReadPos is synced in JumpToMs and LoadTrack. 
        // We only need to handle the initial MT activation here.
        if (!wasMTActive[deckIndex]) {
            deck->MT_ReadPos = deck->Position;
            st->clear();
            wasMTActive[deckIndex] = true;
        }

        static double lastTempo[MAX_DECKS] = {0};
        double absRate = fabs(targetRate);
        double effectiveTempo = absRate * (double)sampleRateRatio;
        if (fabs(lastTempo[deckIndex] - effectiveTempo) > 0.0001) {
            st->setTempo(effectiveTempo);
            lastTempo[deckIndex] = effectiveTempo;
        }
        st->setPitch(1.0); 

        // Feed samples to SoundTouch until we have enough output
        // We use a safety margin to ensure st->receiveSamples gets what it needs
        int maxIterations = 15; 
        while (st->numSamples() < (uint32_t)frames && maxIterations-- > 0) {
            float inBuf[512 * 2];
            int toRead = 512;
            
            for (int j = 0; j < toRead; j++) {
                // Use interpolation if position is fractional for high quality feeding
                AudioEngine_GetSample(deck, deck->MT_ReadPos, &inBuf[j*2], &inBuf[j*2+1]);
                deck->MT_ReadPos += (targetRate > 0) ? 1.0 : -1.0;
                
                if (deck->IsLooping && deck->MT_ReadPos >= deck->LoopEndPos) {
                    double diff = deck->MT_ReadPos - deck->LoopEndPos;
                    deck->MT_ReadPos = deck->LoopStartPos + diff;
                }
            }
            st->putSamples(inBuf, toRead);
        }

        float outBuf[frames * 2];
        uint32_t received = st->receiveSamples(outBuf, frames);
        
        if (received < (uint32_t)frames) {
            memset(outBuf + received * 2, 0, (frames - received) * 2 * sizeof(float));
            received = frames;
        }

        // Logical position update: Accurate linear progression
        deck->Position += (double)received * targetRate;
        if (deck->IsLooping && deck->Position >= deck->LoopEndPos) {
            double diff = deck->Position - deck->LoopEndPos;
            deck->Position = deck->LoopStartPos + diff;
        }

        if (deck->SlipActive) {
            deck->SlipPosition += (double)received * targetRate;
        }

        for (int i = 0; i < (int)received; i++) {
            float l = outBuf[i*2], r = outBuf[i*2+1];
            
            // EQ & FX
            float lowL = EngineLR4_Process(&deck->EqLowStateL, l);
            float highL = EngineLR4_Process(&deck->EqHighStateL, l);
            l = (lowL * gainL) + (l - lowL - highL) * gainM + (highL * gainH);
            
            float lowR = EngineLR4_Process(&deck->EqLowStateR, r);
            float highR = EngineLR4_Process(&deck->EqHighStateR, r);
            r = (lowR * gainL) + (r - lowR - highR) * gainM + (highR * gainH);

            l *= deck->Trim; r *= deck->Trim;
            ColorFXManager_Process(&deck->ColorFX, &l, &r, l, r, fs);

            // Cue & VU (Pre-Fader)
            if (deck->IsCueActive) { 
                outCue[i*2] += l; outCue[i*2+1] += r; 
            }
            maxL = fmaxf(maxL, fabsf(l)); maxR = fmaxf(maxR, fabsf(r));

            // Fader (Post-Cue)
            l *= deck->Fader; r *= deck->Fader;

            if (engine->BeatFX.targetChannel == deckIndex + 1) {
                BeatFXManager_Process(&engine->BeatFX, &l, &r, l, r, engine->OutputSampleRate);
            }

            outMaster[i*2] += l; outMaster[i*2+1] += r;
        }
    } else {
        // --- NORMAL MODE (PITCH CHANGE) ---
        if (wasMTActive[deckIndex]) {
            if (st) st->clear();
            wasMTActive[deckIndex] = false;
        }

        double currentRate = deck->LastRate;
        double rateDelta = (targetRate - currentRate) / (double)frames;

        bool isNormalSpeed = (fabs(currentRate - 1.0) < 0.0001 && fabs(rateDelta) < 0.00001);
        
        for (int i = 0; i < frames; i++) {
            currentRate += rateDelta;
            float l, r;
            
            if (isNormalSpeed && fabs(sampleRateRatio - 1.0f) < 0.0001f) {
                AudioEngine_GetSampleDirect(deck, (int)deck->Position, &l, &r);
                deck->Position += 1.0;
            } else {
                AudioEngine_GetSample(deck, deck->Position, &l, &r);
                deck->Position += currentRate * (double)sampleRateRatio;
            }

            if (deck->IsLooping && deck->Position >= deck->LoopEndPos) {
                double diff = deck->Position - deck->LoopEndPos;
                deck->Position = deck->LoopStartPos + diff;
            }

            if (deck->SlipActive) {
                deck->SlipPosition += currentRate * (double)sampleRateRatio;
            }
            float lowL = EngineLR4_Process(&deck->EqLowStateL, l);
            float highL = EngineLR4_Process(&deck->EqHighStateL, l);
            l = (lowL * gainL) + (l - lowL - highL) * gainM + (highL * gainH);
            
            float lowR = EngineLR4_Process(&deck->EqLowStateR, r);
            float highR = EngineLR4_Process(&deck->EqHighStateR, r);
            r = (lowR * gainL) + (r - lowR - highR) * gainM + (highR * gainH);

            l *= deck->Trim; r *= deck->Trim;
            ColorFXManager_Process(&deck->ColorFX, &l, &r, l, r, fs);

            // Cue & VU (Pre-Fader)
            if (deck->IsCueActive) { 
                outCue[i*2] += l; outCue[i*2+1] += r; 
            }
            maxL = fmaxf(maxL, fabsf(l)); maxR = fmaxf(maxR, fabsf(r));

            // Fader (Post-Cue)
            l *= deck->Fader; r *= deck->Fader;

            if (engine->BeatFX.targetChannel == deckIndex + 1) {
                BeatFXManager_Process(&engine->BeatFX, &l, &r, l, r, engine->OutputSampleRate);
            }

            outMaster[i*2] += l; outMaster[i*2+1] += r;
        }
    }

    deck->LastRate = targetRate;
    
    // VU Meter Calibration & Ballistics
    // Scaling: 0.5 (unity) peak -> 0.8 VU (First Orange segment)
    float peakL = maxL * 1.6f;
    float peakR = maxR * 1.6f;
    if (peakL > 1.0f) peakL = 1.0f;
    if (peakR > 1.0f) peakR = 1.0f;

    // Instant attack, smooth decay
    if (peakL > deck->VuMeterL) deck->VuMeterL = peakL;
    else deck->VuMeterL = deck->VuMeterL * 0.92f + peakL * 0.08f;

    if (peakR > deck->VuMeterR) deck->VuMeterR = peakR;
    else deck->VuMeterR = deck->VuMeterR * 0.92f + peakR * 0.08f;
}

void AudioEngine_Process(AudioEngine *engine, float *outBuffer, int frames) {
    static float masterMix[4096 * 2];
    static float cueMix[4096 * 2];
    memset(masterMix, 0, frames * 2 * sizeof(float));
    memset(cueMix, 0, frames * 2 * sizeof(float));

    for (int i = 0; i < MAX_DECKS; i++) {
        ProcessDeckAudio(&engine->Decks[i], masterMix, cueMix, frames, engine, i);
    }

    float mPeakL = 0, mPeakR = 0;
    for (int s = 0; s < frames; s++) {
        if (engine->BeatFX.targetChannel == 0) {
            BeatFXManager_Process(&engine->BeatFX, &masterMix[s*2], &masterMix[s*2+1], masterMix[s*2], masterMix[s*2+1], engine->OutputSampleRate);
        }

        // --- Master Gain & Soft Limiting ---
        float l = masterMix[s*2] * engine->MasterVolume;
        float r = masterMix[s*2+1] * engine->MasterVolume;
        
        mPeakL = fmaxf(mPeakL, fabsf(l));
        mPeakR = fmaxf(mPeakR, fabsf(r));

        // Transparent Hard Limiter (Clamping)
        // This ensures zero distortion for signals <= 0dB (1.0)
        if (l > 1.0f) l = 1.0f; else if (l < -1.0f) l = -1.0f;
        if (r > 1.0f) r = 1.0f; else if (r < -1.0f) r = -1.0f;

        outBuffer[s*4] = l;
        outBuffer[s*4 + 1] = r;

        outBuffer[s*4 + 2] = fmaxf(-1.0f, fminf(1.0f, cueMix[s*2]));
        outBuffer[s*4 + 3] = fmaxf(-1.0f, fminf(1.0f, cueMix[s*2+1]));
    }

    // Master VU Ballistics
    // Scaling: 0.5 (unity) peak -> 0.7 VU (Healthy level)
    float pML = mPeakL * 1.4f;
    float pMR = mPeakR * 1.4f;
    if (pML > 1.0f) pML = 1.0f; if (pMR > 1.0f) pMR = 1.0f;
    if (pML > engine->MasterVuL) engine->MasterVuL = pML;
    else engine->MasterVuL = engine->MasterVuL * 0.92f + pML * 0.08f;
    if (pMR > engine->MasterVuR) engine->MasterVuR = pMR;
    else engine->MasterVuR = engine->MasterVuR * 0.92f + pMR * 0.08f;
}

void DeckAudio_Play(DeckAudioState *deck) { deck->IsMotorOn = true; }
void DeckAudio_Stop(DeckAudioState *deck) { deck->IsMotorOn = false; }
void DeckAudio_SetPitch(DeckAudioState *deck, uint16_t pitch) { deck->Pitch = pitch; }
void DeckAudio_QueueJumpMs(DeckAudioState *deck, uint32_t targetMs, uint32_t waitMs) {
    deck->QueuedJumpMs = targetMs;
    deck->QueuedWaitSamples = (uint32_t)((float)waitMs * 44.1f);
    deck->HasQueuedJump = true;
}
void DeckAudio_SetPlaying(DeckAudioState *deck, bool playing) { deck->IsMotorOn = playing; }
void DeckAudio_InstantPlay(DeckAudioState *deck) {
    deck->IsMotorOn = true;
    deck->BaseRate = (float)deck->Pitch / 10000.0f;
    deck->OutlinedRate = deck->BaseRate;
}
void DeckAudio_SetJogRate(DeckAudioState *deck, double rate) { if (deck->IsTouching) deck->JogRate = rate; }
void DeckAudio_SetJogTouch(DeckAudioState *deck, bool touching) {
    deck->IsTouching = touching;
    if (!touching) deck->JogRate = deck->OutlinedRate;
}
void DeckAudio_JumpToMs(DeckAudioState *deck, int64_t ms) {
    deck->Position = (double)ms * ((double)deck->SampleRate / 1000.0);
    deck->MT_ReadPos = deck->Position;
    if (deck->Position * 2 >= (double)deck->TotalSamples) {
        deck->Position = (double)(deck->TotalSamples / 2) - 1.0;
        deck->MT_ReadPos = deck->Position;
    }
    if (deck->MasterTempoActive && deck->SoundTouchHandle) ((SoundTouch*)deck->SoundTouchHandle)->clear();
}

void DeckAudio_SetSlip(DeckAudioState *deck, bool active) {
    if (active && !deck->SlipActive) {
        deck->SlipPosition = deck->Position;
    } else if (!active && deck->SlipActive) {
        deck->Position = deck->SlipPosition;
        deck->MT_ReadPos = deck->Position;
        if (deck->SoundTouchHandle) {
            ((soundtouch::SoundTouch*)deck->SoundTouchHandle)->clear();
        }
    }
    deck->SlipActive = active;
}

void DeckAudio_SetLoop(DeckAudioState *deck, bool active, double startPos, double endPos) {
    deck->LoopStartPos = startPos;
    deck->LoopEndPos = endPos;
    deck->IsLooping = active;
    
    if (active && deck->Position >= endPos) {
        deck->Position = startPos;
        deck->MT_ReadPos = startPos;
        if (deck->SoundTouchHandle) {
            ((soundtouch::SoundTouch*)deck->SoundTouchHandle)->clear();
        }
    }
}

void DeckAudio_ExitLoop(DeckAudioState *deck) {
    deck->IsLooping = false;
}
