#pragma once
#include <stdbool.h>

typedef struct HotCue {
    unsigned int Start; // ms
    unsigned int ID;
} HotCue;

typedef struct TrackState {
    unsigned int BeatGrid[1024]; 
    unsigned int GridOffset;
    unsigned char StaticWaveform[1024]; 
    int StaticWaveformLen;
    unsigned char *DynamicWaveform;
    int DynamicWaveformLen;
    HotCue HotCues[8];
    int HotCuesCount;
    HotCue Cues[32]; // Memory Cues
    int CuesCount;
} TrackState;

typedef struct DeckState {
    int ID;
    char SourceName[32];
    char TrackTitle[128];
    char ArtistName[128];
    char TrackKey[16];
    int TrackNumber;
    bool QuantizeEnabled;
    bool MasterTempo;
    int TempoRange; // 0=6%, 1=10%, 2=16%, 3=WIDE
    float TempoPercent;
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

    void *bpmCtx;
} DeckState;

typedef struct BeatFXState {
    int SelectedFX;
    int SelectedPad;
    int SelectedChannel; // 0=Master, 1=Deck 1, 2=Deck 2
    bool ChannelDropdownOpen;
} BeatFXState;
