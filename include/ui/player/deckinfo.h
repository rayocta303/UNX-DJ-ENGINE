#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"

typedef struct DeckInfoPanel DeckInfoPanel;

struct DeckInfoPanel {
    Component base;
    int ID;
    DeckState *State;
};

void DeckInfoPanel_Init(DeckInfoPanel *p, int id, DeckState *state);
