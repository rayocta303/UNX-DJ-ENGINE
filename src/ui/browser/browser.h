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
    bool HasBothDatabases;

    // Filtered view
    RBTrack **TrackPointers;
    SeratoTrack **SeratoTrackPointers;
    int ActiveTrackCount;
    
    // Core Engine Reference
    struct AudioEngine *AudioPlugin;
    struct DeckState *DeckA;
    struct DeckState *DeckB;

    // Playlist Bank (Slot shortcuts)
    struct {
        int PlaylistIdx;       // -1 if empty
        char StoragePath[512]; // Source storage path
        char Name[64];         // Cached name for display
    } PlaylistBank[3];
    
    // Drag and Drop (Mouse interaction)
    bool IsDragging;
    int DraggingIdx;         // Playlist index being dragged
    int DraggingType;        // 0=Tracks, 1=Playlists
    
    // Touch scrolling and Popups
    float TouchDragAccumulator;
    bool ShowLoadPopup;
    int PopupTrackIdx;

    // Touch kinetic & Scrollbar interaction
    float LastTouchY;
    float TouchVelocityY;
    bool IsScrollbarDragging;
    float ScrollbarDragStartY;
    float ScrollbarDragStartScroll;

    // Animation for Marquee and Smooth Scrolling
    float MarqueeScrollX;
    double LastAnimTime;
    float VisualScroll;      // Pixel-based scroll position
    float ScrollVelocity;    // For momentum

    // MIDI/External Interaction Flags
    bool MidiRequestEnter;
    bool MidiRequestBack;
    bool MidiRequestUp;
    bool MidiRequestDown;
    int MidiBrowseDelta;
    bool MidiRequestLoadA;
    bool MidiRequestLoadB;

    // Search & On-Screen Keyboard (OSK) functionality
    char SearchQuery[64];
    bool IsSearching;
    bool ShowOSK;
    bool OSKShiftActive;
    int OSKMode;          // 0 = QWERTY Letters, 1 = Numbers & Symbols

    // Sort functionality
    int SortMode;         // 0 = Default, 1 = BPM, 2 = Key
    bool ShowSortDropdown;
} BrowserState;

void Browser_RefreshStorages(BrowserState *s);
void Browser_CheckStorageConnection(BrowserState *s);
void Browser_Back(BrowserState *s);

typedef struct {
    Component base;
    BrowserState *State;
} BrowserRenderer;

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state);

