#include "audio/engine.h"
#include "SoundTouch.h"
#include "engine/util/engine_math.h"
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>

#define MINIMP3_API static
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"
#include "core/logger.h"
#include <chrono>
#include <thread>

#define DRWAV_API static
#define DRWAV_PRIVATE static
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

using namespace soundtouch;

void DeckAudio_LoadTrackAsync(DeckAudioState *deck, const char *filePath) {
  // Increment LoadID and capture it for this thread
  uint32_t myID = ++(deck->LastLoadID);
  
  deck->IsLoading = true;
  deck->LoadingProgress = 0.0f;

  std::string path = filePath;
  std::thread([deck, path, myID]() {
    DeckAudio_LoadTrack(deck, path.c_str());
    
    // Only clear IsLoading if this was the latest request
    if (deck->LastLoadID == myID) {
      deck->IsLoading = false;
      deck->LoadingProgress = 1.0f;
    } else {
      // Stale request, but DeckAudio_LoadTrack already modified the deck.
      // This is still a bit problematic because DeckAudio_LoadTrack modifies state.
      // However, if DeckAudio_LoadTrack(deck, path) is called for the next track,
      // it will overwrite whatever this thread did.
      // The race condition happens if the second thread finishes BEFORE the first.
    }
  }).detach();
}

void AudioEngine_Init(AudioEngine *engine, uint32_t outputSampleRate) {
  memset(engine, 0, sizeof(AudioEngine));
  engine->OutputSampleRate = outputSampleRate;
  engine->MasterVolume = 1.0f;
  engine->RoutingMode =
      FX_ROUTING_POST_FADER; // Default to Professional DJM-Style

  for (int i = 0; i < MAX_DECKS; i++) {
    DeckAudioState *deck = &engine->Decks[i];
    deck->BaseRate = 1.0f;
    deck->OutlinedRate = 0.0f;
    deck->Pitch = 10000;
    deck->Trim = 0.5f; // Calibrated 12 o'clock to -6dB for headroom
    deck->Fader = 1.0f;
    deck->LastFader = 1.0f;
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
    st->setSetting(SETTING_USE_QUICKSEEK, 1); // Performance boost for mobile
    st->setSetting(SETTING_USE_AA_FILTER, 1);
    st->setSetting(SETTING_SEQUENCE_MS, 40);   // Balanced resolution/latency
    st->setSetting(SETTING_SEEKWINDOW_MS, 15); // Reduced CPU load for mobile
    st->setSetting(SETTING_OVERLAP_MS, 8); // Standard overlap for smoothness
    deck->SoundTouchHandle = (void *)st;
    deck->OutputSampleRate = engine->OutputSampleRate;
  }

  BeatFXManager_Init(&engine->BeatFX);
  engine->Crossfader = 0.0f;
  engine->LastCrossfader = 0.0f;

  UNX_LOG_INFO("=== AUDIO ENGINE SPECIFICS ===");
  UNX_LOG_INFO("Sample Rate : %u Hz", engine->OutputSampleRate);
  UNX_LOG_INFO("Channels    : %d", CHANNELS);
  UNX_LOG_INFO("API         : Raylib Audio (Internal)");
  UNX_LOG_INFO("=== END AUDIO ENGINE SPECIFICS ===");
}

void AudioEngine_SetOutputSampleRate(AudioEngine *engine, uint32_t sampleRate) {
  if (sampleRate == 0)
    return;
  engine->OutputSampleRate = sampleRate;
  for (int i = 0; i < MAX_DECKS; i++) {
    if (engine->Decks[i].SoundTouchHandle) {
      ((SoundTouch *)engine->Decks[i].SoundTouchHandle)
          ->setSampleRate(sampleRate);
    }
    engine->Decks[i].OutputSampleRate = sampleRate;
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
      delete (SoundTouch *)engine->Decks[i].SoundTouchHandle;
      engine->Decks[i].SoundTouchHandle = NULL;
    }
  }
}

void DeckAudio_LoadTrack(DeckAudioState *deck, const char *filePath) {
  uint32_t myID = deck->LastLoadID;
  
  if (!filePath || strlen(filePath) == 0) {
    if (deck->PCMBuffer) {
      void *oldBuf = deck->PCMBuffer;
      deck->PCMBuffer = NULL;
      deck->TotalSamples = 0;
      free(oldBuf);
    }
    return;
  }

  // Decode into local variables first to prevent race conditions during long decoding
  void* localPCM = NULL;
  uint32_t localSamples = 0;
  uint32_t localRate = 44100;

  const char *ext = strrchr(filePath, '.');
  bool isWav =
      (ext && (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".aif") == 0 ||
               strcasecmp(ext, ".aiff") == 0));

  if (isWav) {
    unsigned int channels;
    unsigned int sampleRate;
    drwav_uint64 totalPCMFrameCount;

    if (deck->BitDepth == 24) {
      int32_t *pSampleData = drwav_open_file_and_read_pcm_frames_s32(
          filePath, &channels, &sampleRate, &totalPCMFrameCount, NULL);
      if (pSampleData) {
        if (channels == 1) {
          int32_t *stereoBuf =
              (int32_t *)malloc(totalPCMFrameCount * 2 * sizeof(int32_t));
          if (stereoBuf) {
            for (drwav_uint64 i = 0; i < totalPCMFrameCount; i++) {
              stereoBuf[i * 2] = pSampleData[i];
              stereoBuf[i * 2 + 1] = pSampleData[i];
            }
            drwav_free(pSampleData, NULL);
            localPCM = stereoBuf;
            localSamples = (uint32_t)(totalPCMFrameCount * 2);
          } else {
            drwav_free(pSampleData, NULL);
          }
        } else {
          localPCM = pSampleData;
          localSamples = (uint32_t)(totalPCMFrameCount * channels);
        }
        localRate = sampleRate;
      }
    } else {
      int16_t *pSampleData = drwav_open_file_and_read_pcm_frames_s16(
          filePath, &channels, &sampleRate, &totalPCMFrameCount, NULL);
      if (pSampleData) {
        if (channels == 1) {
          int16_t *stereoBuf =
              (int16_t *)malloc(totalPCMFrameCount * 2 * sizeof(int16_t));
          if (stereoBuf) {
            for (drwav_uint64 i = 0; i < totalPCMFrameCount; i++) {
              stereoBuf[i * 2] = pSampleData[i];
              stereoBuf[i * 2 + 1] = pSampleData[i];
            }
            drwav_free(pSampleData, NULL);
            localPCM = stereoBuf;
            localSamples = (uint32_t)(totalPCMFrameCount * 2);
          } else {
            drwav_free(pSampleData, NULL);
          }
        } else {
          localPCM = pSampleData;
          localSamples = (uint32_t)(totalPCMFrameCount * channels);
        }
        localRate = sampleRate;
      }
    }
  } else {
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    int res = mp3dec_load(&mp3d, filePath, &info, NULL, NULL);

    if (res == 0) {
      if (deck->BitDepth == 24) {
        int32_t *buf24 = (int32_t *)malloc(
            info.samples * (info.channels == 1 ? 2 : 1) * sizeof(int32_t));
        if (buf24) {
          if (info.channels == 1) {
            for (size_t i = 0; i < info.samples; i++) {
              int32_t s = (int32_t)info.buffer[i] << 16;
              buf24[i * 2] = s;
              buf24[i * 2 + 1] = s;
            }
          } else {
            for (size_t i = 0; i < info.samples; i++) {
              buf24[i] = (int32_t)info.buffer[i] << 16;
            }
          }
          free(info.buffer);
          localPCM = buf24;
          localSamples = (uint32_t)(info.samples * (info.channels == 1 ? 2 : 1));
        } else {
          free(info.buffer);
        }
      } else {
        if (info.channels == 1) {
          int16_t *stereoBuf =
              (int16_t *)malloc(info.samples * 2 * sizeof(int16_t));
          if (stereoBuf) {
            for (size_t i = 0; i < info.samples; i++) {
              stereoBuf[i * 2] = info.buffer[i];
              stereoBuf[i * 2 + 1] = info.buffer[i];
            }
            free(info.buffer);
            localPCM = stereoBuf;
            localSamples = (uint32_t)(info.samples * 2);
          } else {
            free(info.buffer);
          }
        } else {
          localPCM = info.buffer;
          localSamples = (uint32_t)info.samples;
        }
      }
      localRate = info.hz;
    }
  }

  // Check if a newer load request has arrived during decoding
  if (myID != deck->LastLoadID) {
    if (localPCM) free(localPCM);
    return;
  }

  // Final apply to deck (Safe update)
  void *oldBuf = deck->PCMBuffer;
  deck->PCMBuffer = NULL; 
  std::this_thread::sleep_for(std::chrono::milliseconds(15)); // Wait for active audio block

  deck->PCMBuffer = localPCM;
  deck->TotalSamples = localSamples;
  deck->SampleRate = localRate;

  if (oldBuf) {
    free(oldBuf);
  }

  deck->Position = 0;
  deck->MT_ReadPos = 0;
  deck->IsPlaying = false;
  deck->IsMotorOn = false;
  deck->IsTouching = false;
  deck->VinylModeEnabled = true;
  deck->OutlinedRate = 0;
  deck->JogRate = 0;

  if (deck->SoundTouchHandle) {
    ((SoundTouch *)deck->SoundTouchHandle)->clear();
  }
  strncpy(deck->FilePath, filePath, 511);
  deck->FilePath[511] = '\0';
}

static void ProcessDeckPhysics(DeckAudioState *deck) {
  deck->BaseRate = (float)deck->Pitch / 10000.0f;
  double targetRate = 0.0;
  float accel = 0.08f;

  if (deck->IsTouching) {
    if (deck->VinylModeEnabled) {
      // Vinyl Scratch Mode: Direct 1:1 platter rate lock
      targetRate = deck->JogRate;
      accel = 1.0f;
    } else {
      // CDJ Pitch Bend Mode: Does not halt motor, shifts pitch relative to BaseRate
      targetRate = deck->IsMotorOn ? (deck->BaseRate + deck->JogRate) : deck->JogRate;
      accel = 0.4f;
    }
  } else {
    if (deck->IsMotorOn) {
      targetRate = deck->BaseRate + deck->JogRate;
      accel = deck->VinylStartAccel > 0 ? deck->VinylStartAccel : 0.12f;
    } else {
      targetRate = deck->JogRate;
      accel = (fabs(deck->JogRate) > 0.001) ? 0.2f : 1.0f; // Instant stop when paused and jog stops
    }
  }

  if (deck->ReleaseFXType == 2) { // Backspin Active
    deck->JogRate *= 0.96f;       // smooth exponential decay
    if (fabs(deck->JogRate) < 0.15) {
      deck->ReleaseFXType = 0;
      deck->IsTouching = false;
      deck->JogRate = 0.0;
      deck->OutlinedRate = 0.0;
      deck->IsMotorOn = false;
    }
    targetRate = deck->JogRate;
    accel = 1.0f;
  }

  if (deck->ReleaseFXTimer > 0) {
    deck->ReleaseFXTimer -= (1.0f / 150.0f); // Assuming 150Hz physics update
    if (deck->ReleaseFXTimer <= 0) {
      deck->ReleaseFXType = 0;
    }
  }

  if (deck->OutlinedRate != targetRate) {
    float diff = targetRate - (float)deck->OutlinedRate;
    if (fabs(diff) < accel)
      deck->OutlinedRate = targetRate;
    else
      deck->OutlinedRate += (diff > 0) ? accel : -accel;
  }
  deck->IsPlaying = (fabs(deck->OutlinedRate) > 0.001);
}

static inline float SampleToFloat(void *buffer, int index, int bitDepth) {
  if (bitDepth == 24) {
    return (float)((int32_t *)buffer)[index] / 2147483648.0f;
  } else {
    return (float)((int16_t *)buffer)[index] / 32768.0f;
  }
}

static inline void AudioEngine_GetSampleDirect(void *buffer, int i, int bitDepth,
                                               uint32_t totalSamples,
                                               float *l, float *r) {
  if (i < 0 || i >= (int)(totalSamples / 2)) {
    *l = 0;
    *r = 0;
    return;
  }
  *l = SampleToFloat(buffer, i * 2, bitDepth);
  *r = SampleToFloat(buffer, i * 2 + 1, bitDepth);
}

static inline void AudioEngine_GetSample(void *buffer, double pos, int bitDepth,
                                         uint32_t totalSamples, float *l,
                                         float *r) {
  if (pos < 2.0) {
    *l = 0;
    *r = 0;
    return;
  }
  int i = (int)pos;
  float f = (float)(pos - i);
  if (i >= (int)((totalSamples / 2) - 3)) {
    *l = 0;
    *r = 0;
    return;
  }

  float y0_l = SampleToFloat(buffer, (i - 1) * 2, bitDepth);
  float y1_l = SampleToFloat(buffer, i * 2, bitDepth);
  float y2_l = SampleToFloat(buffer, (i + 1) * 2, bitDepth);
  float y3_l = SampleToFloat(buffer, (i + 2) * 2, bitDepth);

  float y0_r = SampleToFloat(buffer, (i - 1) * 2 + 1, bitDepth);
  float y1_r = SampleToFloat(buffer, i * 2 + 1, bitDepth);
  float y2_r = SampleToFloat(buffer, (i + 1) * 2 + 1, bitDepth);
  float y3_r = SampleToFloat(buffer, (i + 2) * 2 + 1, bitDepth);

  if (f < 0.0001f) {
    *l = y1_l;
    *r = y1_r;
  } else {
    *l = Engine_InterpolateHermite4(f, y0_l, y1_l, y2_l, y3_l);
    *r = Engine_InterpolateHermite4(f, y0_r, y1_r, y2_r, y3_r);
  }
}

static void ProcessDeckAudio(DeckAudioState *deck, float *outMaster,
                             float *outCue, int frames, AudioEngine *engine,
                             int deckIndex, float *outCleanMaster) {
  void *pcm = deck->PCMBuffer;
  bool noiseActive = (deck->ColorFX.activeFX == COLORFX_NOISE &&
                      deck->ColorFX.colorValue != 0.0f);
  if ((!pcm || deck->IsLoading) && !noiseActive)
    return;

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

  bool hasActiveFX =
      (deck->ColorFX.activeFX != COLORFX_NONE &&
       deck->ColorFX.colorValue != 0.0f) ||
      (engine->BeatFX.targetChannel == deckIndex + 1 && engine->BeatFX.isFxOn);

  // Tail detection: sensitivity increased for professional long releases
  bool hasTails = (deck->VuMeterL > 0.000001f || deck->VuMeterR > 0.000001f) ||
                  BeatFXManager_HasTails(&engine->BeatFX, deckIndex);

  if (!deck->IsPlaying && !noiseActive && !hasActiveFX && !hasTails) {
    if (deck->MasterTempoActive && deck->SoundTouchHandle &&
        wasMTActive[deckIndex]) {
      ((SoundTouch *)deck->SoundTouchHandle)->clear();
      wasMTActive[deckIndex] = false;
    }
    return;
  }

  float fs = (float)engine->OutputSampleRate;
  if (deck->LastRate == 0)
    deck->LastRate = deck->OutlinedRate;
  double targetRate = deck->OutlinedRate;

  // EQ Setup (Standardized frequencies)
  static float lastFreqL[MAX_DECKS] = {0};
  static float lastFreqH[MAX_DECKS] = {0};
  static uint32_t lastSR[MAX_DECKS] = {0};

  if (lastFreqL[deckIndex] != 350.0f ||
      lastSR[deckIndex] != engine->OutputSampleRate) {
    EngineLR4_SetLowpass(&deck->EqLowStateL, 350.0f, engine->OutputSampleRate);
    EngineLR4_SetLowpass(&deck->EqLowStateR, 350.0f, engine->OutputSampleRate);
    lastFreqL[deckIndex] = 350.0f;
  }
  if (lastFreqH[deckIndex] != 2500.0f ||
      lastSR[deckIndex] != engine->OutputSampleRate) {
    EngineLR4_SetHighpass(&deck->EqHighStateL, 2500.0f,
                          engine->OutputSampleRate);
    EngineLR4_SetHighpass(&deck->EqHighStateR, 2500.0f,
                          engine->OutputSampleRate);
    lastFreqH[deckIndex] = 2500.0f;
  }
  lastSR[deckIndex] = engine->OutputSampleRate;

  // Dynamic Anti-Clash Sub-Bass Management & Frequency Balancing
  float bassScale = 1.0f;
  if (MAX_DECKS >= 2) {
    DeckAudioState *d0 = &engine->Decks[0];
    DeckAudioState *d1 = &engine->Decks[1];
    
    // Equal-power effective fader and crossfader weights
    float fader0 = d0->Fader * d0->Fader;
    float fader1 = d1->Fader * d1->Fader;
    float xGain0 = Engine_GetCrossfaderGain(engine->Crossfader, 0);
    float xGain1 = Engine_GetCrossfaderGain(engine->Crossfader, 1);
    
    float w0 = fader0 * xGain0 * d0->EqLow * (d0->IsMotorOn ? 1.0f : 0.0f);
    float w1 = fader1 * xGain1 * d1->EqLow * (d1->IsMotorOn ? 1.0f : 0.0f);
    
    // If BOTH decks are active and sending low-frequency energy simultaneously
    if (w0 > 0.05f && w1 > 0.05f) {
      float sumPower = sqrtf(w0 * w0 + w1 * w1);
      if (deckIndex == 0) bassScale = w0 / (sumPower * fmaxf(w0, 0.001f));
      else if (deckIndex == 1) bassScale = w1 / (sumPower * fmaxf(w1, 0.001f));
    }
  }

  float gainL = (deck->EqLow < 0.5f) ? (deck->EqLow * 2.0f)
                                     : (1.0f + (deck->EqLow - 0.5f) * 4.0f);
  gainL *= bassScale; // Protect sub-bass from clashing when mixing 2 decks

  float gainM = (deck->EqMid < 0.5f) ? (deck->EqMid * 2.0f)
                                     : (1.0f + (deck->EqMid - 0.5f) * 4.0f);
  float gainH = (deck->EqHigh < 0.5f) ? (deck->EqHigh * 2.0f)
                                      : (1.0f + (deck->EqHigh - 0.5f) * 4.0f);

  SoundTouch *st = (SoundTouch *)deck->SoundTouchHandle;
  float maxL = 0, maxR = 0;
  float sampleRateRatio =
      (float)deck->SampleRate / (float)engine->OutputSampleRate;

  // Audio Buffering
  float outBuf[4096 * 2]; // Large enough for any process block
  uint32_t received = 0;

  // Bypass MT during motor start/stop or scratching/spinning inertia
  bool motorSteady = (fabs(deck->OutlinedRate - deck->BaseRate) < 0.05f);
  // In Vinyl mode, we want pitch to shift during any scratch/spin (not steady).
  // In CDJ mode, we only bypass during motor start/stop ramps (at least 95% speed).
  bool motorReady = deck->IsMotorOn && (deck->VinylModeEnabled ? motorSteady : (fabs(deck->OutlinedRate) >= fabs(deck->BaseRate) * 0.95f));

  if (deck->MasterTempoActive && !deck->IsTouching && motorReady && st &&
      fabs(targetRate) > 0.01) {
    if (!wasMTActive[deckIndex]) {
      deck->MT_ReadPos = deck->Position;
      st->clear();
      wasMTActive[deckIndex] = true;
    }
    double effectiveTempo = fabs(targetRate) * (double)sampleRateRatio;
    st->setTempo(effectiveTempo);
    st->setPitch((double)sampleRateRatio);

    int maxIterations = 15;
    while (st->numSamples() < (uint32_t)frames && maxIterations-- > 0) {
      float inBuf[512 * 2];
      for (int j = 0; j < 512; j++) {
        AudioEngine_GetSample(pcm, deck->MT_ReadPos, deck->BitDepth,
                              deck->TotalSamples, &inBuf[j * 2],
                              &inBuf[j * 2 + 1]);
        deck->MT_ReadPos += (targetRate > 0) ? 1.0 : -1.0;
        if (deck->IsLooping) {
          double loopLen = deck->LoopEndPos - deck->LoopStartPos;
          if (loopLen > 1.0) {
            int safety = 0;
            while (deck->MT_ReadPos >= deck->LoopEndPos && ++safety < 10)
              deck->MT_ReadPos -= loopLen;
            safety = 0;
            while (deck->MT_ReadPos < deck->LoopStartPos && ++safety < 10)
              deck->MT_ReadPos += loopLen;
          }
        }
      }
      st->putSamples(inBuf, 512);
    }
    received = st->receiveSamples(outBuf, frames);
    deck->Position += (double)received * targetRate * (double)sampleRateRatio;

    if (deck->SlipActive) {
      deck->SlipPosition +=
          (double)received * deck->BaseRate * (double)sampleRateRatio;
    }
  } else if (deck->IsPlaying || noiseActive) {
    if (wasMTActive[deckIndex]) {
      st->clear();
      wasMTActive[deckIndex] = false;
    }
    double currentRate = deck->LastRate;
    double rateDelta = (targetRate - currentRate) / (double)frames;
    for (int i = 0; i < frames; i++) {
      currentRate += rateDelta;
      AudioEngine_GetSample(pcm, deck->Position, deck->BitDepth,
                            deck->TotalSamples, &outBuf[i * 2],
                            &outBuf[i * 2 + 1]);
      deck->Position += currentRate * (double)sampleRateRatio;

      if (deck->SlipActive) {
        deck->SlipPosition += deck->BaseRate * (double)sampleRateRatio;
      }

      // Enforce -4 bar limit (16 beats)
      // Enforce -4 bar limit (16 beats), or fallback to -2s if BPM unknown
      double limitMs =
          (deck->BPM > 10.0) ? (16.0 * (60.0 / deck->BPM) * 1000.0) : 2000.0;
      double limitSamples = -(limitMs * (double)deck->SampleRate / 1000.0);
      if (deck->Position < limitSamples)
        deck->Position = limitSamples;

      if (deck->IsLooping) {
        double loopLen = deck->LoopEndPos - deck->LoopStartPos;
        if (loopLen > 1.0) {
          int safety = 0;
          while (deck->Position >= deck->LoopEndPos && ++safety < 10)
            deck->Position -= loopLen;
          safety = 0;
          while (deck->Position < deck->LoopStartPos && ++safety < 10)
            deck->Position += loopLen;
        }
      }
    }
    received = frames;
  } else {
    // Deck is paused and no noise: output silence so VU meter can decay
    if (wasMTActive[deckIndex]) {
      st->clear();
      wasMTActive[deckIndex] = false;
    }
    memset(outBuf, 0, frames * 2 * sizeof(float));
    received = frames;
  }

  // Pre-calculate Ramping Gain (Equal-Power Audio Taper Fader Curve)
  float effectiveFader = deck->Fader * deck->Fader;
  float effectiveLastFader = deck->LastFader * deck->LastFader;

  float startCrossGain =
      Engine_GetCrossfaderGain(engine->LastCrossfader, deckIndex);
  float endCrossGain = Engine_GetCrossfaderGain(engine->Crossfader, deckIndex);
  float startTotalGain = effectiveLastFader * startCrossGain;
  float endTotalGain = effectiveFader * endCrossGain;

  // Common Post-Processing Loop
  for (int i = 0; i < (int)received; i++) {
    float l = outBuf[i * 2], r = outBuf[i * 2 + 1];

    // EQ
    float lowL = EngineLR4_Process(&deck->EqLowStateL, l);
    float highL = EngineLR4_Process(&deck->EqHighStateL, l);
    l = (lowL * gainL) + (l - lowL - highL) * gainM + (highL * gainH);
    float lowR = EngineLR4_Process(&deck->EqLowStateR, r);
    float highR = EngineLR4_Process(&deck->EqHighStateR, r);
    r = (lowR * gainL) + (r - lowR - highR) * gainM + (highR * gainH);

    // Trim & Color FX
    l *= deck->Trim;
    r *= deck->Trim;
    ColorFXManager_Process(&deck->ColorFX, &l, &r, l, r, fs);

    // Cue (Pre-Fader)
    if (deck->IsCueActive) {
      outCue[i * 2] += l;
      outCue[i * 2 + 1] += r;
    }
    maxL = fmaxf(maxL, fabsf(l));
    maxR = fmaxf(maxR, fabsf(r));

    // Smooth DryGain ramping for Release FX Echo
    float targetDry = deck->ReleaseFXEchoActive ? 0.0f : 1.0f;
    if (deck->DryGain < targetDry) {
      deck->DryGain += 0.0008f;
      if (deck->DryGain > targetDry) deck->DryGain = targetDry;
    } else if (deck->DryGain > targetDry) {
      deck->DryGain -= 0.0008f;
      if (deck->DryGain < targetDry) deck->DryGain = targetDry;
    }

    // Channel Fader (Post-Cue) with Ramping
    float t = (float)i / (float)received;
    float currentTotalGain =
        startTotalGain + (endTotalGain - startTotalGain) * t;
    float sendL = l * currentTotalGain;
    float sendR = r * currentTotalGain;

    float dryL = sendL * deck->DryGain;
    float dryR = sendR * deck->DryGain;

    // Professional Routing Logic
    if (engine->RoutingMode == FX_ROUTING_POST_FADER) {
      // MASTER gets Dry signal (Post-Fader) attenuated by DryGain
      outMaster[i * 2] += dryL;
      outMaster[i * 2 + 1] += dryR;
      outCleanMaster[i * 2] += dryL;
      outCleanMaster[i * 2 + 1] += dryR;

      // BEAT FX gets Send signal (un-attenuated sendL/sendR while Echo active so tail captures full audio)
      float wetL = 0, wetR = 0;
      if (engine->BeatFX.targetChannel == deckIndex + 1) {
        BeatFXManager_ProcessWetOnly(&engine->BeatFX, &wetL, &wetR, sendL,
                                     sendR, fs);
      }
      outMaster[i * 2] += wetL;
      outMaster[i * 2 + 1] += wetR;
    } else {
      // INSERT MODE: Beat FX sits between Fader and Master
      float fxOutL = sendL, fxOutR = sendR;
      if (engine->BeatFX.targetChannel == deckIndex + 1) {
        BeatFXManager_Process(&engine->BeatFX, &fxOutL, &fxOutR, sendL, sendR,
                              fs);
        outMaster[i * 2] += fxOutL * deck->DryGain;
        outMaster[i * 2 + 1] += fxOutR * deck->DryGain;
        outCleanMaster[i * 2] += fxOutL * deck->DryGain;
        outCleanMaster[i * 2 + 1] += fxOutR * deck->DryGain;
      } else {
        outMaster[i * 2] += dryL;
        outMaster[i * 2 + 1] += dryR;
        outCleanMaster[i * 2] += dryL;
        outCleanMaster[i * 2 + 1] += dryR;
      }
    }
  }

  deck->LastFader = deck->Fader; // Update for next block
  deck->LastRate = targetRate;
  // VU Meter Ballistics
  float peakL = maxL * 1.6f;
  float peakR = maxR * 1.6f;
  if (peakL > 1.0f)
    peakL = 1.0f;
  if (peakR > 1.0f)
    peakR = 1.0f;
  if (peakL > deck->VuMeterL)
    deck->VuMeterL = peakL;
  else {
    deck->VuMeterL = deck->VuMeterL * 0.88f + peakL * 0.12f;
    if (deck->VuMeterL < 0.005f) deck->VuMeterL = 0.0f;
  }
  if (peakR > deck->VuMeterR)
    deck->VuMeterR = peakR;
  else {
    deck->VuMeterR = deck->VuMeterR * 0.88f + peakR * 0.12f;
    if (deck->VuMeterR < 0.005f) deck->VuMeterR = 0.0f;
  }
}

void AudioEngine_SetFXRouting(AudioEngine *engine, FXRoutingMode mode) {
  engine->RoutingMode = mode;
}

void AudioEngine_Process(AudioEngine *engine, float *outBuffer, int frames) {
  static std::thread::id g_audioThreadId;
  static bool g_audioThreadIdSet = false;
  auto startTime = std::chrono::high_resolution_clock::now();

  if (!g_audioThreadIdSet) {
    g_audioThreadId = std::this_thread::get_id();
    g_audioThreadIdSet = true;
  } else if (g_audioThreadId != std::this_thread::get_id()) {
    UNX_LOG_ERR("[AUDIO] Race Condition! AudioEngine_Process called from multiple threads!");
  }

  static float masterMix[4096 * 2];
  static float cleanMasterMix[4096 * 2];
  static float cueMix[4096 * 2];
  memset(masterMix, 0, frames * 2 * sizeof(float));
  memset(cleanMasterMix, 0, frames * 2 * sizeof(float));
  memset(cueMix, 0, frames * 2 * sizeof(float));

  for (int i = 0; i < MAX_DECKS; i++) {
    ProcessDeckAudio(&engine->Decks[i], masterMix, cueMix, frames, engine, i,
                     cleanMasterMix);
  }

  float mPeakL = 0, mPeakR = 0;
  for (int s = 0; s < frames; s++) {
    // Professional Master FX Send/Return (If assigned to Master)
    if (engine->BeatFX.targetChannel == 0) {
      float wetL = 0, wetR = 0;
      // Take input from cleanMasterMix to avoid feedback loops with deck tails
      BeatFXManager_ProcessWetOnly(
          &engine->BeatFX, &wetL, &wetR, cleanMasterMix[s * 2],
          cleanMasterMix[s * 2 + 1], engine->OutputSampleRate);
      masterMix[s * 2] += wetL;
      masterMix[s * 2 + 1] += wetR;
    }

    // --- Master Gain & Soft Limiting ---
    float l = masterMix[s * 2] * engine->MasterVolume;
    float r = masterMix[s * 2 + 1] * engine->MasterVolume;

    mPeakL = fmaxf(mPeakL, fabsf(l));
    mPeakR = fmaxf(mPeakR, fabsf(r));

    // Safety Limiter (Professional Stage)
    if (l > 1.0f)
      l = 1.0f;
    else if (l < -1.0f)
      l = -1.0f;
    if (r > 1.0f)
      r = 1.0f;
    else if (r < -1.0f)
      r = -1.0f;

    outBuffer[s * 4] = l;
    outBuffer[s * 4 + 1] = r;
    outBuffer[s * 4 + 2] = fmaxf(-1.0f, fminf(1.0f, cueMix[s * 2]));
    outBuffer[s * 4 + 3] = fmaxf(-1.0f, fminf(1.0f, cueMix[s * 2 + 1]));
  }

  // Master VU Ballistics
  float pML = mPeakL * 1.4f;
  float pMR = mPeakR * 1.4f;
  if (pML > 1.0f)
    pML = 1.0f;
  if (pMR > 1.0f)
    pMR = 1.0f;
  if (pML > engine->MasterVuL)
    engine->MasterVuL = pML;
  else
    engine->MasterVuL = engine->MasterVuL * 0.88f + pML * 0.12f;
  if (pMR > engine->MasterVuR)
    engine->MasterVuR = pMR;
  else
    engine->MasterVuR = engine->MasterVuR * 0.88f + pMR * 0.12f;
  engine->LastCrossfader = engine->Crossfader;

  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = endTime - startTime;
  double budget = (double)frames / (double)engine->OutputSampleRate;
  
  // Periodic Performance Logging (every 5 seconds)
  static double accumTime = 0;
  static int accumCount = 0;
  static auto lastLogTime = std::chrono::steady_clock::now();
  accumTime += elapsed.count();
  accumCount++;
  
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime).count() >= 5) {
      double avgMs = (accumTime / accumCount) * 1000.0;
      double budgetMs = budget * 1000.0;
      UNX_LOG_INFO("[PERF] [AUDIO] Latency: %.2f ms (Budget: %.2f ms)", avgMs, budgetMs);
      accumTime = 0;
      accumCount = 0;
      lastLogTime = now;
  }

  if (elapsed.count() > budget) {
    UNX_LOG_WARN("[AUDIO] Buffer Underrun Detected! Processed %d frames in %.2f ms (Budget: %.2f ms)",
                 frames, elapsed.count() * 1000.0, budget * 1000.0);
  }
}

void DeckAudio_Play(DeckAudioState *deck) { deck->IsMotorOn = true; }
void DeckAudio_Stop(DeckAudioState *deck) { deck->IsMotorOn = false; }

void DeckAudio_Unload(DeckAudioState *deck) {
  deck->IsLoading = true;
  // Short sleep to ensure audio thread finishes any active sample processing
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  deck->IsPlaying = false;
  deck->IsMotorOn = false;
  deck->IsTouching = false;
  deck->JogRate = 0.0;
  deck->OutlinedRate = 0.0;
  deck->LastRate = 0.0;
  deck->IsLooping = false;
  deck->SlipActive = false;
  deck->ReleaseFXType = 0;
  deck->ReleaseFXTimer = 0.0f;
  deck->DryGain = 1.0f;
  deck->ReleaseFXEchoActive = false;
  if (deck->PCMBuffer) {
    void *ptr = deck->PCMBuffer;
    deck->PCMBuffer = NULL;
    free(ptr);
  }
  deck->TotalSamples = 0;
  deck->Position = 0;
  deck->FilePath[0] = '\0';
  deck->IsLoading = false;
  deck->VuMeterL = 0.0f;
  deck->VuMeterR = 0.0f;
}

void DeckAudio_TriggerReleaseFX(DeckAudioState *deck, int type) {
  float bpm = (deck->BPM > 10.0) ? (float)deck->BPM : 120.0f;
  float beatSec = 60.0f / bpm; // 1 Beat duration in seconds

  deck->ReleaseFXType = type;

  switch (type) {
  case 1: { // Vinyl Brake Short (1 Beat)
    float durationSec = beatSec * 1.0f;
    if (durationSec < 0.1f) durationSec = 0.1f;
    deck->VinylStopAccel = 1.0f / (durationSec * 60.0f);
    deck->ReleaseFXTimer = durationSec;
    deck->IsMotorOn = false;
    break;
  }
  case 2: { // Vinyl Brake Long (4 Beats / 1 Bar)
    float durationSec = beatSec * 4.0f;
    if (durationSec < 0.2f) durationSec = 0.2f;
    deck->VinylStopAccel = 1.0f / (durationSec * 60.0f);
    deck->ReleaseFXTimer = durationSec;
    deck->IsMotorOn = false;
    break;
  }
  case 3: { // Backspin Short (2 Beats)
    float durationSec = beatSec * 2.0f;
    if (durationSec < 0.2f) durationSec = 0.2f;
    deck->VinylModeEnabled = true;
    deck->IsTouching = true;
    deck->JogRate = -7.0f;
    deck->OutlinedRate = deck->JogRate;
    deck->ReleaseFXType = 2; // Backspin Active mode
    deck->ReleaseFXTimer = durationSec;
    deck->IsMotorOn = false;
    break;
  }
  case 4: { // Backspin Long (4 Beats / 1 Bar)
    float durationSec = beatSec * 4.0f;
    if (durationSec < 0.4f) durationSec = 0.4f;
    deck->VinylModeEnabled = true;
    deck->IsTouching = true;
    deck->JogRate = -15.0f;
    deck->OutlinedRate = deck->JogRate;
    deck->ReleaseFXType = 2; // Backspin Active mode
    deck->ReleaseFXTimer = durationSec;
    deck->IsMotorOn = false;
    break;
  }
  case 5: { // Echo Out (4 Beats / 1 Bar)
    float durationSec = beatSec * 4.0f;
    deck->ReleaseFXTimer = durationSec;
    break;
  }
  }
}
void DeckAudio_SetPitch(DeckAudioState *deck, uint16_t pitch) {
  deck->Pitch = pitch;
}
void DeckAudio_QueueJumpMs(DeckAudioState *deck, uint32_t targetMs,
                           uint32_t waitMs) {
  deck->QueuedJumpMs = targetMs;
  uint32_t sr = (deck->OutputSampleRate > 0) ? deck->OutputSampleRate : 48000;
  deck->QueuedWaitSamples = (uint32_t)((float)waitMs * ((float)sr / 1000.0f));
  deck->HasQueuedJump = true;
}
void DeckAudio_SetPlaying(DeckAudioState *deck, bool playing) {
  deck->IsMotorOn = playing;
}
void DeckAudio_InstantPlay(DeckAudioState *deck) {
  deck->IsMotorOn = true;
  deck->BaseRate = (float)deck->Pitch / 10000.0f;
  deck->OutlinedRate = deck->BaseRate;
  deck->LastRate = deck->BaseRate;
}
void DeckAudio_InstantStop(DeckAudioState *deck) {
  deck->IsMotorOn = false;
  deck->OutlinedRate = 0;
  deck->LastRate = 0;
}
void DeckAudio_SetJogRate(DeckAudioState *deck, double rate) {
  if (deck->IsTouching)
    deck->JogRate = rate;
}
void DeckAudio_SetJogTouch(DeckAudioState *deck, bool touching) {
  if (deck->ReleaseFXType == 2)
    return; // Protect active backspin
  
  bool wasTouching = deck->IsTouching;
  deck->IsTouching = touching;
  
  if (!touching) {
    deck->JogRate = deck->OutlinedRate;
    
    // Slip Mode Catch-up on touch release!
    if (wasTouching && deck->SlipActive) {
      deck->Position = deck->SlipPosition;
      deck->MT_ReadPos = deck->SlipPosition;
      deck->SlipActive = false;
    }
    
    // Instant zero lock if deck is paused
    if (!deck->IsMotorOn && fabs(deck->JogRate) < 0.05) {
      deck->JogRate = 0.0;
      deck->OutlinedRate = 0.0;
    }
  }
}
void DeckAudio_JumpToMs(DeckAudioState *deck, int64_t ms) {

  // Enforce -4 bar limit
  int64_t limitMs = 0;
  if (deck->BPM > 10.0) {
    limitMs = (int64_t)-(16.0 * (60000.0 / deck->BPM));
  }
  if (ms < limitMs)
    ms = limitMs;

  deck->Position = (double)ms * ((double)deck->SampleRate / 1000.0);
  deck->MT_ReadPos = deck->Position;
  if (deck->Position * 2 >= (double)deck->TotalSamples) {
    deck->Position = (double)(deck->TotalSamples / 2) - 1.0;
    deck->MT_ReadPos = deck->Position;
  }
  if (deck->MasterTempoActive && deck->SoundTouchHandle)
    ((SoundTouch *)deck->SoundTouchHandle)->clear();
}

void DeckAudio_SetSlip(DeckAudioState *deck, bool active) {
  if (active && !deck->SlipActive) {
    deck->SlipPosition = deck->Position;
  } else if (!active && deck->SlipActive) {
    deck->Position = deck->SlipPosition;
    deck->MT_ReadPos = deck->Position;
    if (deck->SoundTouchHandle) {
      ((soundtouch::SoundTouch *)deck->SoundTouchHandle)->clear();
    }
  }
  deck->SlipActive = active;
}

void DeckAudio_SetLoop(DeckAudioState *deck, bool active, double startPos,
                       double endPos) {
  deck->LoopStartPos = startPos;
  deck->LoopEndPos = endPos;
  deck->IsLooping = active;

  double loopLen = endPos - startPos;
  if (active && loopLen > 1.0) {
    if (deck->Position >= endPos || deck->Position < startPos) {
      double offset = deck->Position - startPos;
      deck->Position = startPos + fmod(offset, loopLen);
      if (deck->Position < startPos)
        deck->Position += loopLen; // handle negative offset if any
      deck->MT_ReadPos = deck->Position;
      if (deck->SoundTouchHandle) {
        ((soundtouch::SoundTouch *)deck->SoundTouchHandle)->clear();
      }
    }
  }
}

void DeckAudio_ExitLoop(DeckAudioState *deck) { deck->IsLooping = false; }

void DeckAudio_ClearMT(DeckAudioState *deck) {
  if (deck->SoundTouchHandle) {
    ((soundtouch::SoundTouch *)deck->SoundTouchHandle)->clear();
  }
}
