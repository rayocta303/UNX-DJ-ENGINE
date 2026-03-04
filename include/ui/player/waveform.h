#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"

typedef struct WaveformRenderer WaveformRenderer;

struct WaveformRenderer {
    Component base;
    int ID;
    DeckState *State;
    int dynWfmFrames;
    float lastMouseX;
};

void WaveformRenderer_Init(WaveformRenderer *r, int id, DeckState *state);
