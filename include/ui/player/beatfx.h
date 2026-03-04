#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"

typedef struct BeatFXPanel BeatFXPanel;

struct BeatFXPanel {
    Component base;
    BeatFXState *State;
};

void BeatFXPanel_Init(BeatFXPanel *b, BeatFXState *state);
