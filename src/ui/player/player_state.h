#pragma once
#include "library/rekordbox_reader.h"
#include <stdbool.h>
#include "raylib.h"

typedef struct {
  unsigned int Start; // ms
  unsigned int LoopTime; // ms (0 if not loop)
  unsigned int ID;
  unsigned int Status; // 1=Enabled, 4=ActiveLoop
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
  float JogCalibRPM;
} WaveformSettings;

typedef struct TrackState {
  RBAnalysis Analysis; // Source of truth for raw data
  
  // Legacy/UI specific mappings (preserved for backward compatibility)
  unsigned int GridOffset;
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

  char AnalyzePath[512]; // Path to .ANLZ/EXT file for reloading
  char FilePath[512];
  char Title[128];
  char Artist[128];
} TrackState;

typedef struct DeckState {
  int ID;
  char SourceName[32];
  char TrackTitle[128];
  char ArtistName[128];
  char AlbumName[128];
  char GenreName[64];
  char LabelName[128];
  char MixName[128];
  char Remixer[128];
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
  float LoadAnimTimer; // >0 when track load animation is active (seconds)
  float JogPointerAngle; // Platter/jog pointer rotation angle (degrees)
  
  TrackState *LoadedTrack;

  double Position; // Native half-frames (150Hz) for scrolling
  long long PositionMs;
  long long TrackLengthMs;

  float ZoomScale; // Native Pioneer zoom (1-32, now float for precision)
  float CurrentBPM;
  float OriginalBPM; // Added OriginalBPM missing from struct

  float LastPhaseAdjustment; // For Phase (Beat) Sync proportional control
  bool IsPhaseDrifted;        // Sync is on but phase doesn't match master
  void *bpmCtx;

  WaveformSettings Waveform;
  int TimeMode; // 0=Elapsed, 1=Remaining
  long long SeekMs;
  bool HasSeekRequest;
  bool IsLoading;
  float LoadingProgress; // 0.0 to 1.0
  Texture2D ArtworkTexture;
  char LastLoadedArtPath[512]; // Internal cache key
  long long MainCueMs;
  bool IsCueHeld;
  bool IsCueActive; // For blinking or UI feedback
  bool IsLooping;
  bool LoopAdjustIn;
  bool LoopAdjustOut;

  // MIDI Interaction Flags
  bool MidiRequestHotCue[8];
  bool MidiRequestHotCueClear[8];
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
  bool MidiRequestMemoryCue;
  bool MidiRequestPlay;
  bool MidiRequestCue;
} DeckState;

typedef struct BeatFXState {
  int SelectedFX;
  int SelectedPad;
  int SelectedChannel; // 0=Master, 1=Deck 1, 2=Deck 2
  bool ChannelDropdownOpen;
  bool FXDropdownOpen;
  bool ShowBeatFXTab; // false = STATUS, true = BEAT FX
  bool IsFXOn;
  float LevelDepth; // Persist knob depth
  float XPadScrubValue; // -1.0 to 1.0 for Reverb LPF/HPF and Flanger Sweep
  bool IsXPadScrubbing; // True when holding the scrub line
  bool Quantize;

  // MIDI request flags
  bool MidiRequestPrevFX;
  bool MidiRequestNextFX;
  bool MidiRequestToggleFX;
  bool MidiRequestCh1;
  bool MidiRequestCh2;
  bool MidiRequestCh3;
  bool MidiRequestCh4;
  bool MidiRequestChMaster;
  bool MidiRequestBeatLeft;
  bool MidiRequestBeatRight;
  bool MidiRequestTap;
} BeatFXState;

