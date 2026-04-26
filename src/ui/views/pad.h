#pragma once

#include "ui/components/component.h"
#include "ui/player/player_state.h"

typedef enum {
    PAD_MODE_HOT_CUE,
    PAD_MODE_BEAT_LOOP,
    PAD_MODE_SLIP_LOOP,
    PAD_MODE_BEAT_JUMP,
    PAD_MODE_GATE_CUE,
    PAD_MODE_RELEASE_FX,
    PAD_MODE_COUNT
} PadMode;

typedef struct {
    bool IsActive;
    DeckState *Decks[2];
    PadMode Mode[2];
} PadState;

typedef struct {
    Component base;
    PadState *State;
    void (*OnPadPress)(void *ctx, int deckIdx, int padIdx);
    void (*OnPadRelease)(void *ctx, int deckIdx, int padIdx);
    void (*OnModeChange)(void *ctx, int deckIdx, PadMode mode);
    void *callbackCtx;
} PadRenderer;

void PadRenderer_Init(PadRenderer *r, PadState *state);
