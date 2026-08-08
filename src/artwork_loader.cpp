/**
 * artwork_loader.cpp
 * Async artwork decode worker — BUG-07 fix.
 *
 * Disk I/O + image decode + resize run on a detached std::thread.
 * main.c (pure C) calls these extern "C" helpers so the render thread
 * never blocks on file I/O.
 *
 * Thread safety:
 *   ArtworkPending.loading is written only by the render thread (kick/abort)
 *   and the worker thread (clear on finish) — one writer per field at a time.
 *   ArtworkPending.ready / .data are written only by the worker thread and
 *   read by the render thread AFTER loading == false, so no mutex needed for
 *   the simple state machine used here.
 */

#include "raylib.h"
#include "core/logger.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

// Must match the ArtworkPending typedef in main.c (plain-C layout)
struct ArtworkPending {
  bool  loading;
  bool  ready;
  void *data;
  int   w, h;
  char  path[512];
};

extern "C" {

// Declared in main.c and called from ManageArtwork
Image LoadImageManual(const char *path); // defined in main.c / raylib wrapper

void ArtworkLoader_Kick(void *slot, const char *path) {
  ArtworkPending *pend = static_cast<ArtworkPending *>(slot);

  std::string filePath(path);

  std::thread([pend, filePath]() {
    Image img = LoadImageManual(filePath.c_str());
    if (img.data) {
      ImageResize(&img, 128, 128);
      // Convert to a plain RGBA pixel buffer so the render thread can upload
      // without holding a Raylib Image struct across thread boundaries.
      Image rgba = ImageCopy(img);
      ImageFormat(&rgba, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      UnloadImage(img);

      void *pixels = malloc(static_cast<size_t>(rgba.width * rgba.height * 4));
      if (pixels) {
        memcpy(pixels, rgba.data, static_cast<size_t>(rgba.width * rgba.height * 4));
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
    // Render thread polls pend->ready after pend->loading clears
    pend->ready   = true;
    pend->loading = false;
  }).detach();
}

// Unused stubs — kept for the forward declarations in main.c
bool  ArtworkLoader_IsReady(void *slot)   { return static_cast<ArtworkPending*>(slot)->ready; }
bool  ArtworkLoader_IsLoading(void *slot) { return static_cast<ArtworkPending*>(slot)->loading; }
void *ArtworkLoader_TakePixels(void *slot, int *outW, int *outH) {
  auto *p = static_cast<ArtworkPending*>(slot);
  *outW = p->w; *outH = p->h;
  void *d = p->data; p->data = nullptr;
  return d;
}
void ArtworkLoader_Abort(void *slot) {
  auto *p = static_cast<ArtworkPending*>(slot);
  if (p->data) { free(p->data); p->data = nullptr; }
  p->ready = false;
}

} // extern "C"
