#pragma once

#include "ui/components/component.h"
#include <stdbool.h>

typedef struct {
  char Title[256];
  char Artist[128];
  char Album[128];
  char Genre[64];
  float BPM;
  char Key[16];
  int Duration; // in seconds
  int Rating;   // 0-5
  char Source[32];
  char FilePath[256];
  char Label[128];
  int Year;
  char Comment[256];
  char ArtworkPath[256];
  void *ArtworkTexture; // Pointer to Texture2D in DeckState
} InfoTrack;

typedef struct {
  bool IsActive;
  InfoTrack Tracks[2];
} InfoState;

typedef struct InfoRenderer InfoRenderer;

struct InfoRenderer {
  Component base;
  InfoState *State;
};

void InfoRenderer_Init(InfoRenderer *r, InfoState *state);
