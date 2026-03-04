#pragma once
#include "ui/components/component.h"
#include <stdbool.h>
#include <stdint.h>

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
} BrowserState;

typedef struct {
    Component base;
    BrowserState *State;
} BrowserRenderer;

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state);
