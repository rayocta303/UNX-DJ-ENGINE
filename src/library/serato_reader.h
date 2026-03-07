#ifndef SERATO_READER_H
#define SERATO_READER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t R, G, B, A;
} SeratoWaveformSample;

typedef struct {
    SeratoWaveformSample* Samples;
    uint32_t SampleCount;
} SeratoWaveform;

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
    char Comment[256];
} SeratoTrack;

typedef struct {
    uint32_t ID;
    char Name[256];
    uint32_t* TrackIDs;
    uint32_t TrackCount;
} SeratoPlaylist;

typedef struct {
    SeratoTrack* Tracks;
    uint32_t TrackCount;
    SeratoPlaylist* Playlists;
    uint32_t PlaylistCount;
} SeratoDatabase;

// Path should be the root of the music folder containing _Serato_
SeratoDatabase* Serato_LoadDatabase(const char* rootPath);
void Serato_FreeDatabase(SeratoDatabase* db);

// Waveform Loading
SeratoWaveform* Serato_LoadWaveformFromBase64(const char* base64Data);
void Serato_FreeWaveform(SeratoWaveform* wf);

#ifdef __cplusplus
}
#endif

#endif
