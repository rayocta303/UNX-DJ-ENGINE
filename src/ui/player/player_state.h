#pragma once
#include <stdbool.h>
#include "library/rekordbox_reader.h"

typedef struct HotCue {
    unsigned int Start; // ms
    unsigned int ID;
    unsigned char Color[3]; // RGB
} HotCue;

typedef struct TrackState {
    RBBeat BeatGrid[1024]; 
    unsigned int GridOffset;
    int BeatGridCount;
    unsigned char StaticWaveform[1024]; 
    int StaticWaveformLen;
    unsigned char *DynamicWaveform;
    int DynamicWaveformLen;
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
    bool IsTouching;   // Renamed from IsScratching to be more general
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
