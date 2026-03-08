#pragma once
#include "ui/components/component.h"
#include <stdbool.h>
#include <stdint.h>

#include "library/rekordbox_reader.h"

typedef struct {
    char Name[64];
    char Path[256];
    char Type[16]; // "Testing", "USB", "SD"
} StorageDevice;

typedef struct {
    bool IsActive;
    int CursorPos;
    int ScrollOffset;
    int BrowseLevel; // 0=Tracks, 1=Playlists, 2=Categories, 3=Source
    bool InfoEnabled;
    int CurrentPlaylistIdx;
    bool IsTagList;
    uint16_t TagList[256];
    int TagListCount;

    StorageDevice AvailableStorages[8];
    int StorageCount;
    StorageDevice *SelectedStorage;
    RBDatabase *DB;

    // Filtered view
    RBTrack **TrackPointers;
    int ActiveTrackCount;
    
    // Core Engine Reference
    struct AudioEngine *AudioPlugin;
    struct DeckState *DeckA;
    struct DeckState *DeckB;

    // Playlist Bank (Slot shortcuts)
    int PlaylistBankIdx[3]; // Stores index of playlist in DB->Playlists (-1 if empty)
    
    // Drag and Drop (Mouse interaction)
    bool IsDragging;
    int DraggingIdx;         // Playlist index being dragged
    int DraggingType;        // 0=Tracks, 1=Playlists
} BrowserState;

void Browser_RefreshStorages(BrowserState *s);
void Browser_Back(BrowserState *s);

typedef struct {
    Component base;
    BrowserState *State;
} BrowserRenderer;

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state);
