#pragma once

#include "ui/components/component.h"
#include "ui/player/player_state.h"

typedef struct {
    bool IsActive;
    DeckState *Decks[2];
} PadState;

typedef struct {
    Component base;
    PadState *State;
    void (*OnPadPress)(void *ctx, int deckIdx, int padIdx);
    void *callbackCtx;
} PadRenderer;

void PadRenderer_Init(PadRenderer *r, PadState *state);
