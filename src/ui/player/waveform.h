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
  unsigned int cachedTrackLength;
  int dynWfmFrames;
  float dataDensity;
  float lastMouseX;
};

static const float ZOOM_LEVELS[] = {
    0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 
    4.0f, 6.0f, 8.0f
};
static const int NUM_ZOOM_LEVELS = sizeof(ZOOM_LEVELS) / sizeof(ZOOM_LEVELS[0]);

void WaveformRenderer_Init(WaveformRenderer *r, int id, DeckState *state,
                           DeckState *otherDeck);

// Waveform decoding utilities for shared use
typedef struct {
  unsigned char r, g, b, a;
} WFColor;

extern const unsigned char PWV2_BLUE_TABLE[8][3];
int PWV2_Decode(unsigned char v, Color *outColor);
int PWV4_Decode(unsigned char *data, int64_t frame, int64_t maxFrames, Color *outColor);
void Get3BandPeak(unsigned char *data, int64_t maxFrames, double start, double end, float *outL, float *outM, float *outH);
