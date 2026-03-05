#pragma once
#include "raylib.h"
#include "ui/player/player_state.h"

typedef struct {
    KeyboardKey playPause1;
    KeyboardKey cue1;
    KeyboardKey hotCues1[5];

    KeyboardKey playPause2;
    KeyboardKey cue2;
    KeyboardKey hotCues2[5];

    KeyboardKey toggleBrowser;
    KeyboardKey toggleInfo;
    KeyboardKey toggleSettings;
    KeyboardKey back;
} KeyboardMapping;

#include "audio/engine.h"

KeyboardMapping GetDefaultMapping();
void HandleKeyboardInputs(KeyboardMapping *m, DeckState *d1, DeckState *d2, AudioEngine *engine);
