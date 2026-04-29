#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "engine/fx/beatfx/beatfx_manager.h"
#include "engine/fx/colorfx/colorfx_manager.h"

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define MAX_DECKS 2

// 1 frame = 1/105 s at 44.1kHz = 420 samples (Teensy Matching Rate)
#define SAMPLES_PER_FRAME 420

typedef enum {
  CHANGE_SPEED_NO_CHANGE = 0,
  CHANGE_SPEED_NEED_UP = 1,
  CHANGE_SPEED_NEED_DOWN = 2
} SpeedChangeState;

typedef enum {
  FX_ROUTING_INSERT = 0,    // Pre-Fader (Internal)
  FX_ROUTING_POST_FADER = 1 // DJM-Style Parallel Send/Return
} FXRoutingMode;

typedef struct BiquadState {
  float x1, x2;
  float y1, y2;
} BiquadState;

typedef struct DeckAudioState {
  void *PCMBuffer;       // Full track audio decoded (interleaved L/R)
  uint32_t TotalSamples; // Total samples in buffer
  uint32_t SampleRate;   // Original sample rate (e.g. 44100, 48000)
  uint32_t OutputSampleRate; // Engine output sample rate
  int BitDepth;          // 16 or 24
  char FilePath[512];    // Path to the loaded track for auto-reinit

  // Core playback state (from XDJ-X firmware concept)
  double Position;         // Read head position (exact fractional sample index)
  double MT_ReadPos;       // SoundTouch read position
  uint32_t PlayAddressCUE; // Integer frame base for CUE (usually 294 * CUE_ADR)

  // Physics and Scratch
  uint16_t Pitch;        // 10000 = 100% pitch
  uint16_t TargetPitch;  // Where the fader is currently set
  double JogRate;        // Inertia value for vinyl mode
  bool IsTouching;       // User is actively touching jog (renamed)
  bool VinylModeEnabled; // Vinyl vs CDJ mode
  bool IsPlaying;        // Deck is in play mode (logical)
  bool IsMotorOn;        // Platter motor state (physical)
  bool IsReverse;        // Reverse playback active

  // Exact beat quantize buffering
  uint32_t QueuedJumpMs;
  uint32_t QueuedWaitSamples;
  bool HasQueuedJump;

  SpeedChangeState SpeedState; // Need ramp up/down

  float VinylStartAccel;
  float VinylStopAccel;

  // Engine EngineBufferScaleLinear state
  float BaseRate;         // Determined by pitch slider
  float OutlinedRate;     // Final calculated rate including scratch offsets
  bool MasterTempoActive; // Key lock

  // SoundTouch (Master Tempo / Pitch) State
  void *SoundTouchHandle; // Opaque pointer to soundtouch::SoundTouch instance

  float Trim;
  float Fader;     // Channel Fader (0.0 to 1.0)
  float LastFader; // ramping gain

  // EQ State (3-Band)
  float EqLow;  // 0.0 to 1.0 (default 0.5)
  float EqMid;  // 0.0 to 1.0 (default 0.5)
  float EqHigh; // 0.0 to 1.0 (default 0.5)

  EngineLR4 EqLowStateL, EqLowStateR;
  EngineLR4 EqHighStateL, EqHighStateR;

  float LastRate;

  // Sound Color FX
  ColorFXManager ColorFX;

  bool IsCueActive; // Channel monitoring for headphones (Ch 3-4)

  // Slip Mode & Looping
  bool SlipActive;
  double SlipPosition; // "Ghost" position for slip mode
  bool IsLooping;
  double LoopStartPos;
  double LoopEndPos;

  bool IsLoading;        // Track is currently being decoded in background
  float LoadingProgress; // 0.0 to 1.0 (if supported by decoder)

  // VU Meter (Real-time tracking of DSP output peak for UI)
  float VuMeterL;
  float VuMeterR;

  double BPM; // Current BPM for beat-based limits
  
  // Release FX State
  int ReleaseFXType;   // 0=None, 1=Brake, 2=Spin, 3=Echo
  float ReleaseFXTimer; 
} DeckAudioState;

typedef struct AudioEngine {
  DeckAudioState Decks[MAX_DECKS];
  BeatFXManager BeatFX;
  float Crossfader;          // -1.0 (Deck A) to 1.0 (Deck B)
  float LastCrossfader;      // ramping gain
  float MasterVolume;        // 0.0 to 1.0
  uint32_t OutputSampleRate; // Actual hardware sample rate
  FXRoutingMode RoutingMode;
  float MasterVuL;
  float MasterVuR;
} AudioEngine;

void AudioEngine_Init(AudioEngine *engine, uint32_t outputSampleRate);
void AudioEngine_SetOutputSampleRate(AudioEngine *engine, uint32_t sampleRate);
void AudioEngine_SetPCMBitDepth(AudioEngine *engine, int bitDepth);
void AudioEngine_Process(AudioEngine *engine, float *outBuffer, int frames);
void AudioEngine_SetFXRouting(AudioEngine *engine, FXRoutingMode mode);

void DeckAudio_LoadTrack(DeckAudioState *deck, const char *filePath);
void DeckAudio_LoadTrackAsync(DeckAudioState *deck, const char *filePath);
void DeckAudio_Play(DeckAudioState *deck);
void DeckAudio_Stop(DeckAudioState *deck);
void DeckAudio_SetPlaying(DeckAudioState *deck, bool playing);
void DeckAudio_InstantPlay(DeckAudioState *deck); // Start without vinyl ramp
// Called when jog wheel is moved during touch
void DeckAudio_SetJogRate(DeckAudioState *deck, double delta);
void DeckAudio_SetJogTouch(DeckAudioState *deck, bool touching);
void DeckAudio_JumpToMs(DeckAudioState *deck, int64_t ms);
void DeckAudio_QueueJumpMs(DeckAudioState *deck, uint32_t targetMs,
                           uint32_t waitMs);
void DeckAudio_SetPitch(DeckAudioState *deck, uint16_t pitch);
void DeckAudio_Unload(DeckAudioState *deck);
void DeckAudio_TriggerReleaseFX(DeckAudioState *deck, int type);

void DeckAudio_SetSlip(DeckAudioState *deck, bool active);
void DeckAudio_SetLoop(DeckAudioState *deck, bool active, double startPos,
                       double endPos);
void DeckAudio_ExitLoop(DeckAudioState *deck);
void DeckAudio_ClearMT(DeckAudioState *deck);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_ENGINE_H
