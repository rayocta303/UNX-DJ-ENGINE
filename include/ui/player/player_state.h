#pragma once
#include <stdbool.h>

typedef struct {
    unsigned int Start; // ms
    unsigned int ID;
} HotCue;

typedef struct {
    unsigned int BeatGrid[1024]; 
    unsigned int GridOffset;
    unsigned char StaticWaveform[1024]; 
    int StaticWaveformLen;
    HotCue HotCues[8];
    int HotCuesCount;
} TrackState;

typedef struct {
    int ID;
    char SourceName[32];
    char TrackTitle[128];
    char TrackKey[16];
    int TrackNumber;
    bool QuantizeEnabled;
    bool MasterTempo;
    int TempoRange; // 0=6%, 1=10%, 2=16%, 3=WIDE
    float TempoPercent;
    bool IsMaster;
    
    TrackState *LoadedTrack;
    
    long long PositionMs;
    long long TrackLengthMs;

    float (*CurrentBPM)(void* self);
    void *bpmCtx;
} DeckState;

typedef struct {
    int SelectedFX;
    int SelectedPad;
} BeatFXState;
