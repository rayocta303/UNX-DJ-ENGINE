#ifndef HOTCUE_DB_H
#define HOTCUE_DB_H

#include "ui/player/player_state.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void HotCueDB_Init(const char* storagePath);
void HotCueDB_LoadStorage(const char* storagePath);
void HotCueDB_SaveTrack(const char* storagePath, const char* filePath, const char* title, const char* artist, const HotCue* hotCues, int count);
bool HotCueDB_GetTrack(const char* storagePath, const char* filePath, const char* title, const char* artist, HotCue* outHotCues, int* outCount);

#ifdef __cplusplus
}
#endif

#endif
