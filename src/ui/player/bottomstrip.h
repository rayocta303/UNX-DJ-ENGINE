#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"

#include "audio/engine.h"

typedef struct BeatFXSelectBar BeatFXSelectBar;

struct BeatFXSelectBar {
    Component base;
    BeatFXState *State;
    DeckState *DeckA;
    DeckState *DeckB;
    AudioEngine *AudioPlugin;
};

void BeatFXSelectBar_Init(BeatFXSelectBar *b, BeatFXState *state, DeckState *a, DeckState *b_ptr, AudioEngine *audioPlugin);
