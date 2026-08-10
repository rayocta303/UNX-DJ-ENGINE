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
    uint16_t Status; // 1=Enabled, 4=ActiveLoop
    uint32_t LoopTime; 
    char Comment[64];
    unsigned char Color[3]; // RGB
} RBCue;

typedef struct {
    uint16_t Index;        // Phrase absolute number
    uint16_t Beat;         // Starting beat number
    char Kind[32];         // String representation (Up, Down, Chorus, Intro)
    int KindID;            // Enum mapping
} RBPhrase;

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
    RBCue* Cues;
    uint32_t CueCount;
    RBPhrase* Phrases;
    uint32_t PhraseCount;
    RBBeat* BeatGrid;
    int BeatGridCount;
    unsigned char StaticWaveform[8192];
    int StaticWaveformLen;
    int StaticWaveformType; // 0=None, 1=Blue, 2=Color, 3=3Band
    unsigned char* DynamicWaveform;
    int DynamicWaveformLen;
    int WaveformType; // 0=None, 1=Blue, 2=Color, 3=3Band
} RBAnalysis;

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
    char Label[128];
    char Comment[256];
    char Remixer[128];
    char Composer[128];
    char MixName[128];
    char DateAdded[32];
    char ReleaseDate[32];
    char ISRC[32];
    int Rating;
    int Year;
    int Bitrate;
    int SampleRate;
    int PlayCount;
    int ColorID;
    int TrackNumber;
    int DiscNumber;
    int BrowserDisplayNumber; // Used for UI sorting and display
    
    RBAnalysis Analysis;
} RBTrack;

typedef struct {
    uint32_t ID;
    uint32_t ParentID;
    bool IsFolder;
    char Name[256];
    uint32_t* TrackIDs;
    uint32_t TrackCount;
} RBPlaylist;

typedef struct {
    RBTrack* Tracks;
    uint32_t TrackCount;
    RBPlaylist* Playlists;
    uint32_t PlaylistCount;
    RBPlaylist* History;
    uint32_t HistoryCount;
} RBDatabase;

// Path should be the root of the USB (e.g. "p:/UNX-ENGINE/usb_test")
RBDatabase* RB_LoadDatabase(const char* rootPath);
void RB_FreeDatabase(RBDatabase* db);

// Loads analysis data for a specific track
void RB_LoadTrackData(RBAnalysis* analysis, const char* analyzePath, const char* title, uint32_t id, const char* rootPath);

// Low-level reload for memory management (re-parses waveform sections into existing pointers)
void RB_ReloadWaveform(const char* path, unsigned char** outData, int* outLen, int* outType);

#ifdef __cplusplus
}
#endif

#endif
