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

#define DRWAV_API static
#define DRWAV_PRIVATE static
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

using namespace soundtouch;

void DeckAudio_LoadTrackAsync(DeckAudioState *deck, const char *filePath) {
  if (deck->IsLoading)
    return;

  deck->IsLoading = true;
  deck->LoadingProgress = 0.0f;

  std::string path = filePath;
  std::thread([deck, path]() {
    DeckAudio_LoadTrack(deck, path.c_str());
    deck->IsLoading = false;
    deck->LoadingProgress = 1.0f;
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
  }

  BeatFXManager_Init(&engine->BeatFX);
  engine->Crossfader = 0.0f;
  engine->LastCrossfader = 0.0f;
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
    ((SoundTouch *)deck->SoundTouchHandle)->clear();
  }
  strncpy(deck->FilePath, filePath, 511);
  deck->FilePath[511] = '\0';

  if (!filePath || strlen(filePath) == 0)
    return;

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
          deck->PCMBuffer = buf24;
          deck->TotalSamples = info.samples * (info.channels == 1 ? 2 : 1);
        } else {
          free(info.buffer);
          deck->PCMBuffer = NULL;
          deck->TotalSamples = 0;
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
    targetRate = (deck->VinylModeEnabled) ? deck->JogRate
                                          : (deck->BaseRate + deck->JogRate);
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

static inline void AudioEngine_GetSampleDirect(DeckAudioState *deck, int i,
                                               float *l, float *r) {
  if (i < 0 || i >= (int)(deck->TotalSamples / 2)) {
    *l = 0;
    *r = 0;
    return;
  }
  *l = SampleToFloat(deck->PCMBuffer, i * 2, deck->BitDepth);
  *r = SampleToFloat(deck->PCMBuffer, i * 2 + 1, deck->BitDepth);
}

static inline void AudioEngine_GetSample(DeckAudioState *deck, double pos,
                                         float *l, float *r) {
  if (pos < 2.0) {
    *l = 0;
    *r = 0;
    return;
  }
  int i = (int)pos;
  float f = (float)(pos - i);
  if (i >= (int)((deck->TotalSamples / 2) - 3)) {
    *l = 0;
    *r = 0;
    return;
  }

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

static void ProcessDeckAudio(DeckAudioState *deck, float *outMaster,
                             float *outCue, int frames, AudioEngine *engine,
                             int deckIndex, float *outCleanMaster) {
  bool noiseActive = (deck->ColorFX.activeFX == COLORFX_NOISE &&
                      deck->ColorFX.colorValue != 0.0f);
  if ((!deck->PCMBuffer || deck->IsLoading) && !noiseActive)
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
    if (deck->MasterTempoActive && deck->SoundTouchHandle && wasMTActive[deckIndex]) {
      ((SoundTouch *)deck->SoundTouchHandle)->clear();
      wasMTActive[deckIndex] = false;
    }
    return;
  }

  float fs = (float)engine->OutputSampleRate;
  if (deck->LastRate == 0) deck->LastRate = deck->OutlinedRate;
  double targetRate = deck->OutlinedRate;

  // EQ Setup (Standardized frequencies)
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

  SoundTouch *st = (SoundTouch *)deck->SoundTouchHandle;
  float maxL = 0, maxR = 0;
  float sampleRateRatio = (float)deck->SampleRate / (float)engine->OutputSampleRate;

  // Audio Buffering
  float outBuf[4096 * 2]; // Large enough for any process block
  uint32_t received = 0;

  if (deck->MasterTempoActive && !deck->IsTouching && st && fabs(targetRate) > 0.01) {
    if (!wasMTActive[deckIndex]) {
      deck->MT_ReadPos = deck->Position;
      st->clear();
      wasMTActive[deckIndex] = true;
    }
    double effectiveTempo = fabs(targetRate) * (double)sampleRateRatio;
    st->setTempo(effectiveTempo);
    st->setPitch(1.0);

    int maxIterations = 15;
    while (st->numSamples() < (uint32_t)frames && maxIterations-- > 0) {
      float inBuf[512 * 2];
      for (int j = 0; j < 512; j++) {
        AudioEngine_GetSample(deck, deck->MT_ReadPos, &inBuf[j * 2], &inBuf[j * 2 + 1]);
        deck->MT_ReadPos += (targetRate > 0) ? 1.0 : -1.0;
        if (deck->IsLooping) {
          double loopLen = deck->LoopEndPos - deck->LoopStartPos;
          if (loopLen > 1.0) {
            while (deck->MT_ReadPos >= deck->LoopEndPos) deck->MT_ReadPos -= loopLen;
            while (deck->MT_ReadPos < deck->LoopStartPos) deck->MT_ReadPos += loopLen;
          }
        }
      }
      st->putSamples(inBuf, 512);
    }
    received = st->receiveSamples(outBuf, frames);
    deck->Position += (double)received * targetRate;
  } else if (deck->IsPlaying || noiseActive) {
    if (wasMTActive[deckIndex]) { st->clear(); wasMTActive[deckIndex] = false; }
    double currentRate = deck->LastRate;
    double rateDelta = (targetRate - currentRate) / (double)frames;
    for (int i = 0; i < frames; i++) {
      currentRate += rateDelta;
      AudioEngine_GetSample(deck, deck->Position, &outBuf[i * 2], &outBuf[i * 2 + 1]);
      deck->Position += currentRate * (double)sampleRateRatio;
      if (deck->IsLooping) {
        double loopLen = deck->LoopEndPos - deck->LoopStartPos;
        if (loopLen > 1.0) {
          while (deck->Position >= deck->LoopEndPos) deck->Position -= loopLen;
          while (deck->Position < deck->LoopStartPos) deck->Position += loopLen;
        }
      }
    }
    received = frames;
  } else {
    // Deck is paused and no noise: output silence so VU meter can decay
    if (wasMTActive[deckIndex]) { st->clear(); wasMTActive[deckIndex] = false; }
    memset(outBuf, 0, frames * 2 * sizeof(float));
    received = frames;
  }

  // Pre-calculate Ramping Gain (Interpolation) to prevent clicks
  float startCrossGain = Engine_GetCrossfaderGain(engine->LastCrossfader, deckIndex);
  float endCrossGain = Engine_GetCrossfaderGain(engine->Crossfader, deckIndex);
  float startTotalGain = deck->LastFader * startCrossGain;
  float endTotalGain = deck->Fader * endCrossGain;

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
    l *= deck->Trim; r *= deck->Trim;
    ColorFXManager_Process(&deck->ColorFX, &l, &r, l, r, fs);

    // Cue (Pre-Fader)
    if (deck->IsCueActive) { outCue[i * 2] += l; outCue[i * 2 + 1] += r; }
    maxL = fmaxf(maxL, fabsf(l)); maxR = fmaxf(maxR, fabsf(r));

    // Channel Fader (Post-Cue) with Ramping
    float t = (float)i / (float)received;
    float currentTotalGain = startTotalGain + (endTotalGain - startTotalGain) * t;
    float sendL = l * currentTotalGain; 
    float sendR = r * currentTotalGain;

    // Professional Routing Logic
    if (engine->RoutingMode == FX_ROUTING_POST_FADER) {
      // MASTER gets Dry signal (Post-Fader)
      outMaster[i * 2] += sendL; outMaster[i * 2 + 1] += sendR;
      // Also add to clean sum for Master FX
      outCleanMaster[i * 2] += sendL; outCleanMaster[i * 2 + 1] += sendR;

      // BEAT FX gets Send signal (Post-Fader), MASTER gets Wet signal (Return)
      float wetL = 0, wetR = 0;
      if (engine->BeatFX.targetChannel == deckIndex + 1) {
        BeatFXManager_ProcessWetOnly(&engine->BeatFX, &wetL, &wetR, sendL, sendR, fs);
      }
      outMaster[i * 2] += wetL; outMaster[i * 2 + 1] += wetR;
      } else {
      // INSERT MODE: Beat FX sits between Fader and Master
      float fxOutL = sendL, fxOutR = sendR;
      if (engine->BeatFX.targetChannel == deckIndex + 1) {
        BeatFXManager_Process(&engine->BeatFX, &fxOutL, &fxOutR, sendL, sendR, fs);
      }
      outMaster[i * 2] += fxOutL; outMaster[i * 2 + 1] += fxOutR;
      outCleanMaster[i * 2] += fxOutL; outCleanMaster[i * 2 + 1] += fxOutR;
    }
  }

  deck->LastFader = deck->Fader; // Update for next block
  deck->LastRate = targetRate;
  // VU Meter Ballistics
  float peakL = maxL * 1.6f; float peakR = maxR * 1.6f;
  if (peakL > 1.0f) peakL = 1.0f; if (peakR > 1.0f) peakR = 1.0f;
  if (peakL > deck->VuMeterL) deck->VuMeterL = peakL;
  else deck->VuMeterL = deck->VuMeterL * 0.88f + peakL * 0.12f;
  if (peakR > deck->VuMeterR) deck->VuMeterR = peakR;
  else deck->VuMeterR = deck->VuMeterR * 0.88f + peakR * 0.12f;
}

void AudioEngine_SetFXRouting(AudioEngine *engine, FXRoutingMode mode) {
  engine->RoutingMode = mode;
}

void AudioEngine_Process(AudioEngine *engine, float *outBuffer, int frames) {
  static float masterMix[4096 * 2];
  static float cleanMasterMix[4096 * 2];
  static float cueMix[4096 * 2];
  memset(masterMix, 0, frames * 2 * sizeof(float));
  memset(cleanMasterMix, 0, frames * 2 * sizeof(float));
  memset(cueMix, 0, frames * 2 * sizeof(float));

  for (int i = 0; i < MAX_DECKS; i++) {
    ProcessDeckAudio(&engine->Decks[i], masterMix, cueMix, frames, engine, i, cleanMasterMix);
  }

  float mPeakL = 0, mPeakR = 0;
  for (int s = 0; s < frames; s++) {
    // Professional Master FX Send/Return (If assigned to Master)
    if (engine->BeatFX.targetChannel == 0) {
       float wetL = 0, wetR = 0;
       // Take input from cleanMasterMix to avoid feedback loops with deck tails
       BeatFXManager_ProcessWetOnly(&engine->BeatFX, &wetL, &wetR, cleanMasterMix[s*2], cleanMasterMix[s*2+1], engine->OutputSampleRate);
       masterMix[s*2] += wetL; masterMix[s*2+1] += wetR;
    }

    // --- Master Gain & Soft Limiting ---
    float l = masterMix[s * 2] * engine->MasterVolume;
    float r = masterMix[s * 2 + 1] * engine->MasterVolume;

    mPeakL = fmaxf(mPeakL, fabsf(l));
    mPeakR = fmaxf(mPeakR, fabsf(r));

    // Safety Limiter (Professional Stage)
    if (l > 1.0f) l = 1.0f; else if (l < -1.0f) l = -1.0f;
    if (r > 1.0f) r = 1.0f; else if (r < -1.0f) r = -1.0f;

    outBuffer[s * 4] = l;
    outBuffer[s * 4 + 1] = r;
    outBuffer[s * 4 + 2] = fmaxf(-1.0f, fminf(1.0f, cueMix[s * 2]));
    outBuffer[s * 4 + 3] = fmaxf(-1.0f, fminf(1.0f, cueMix[s * 2 + 1]));
  }

  // Master VU Ballistics
  float pML = mPeakL * 1.4f; float pMR = mPeakR * 1.4f;
  if (pML > 1.0f) pML = 1.0f; if (pMR > 1.0f) pMR = 1.0f;
  if (pML > engine->MasterVuL) engine->MasterVuL = pML;
  else engine->MasterVuL = engine->MasterVuL * 0.88f + pML * 0.12f;
  if (pMR > engine->MasterVuR) engine->MasterVuR = pMR;
  else engine->MasterVuR = engine->MasterVuR * 0.88f + pMR * 0.12f;
  engine->LastCrossfader = engine->Crossfader;
}

void DeckAudio_Play(DeckAudioState *deck) { deck->IsMotorOn = true; }
void DeckAudio_Stop(DeckAudioState *deck) { deck->IsMotorOn = false; }

void DeckAudio_Unload(DeckAudioState *deck) {
  deck->IsLoading = true;
  // Short sleep to ensure audio thread finishes any active sample processing
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  deck->IsPlaying = false;
  deck->IsMotorOn = false;
  deck->OutlinedRate = 0;
  if (deck->PCMBuffer) {
    void *ptr = deck->PCMBuffer;
    deck->PCMBuffer = NULL;
    free(ptr);
  }
  deck->TotalSamples = 0;
  deck->Position = 0;
  deck->FilePath[0] = '\0';
  deck->IsLoading = false;
}
void DeckAudio_SetPitch(DeckAudioState *deck, uint16_t pitch) {
  deck->Pitch = pitch;
}
void DeckAudio_QueueJumpMs(DeckAudioState *deck, uint32_t targetMs,
                           uint32_t waitMs) {
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
void DeckAudio_SetJogRate(DeckAudioState *deck, double rate) {
  if (deck->IsTouching)
    deck->JogRate = rate;
}
void DeckAudio_SetJogTouch(DeckAudioState *deck, bool touching) {
  deck->IsTouching = touching;
  if (!touching)
    deck->JogRate = deck->OutlinedRate;
}
void DeckAudio_JumpToMs(DeckAudioState *deck, int64_t ms) {
  deck->IsLooping = false;
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

  if (active && deck->Position >= endPos) {
    deck->Position = startPos;
    deck->MT_ReadPos = startPos;
    if (deck->SoundTouchHandle) {
      ((soundtouch::SoundTouch *)deck->SoundTouchHandle)->clear();
    }
  }
}

void DeckAudio_ExitLoop(DeckAudioState *deck) { deck->IsLooping = false; }

void DeckAudio_ClearMT(DeckAudioState *deck) {
  if (deck->SoundTouchHandle) {
    ((soundtouch::SoundTouch *)deck->SoundTouchHandle)->clear();
  }
}
