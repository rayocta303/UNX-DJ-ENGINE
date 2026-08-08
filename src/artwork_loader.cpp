/**
 * artwork_loader.cpp — BUG-A, BUG-B, BUG-C fix
 *
 * Fixes:
 *   BUG-A: std::atomic<bool> for ready/loading (acquire/release ordering)
 *   BUG-B: std::mutex g_stbiMutex — serializes stbi_load calls from concurrent threads
 *   BUG-C: ArtworkPending defined once in artwork_loader.h
 */

#include "artwork_loader.h"
#include "raylib.h"
#include "core/logger.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

// BUG-B FIX: stbi_load has internal global state (stbi__g_failure_reason etc.)
// — not safe to call from multiple threads concurrently.
static std::mutex g_stbiMutex;

extern "C" {

Image LoadImageManual(const char *path); // defined in main.c

void ArtworkLoader_Kick(void *slot, const char *path) {
  ArtworkPending *pend = static_cast<ArtworkPending *>(slot);
  std::string filePath(path);

  std::thread([pend, filePath]() {
    // BUG-B: serialize all stbi_load calls to prevent concurrent global state access
    Image img;
    {
      std::lock_guard<std::mutex> lk(g_stbiMutex);
      img = LoadImageManual(filePath.c_str());
    }

    if (img.data) {
      // ImageResize / ImageCopy / ImageFormat are pure CPU ops — no global state
      ImageResize(&img, 128, 128);
      Image rgba = ImageCopy(img);
      ImageFormat(&rgba, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      UnloadImage(img);

      void *pixels = malloc(static_cast<size_t>(rgba.width * rgba.height * 4));
      if (pixels) {
        memcpy(pixels, rgba.data, static_cast<size_t>(rgba.width * rgba.height * 4));
        // BUG-A FIX: write data/w/h BEFORE signalling ready with release store
        pend->data = pixels;
        pend->w    = rgba.width;
        pend->h    = rgba.height;
      }
      UnloadImage(rgba);
      UNX_LOG_INFO("[ARTWORK] Loaded async: '%s' (128x128)", filePath.c_str());
    } else {
      UNX_LOG_WARN("[ARTWORK] Failed to decode: '%s'", filePath.c_str());
      pend->data = nullptr;
    }

    // BUG-A FIX: release store ensures data/w/h are visible to render thread
    // before ready becomes true (acquire load in ManageArtwork).
    pend->ready.store(true, std::memory_order_release);
    pend->loading.store(false, std::memory_order_relaxed);
  }).detach();
}

bool  ArtworkLoader_IsReady(void *slot)   { return static_cast<ArtworkPending*>(slot)->ready.load(std::memory_order_acquire); }
bool  ArtworkLoader_IsLoading(void *slot) { return static_cast<ArtworkPending*>(slot)->loading.load(std::memory_order_relaxed); }

void *ArtworkLoader_TakePixels(void *slot, int *outW, int *outH) {
  auto *p = static_cast<ArtworkPending*>(slot);
  *outW = p->w; *outH = p->h;
  void *d = p->data; p->data = nullptr;
  return d;
}

void ArtworkLoader_Abort(void *slot) {
  auto *p = static_cast<ArtworkPending*>(slot);
  if (p->data) { free(p->data); p->data = nullptr; }
  p->ready.store(false, std::memory_order_relaxed);
}

} // extern "C"
