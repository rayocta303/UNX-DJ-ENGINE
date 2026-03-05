#pragma once
#include "ui/components/component.h"
#include "ui/player/player_state.h"
#include "ui/player/deckinfo.h"
#include "ui/player/deckstrip.h"
#include "ui/player/beatfx.h"
#include "ui/player/waveform.h"
#include "ui/player/bottomstrip.h"
#include "audio/engine.h"

typedef struct PlayerRenderer PlayerRenderer;

struct PlayerRenderer {
    Component base;
    DeckState *DeckA;
    DeckState *DeckB;
    BeatFXState *FXState;
    AudioEngine *AudioPlugin;
    
    DeckInfoPanel InfoA;
    DeckInfoPanel InfoB;
    WaveformRenderer WaveA;
    WaveformRenderer WaveB;
    BeatFXPanel BeatFX;
    BeatFXSelectBar FXBar;
};

void PlayerRenderer_Init(PlayerRenderer *r, DeckState *a, DeckState *b, BeatFXState *fx, AudioEngine *audioPlugin);
