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
};

// Generic waveform drawing routine for both scrolling and static overviews
void Waveform_DrawGeneric(
    Rectangle bounds, unsigned char *data, int dataLen,
    double
        positionHalfFrames, // For scrolling: playhead pos. For static: unused/0
    float zoomStep,         // For scrolling: samples per pixel. For static:
                            // totalLen/width
    bool isStatic, // True for deckstrip overview, False for scrolling track
    WaveformStyle style, float gLow, float gMid, float gHigh,
    float playedRatio,  // 0.0-1.0 to dim played area (only for static)
    float startScreenX, // Usually 0
    float endScreenX    // Usually bounds.width
);

void WaveformRenderer_Init(WaveformRenderer *r, int id, DeckState *state,
                           DeckState *otherDeck);
