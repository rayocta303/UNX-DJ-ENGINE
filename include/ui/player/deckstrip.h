#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"

typedef struct DeckStrip DeckStrip;

struct DeckStrip {
    Component base;
    int ID;
    DeckState *State;
};

void DeckStrip_Init(DeckStrip *d, int id, DeckState *state);
