#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"

typedef struct BeatFXPanel BeatFXPanel;

struct BeatFXPanel {
    Component base;
    BeatFXState *State;
    DeckState *DeckA;
    DeckState *DeckB;
    Rectangle FXButton;
};

void BeatFXPanel_Init(BeatFXPanel *b, BeatFXState *state, DeckState *a, DeckState *b_state);
