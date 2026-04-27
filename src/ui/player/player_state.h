#pragma once
#include "library/rekordbox_reader.h"
#include <stdbool.h>
#include "raylib.h"

typedef struct {
  unsigned int Start; // ms
  unsigned int ID;
  unsigned char Color[3]; // RGB
} HotCue;

// Helper to get Color from HotCue with fallback
static inline Color GetCueColor(HotCue cue, Color fallback) {
    if (cue.Color[0] == 0 && cue.Color[1] == 0 && cue.Color[2] == 0) return fallback;
    return (Color){ cue.Color[0], cue.Color[1], cue.Color[2], 255 };
}

typedef enum {
  WAVEFORM_STYLE_BLUE = 0,
  WAVEFORM_STYLE_RGB = 1,
  WAVEFORM_STYLE_3BAND = 2
} WaveformStyle;

typedef struct {
  WaveformStyle Style;
  float GainLow;
  float GainMid;
  float GainHigh;
  float VinylStartMs;
  float VinylStopMs;
  bool LoadLock;
} WaveformSettings;

typedef struct TrackState {
  RBBeat* BeatGrid;
  unsigned int GridOffset;
  int BeatGridCount;
  unsigned char StaticWaveform[8192];
  int StaticWaveformLen;
  int StaticWaveformType; // 0=None, 1=Blue, 2=Color, 3=3Band
  unsigned char *DynamicWaveform;
  int DynamicWaveformLen;
  int WaveformType; // 0=None, 1=Blue, 2=Color, 3=3Band
  HotCue HotCues[8];
  int HotCuesCount;
  HotCue Cues[32]; // Memory Cues
  int CuesCount;

  struct {
    int Index;
    int Beat;
    char Kind[32];
    int KindID;
  } Phrases[64];
  int PhraseCount;
} TrackState;

typedef struct DeckState {
  int ID;
  char SourceName[32];
  char TrackTitle[128];
  char ArtistName[128];
  char AlbumName[128];
  char GenreName[64];
  char LabelName[128];
  char Comment[256];
  int Rating;
  int Year;
  char TrackKey[16];
  char ArtworkPath[512];
  int TrackNumber;
  bool QuantizeEnabled;
  int SyncMode; // 0=OFF, 1=BPM, 2=BEAT
  bool MasterTempo;
  int TempoRange; // 0=6%, 1=10%, 2=16%, 3=WIDE
  float TempoPercent;

  // Hardware integration
  float HardwarePitchPercent;
  bool PitchTakeoverActive;
  float TargetTakeoverPercent;

  bool IsMaster;
  bool IsTouching; // Renamed from IsScratching to be more general
  bool VinylModeEnabled;
  bool IsPlaying;
  float JogRate;
  double JogDelta; // Pending jog/touch movement from UI (half-frames)
  
  TrackState *LoadedTrack;

  double Position; // Native half-frames (150Hz) for scrolling
  long long PositionMs;
  long long TrackLengthMs;

  float ZoomScale; // Native Pioneer zoom (1-32, now float for precision)
  float CurrentBPM;
  float OriginalBPM; // Added OriginalBPM missing from struct

  float LastPhaseAdjustment; // For Phase (Beat) Sync proportional control
  void *bpmCtx;

  WaveformSettings Waveform;
  int TimeMode; // 0=Elapsed, 1=Remaining
  long long SeekMs;
  bool HasSeekRequest;
  bool IsLoading;
  float LoadingProgress; // 0.0 to 1.0
  Texture2D ArtworkTexture;
  char LastLoadedArtPath[512]; // Internal cache key

  // MIDI Interaction Flags
  bool MidiRequestHotCue[8];
  bool MidiRequestLoopIn;
  bool MidiRequestLoopOut;
  bool MidiRequestLoopExit;
  bool MidiRequestLoopHalve;
  bool MidiRequestLoopDouble;
  bool MidiRequestPitchBendPlus;
  bool MidiRequestPitchBendMinus;
  bool MidiRequestSync;
  bool MidiRequestMaster;
  bool MidiRequestBeatJumpForward;
  bool MidiRequestBeatJumpBackward;
  bool MidiRequestAutoLoop[5]; // 1, 2, 4, 8, 16 beats
} DeckState;

typedef struct BeatFXState {
  int SelectedFX;
  int SelectedPad;
  int SelectedChannel; // 0=Master, 1=Deck 1, 2=Deck 2
  bool ChannelDropdownOpen;
  bool FXDropdownOpen;
  bool ShowBeatFXTab; // false = STATUS, true = BEAT FX
  bool IsFXOn;
  float XPadScrubValue; // -1.0 to 1.0 for Reverb LPF/HPF and Flanger Sweep
  bool IsXPadScrubbing; // True when holding the scrub line
  bool Quantize;
} BeatFXState;
