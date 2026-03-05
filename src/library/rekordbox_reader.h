#ifndef REKORDBOX_READER_H
#define REKORDBOX_READER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t Time; // ms
    uint16_t ID;   // 1-8 for HotCue, 0 for Memory
    uint16_t Type; // 1=Memory, 2=Loop
    uint32_t LoopTime; 
    char Comment[64];
    unsigned char Color[3]; // RGB
} RBCue;

typedef struct {
    uint32_t Time; // ms
    uint16_t BPM;  // BPM * 100
    uint16_t BeatNumber; 
} RBBeat;

typedef struct {
    RBBeat* Beats;
    uint32_t BeatCount;
} RBBeatGrid;

typedef struct {
    uint32_t ID;
    char Title[256];
    char Artist[256];
    char Album[256];
    char Genre[256];
    char Key[32];
    float BPM;
    uint32_t Duration;
    char FilePath[512];
    char ArtworkPath[512];
    char AnalyzePath[512];
    
    // Analysis data
    RBCue* Cues;
    uint32_t CueCount;
    unsigned int BeatGrid[1024];
    int BeatGridCount;
    unsigned char StaticWaveform[1024];
    int StaticWaveformLen;
    unsigned char* DynamicWaveform;
    int DynamicWaveformLen;
} RBTrack;

typedef struct {
    uint32_t ID;
    char Name[256];
    uint32_t* TrackIDs;
    uint32_t TrackCount;
} RBPlaylist;

typedef struct {
    RBTrack* Tracks;
    uint32_t TrackCount;
    RBPlaylist* Playlists;
    uint32_t PlaylistCount;
} RBDatabase;

// Path should be the root of the USB (e.g. "p:/XDJ-UNX-C/usb_test")
RBDatabase* RB_LoadDatabase(const char* rootPath);
void RB_FreeDatabase(RBDatabase* db);

// Loads analysis data for a specific track
void RB_LoadTrackData(RBTrack* track, const char* rootPath);

#ifdef __cplusplus
}
#endif

#endif
