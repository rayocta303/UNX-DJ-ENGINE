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
    bool IsScratching;
    float ScratchSpeed;
    
    TrackState *LoadedTrack;
    
    long long PositionMs;
    long long TrackLengthMs;

    float CurrentBPM;
    float OriginalBPM; // Added OriginalBPM missing from struct

    void *bpmCtx;
} DeckState;

typedef struct BeatFXState {
    int SelectedFX;
    int SelectedPad;
} BeatFXState;
