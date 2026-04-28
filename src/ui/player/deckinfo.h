#pragma once
#include "audio/engine.h"
#include "ui/components/component.h"
#include "ui/player/player_state.h"

typedef struct DeckInfoPanel DeckInfoPanel;

struct DeckInfoPanel {
    Component base;
    int ID;
    DeckState *State;
    AudioEngine *Engine;
};

void DeckInfoPanel_Init(DeckInfoPanel *p, int id, DeckState *state, AudioEngine *engine);
