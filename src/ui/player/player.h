#pragma once
#include "audio/engine.h"
#include "ui/components/component.h"
#include "ui/player/beatfx.h"
#include "ui/player/bottomstrip.h"
#include "ui/player/deckinfo.h"
#include "ui/player/deckstrip.h"
#include "ui/player/player_state.h"
#include "ui/player/waveform.h"

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
  Texture2D Logo;
};

void PlayerRenderer_Init(PlayerRenderer *r, DeckState *a, DeckState *b,
                         BeatFXState *fx, AudioEngine *audioPlugin);
