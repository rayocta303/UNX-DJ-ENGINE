#pragma once
#include "ui/components/component.h"
#include <stdbool.h>
#include <stdint.h>

#include "library/rekordbox_reader.h"
#include "library/serato_reader.h"

typedef struct {
    char Name[128];
    char Path[512];
    char Type[32]; // "Testing", "USB", "SD", "RB/Serato"
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

    StorageDevice AvailableStorages[16];
    int StorageCount;
    StorageDevice *SelectedStorage;
    RBDatabase *DB;
    SeratoDatabase *SeratoDB;
    int DatabaseType; // 0=Rekordbox, 1=Serato

    // Filtered view
    RBTrack **TrackPointers;
    SeratoTrack **SeratoTrackPointers;
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
    
    // Touch scrolling and Popups
    float TouchDragAccumulator;
    bool ShowLoadPopup;
    int PopupTrackIdx;

    // Animation for Marquee
    float MarqueeScrollX;
    double LastAnimTime;
} BrowserState;

void Browser_RefreshStorages(BrowserState *s);
void Browser_Back(BrowserState *s);

typedef struct {
    Component base;
    BrowserState *State;
} BrowserRenderer;

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state);
