/**
 * artwork_loader.h
 * Shared definition of ArtworkPending — included by both main.c and artwork_loader.cpp.
 * BUG-C fix: eliminates the duplicate struct definition that was in both files.
 */
#pragma once

#ifdef __cplusplus
#include <atomic>
// C++ version uses std::atomic for correct memory ordering (BUG-A fix)
struct ArtworkPending {
  std::atomic<bool> loading; // written by render thread (kick) + worker (clear)
  std::atomic<bool> ready;   // written only by worker thread (acquire/release)
  void *data;                // pixel buffer — written BEFORE ready.store(release)
  int   w, h;
  char  path[512];
};
#else
// Plain-C version for main.c — uses plain bool (single-threaded access pattern
// enforced by the state machine: render thread only reads ready AFTER loading clears)
#include <stdbool.h>
typedef struct {
  bool   loading;
  bool   ready;
  void  *data;
  int    w, h;
  char   path[512];
} ArtworkPending;

// Forward declarations for ManageArtwork (implemented in artwork_loader.cpp)
void  ArtworkLoader_Kick(void *pendingSlot, const char *path);
bool  ArtworkLoader_IsReady(void *pendingSlot);
bool  ArtworkLoader_IsLoading(void *pendingSlot);
void *ArtworkLoader_TakePixels(void *pendingSlot, int *outW, int *outH);
void  ArtworkLoader_Abort(void *pendingSlot);
#endif
