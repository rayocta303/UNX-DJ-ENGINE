#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"
#include "ui/player/deckinfo.h"
#include "ui/player/deckstrip.h"
#include "ui/player/beatfx.h"
#include "ui/player/waveform.h"
#include "ui/player/bottomstrip.h"

typedef struct PlayerRenderer PlayerRenderer;

struct PlayerRenderer {
    Component base;
    DeckState *DeckA;
    DeckState *DeckB;
    BeatFXState *FXState;
    
    DeckInfoPanel InfoA;
    DeckInfoPanel InfoB;
    DeckStrip StripA;
    DeckStrip StripB;
    WaveformRenderer WaveA;
    WaveformRenderer WaveB;
    BeatFXPanel BeatFX;
    BeatFXSelectBar FXBar;
};

void PlayerRenderer_Init(PlayerRenderer *r, DeckState *a, DeckState *b, BeatFXState *fx);
