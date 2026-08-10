#ifndef UNX_DATABASE_H
#define UNX_DATABASE_H

#include "ui/player/player_state.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the database and history subsystem on the given storage path.
// Creates the UNX-DJ folder and History subfolder, and sets up the session CSV file.
void UNXDatabase_Init(const char* storagePath);

// Loads the hotcue database from the storage
void UNXDatabase_LoadStorage(const char* storagePath);

// Hotcue management
void UNXDatabase_SaveTrack(const char* storagePath, const char* filePath, const char* title, const char* artist, const HotCue* hotCues, int count);
bool UNXDatabase_GetTrack(const char* storagePath, const char* filePath, const char* title, const char* artist, HotCue* outHotCues, int* outCount);

// History logging
void UNXDatabase_LogHistory(const char* storagePath, DeckState* deck);

#ifdef __cplusplus
}
#endif

#endif
