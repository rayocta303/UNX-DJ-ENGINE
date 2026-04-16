#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"
#include <raylib.h>

typedef struct WaveformRenderer WaveformRenderer;

struct WaveformRenderer {
  Component base;
  int ID;
  DeckState *State;
  DeckState *OtherDeck;
  TrackState *cachedTrack;
  int dynWfmFrames;
  float dataDensity;
  float lastMouseX;
  Texture2D Logo;
};



void WaveformRenderer_Init(WaveformRenderer *r, int id, DeckState *state,
                           DeckState *otherDeck);

// Waveform decoding utilities for shared use
typedef struct {
  unsigned char r, g, b, a;
} WFColor;

extern const unsigned char PWV2_BLUE_TABLE[8][3];
int PWV2_Decode(unsigned char v, Color *outColor);
int PWV4_Decode(unsigned char *data, int64_t frame, Color *outColor);
void Get3BandPeak(unsigned char *data, int64_t maxFrames, double start, double end, float *outL, float *outM, float *outH);
