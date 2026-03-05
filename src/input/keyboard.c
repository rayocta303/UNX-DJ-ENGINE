#include "input/keyboard.h"
#include "raylib.h"

KeyboardMapping GetDefaultMapping() {
    KeyboardMapping m = {0};
    m.playPause1 = KEY_Z;
    m.cue1 = KEY_X;
    m.hotCues1[0] = KEY_ONE;
    m.hotCues1[1] = KEY_TWO;
    m.hotCues1[2] = KEY_THREE;
    m.hotCues1[3] = KEY_FOUR;
    m.hotCues1[4] = KEY_FIVE;

    m.playPause2 = KEY_N;
    m.cue2 = KEY_M;
    m.hotCues2[0] = KEY_SIX;
    m.hotCues2[1] = KEY_SEVEN;
    m.hotCues2[2] = KEY_EIGHT;
    m.hotCues2[3] = KEY_NINE;
    m.hotCues2[4] = KEY_ZERO;

    m.toggleBrowser = KEY_SPACE;
    m.toggleInfo = KEY_I;
    m.toggleSettings = KEY_TAB;
    m.back = KEY_ESCAPE;
    
    return m;
}

void HandleKeyboardInputs(KeyboardMapping *m, DeckState *d1, DeckState *d2, AudioEngine *engine) {
    // Deck 1
    if (IsKeyPressed(m->playPause1)) {
        if (engine) engine->Decks[0].IsPlaying = !engine->Decks[0].IsPlaying;
    }
    for (int i = 0; i < 5; i++) {
        if (IsKeyPressed(m->hotCues1[i])) {
            // Handle hot cue mock
        }
    }

    // Deck 2
    if (IsKeyPressed(m->playPause2)) {
        if (engine) engine->Decks[1].IsPlaying = !engine->Decks[1].IsPlaying;
    }
    for (int i = 0; i < 5; i++) {
        if (IsKeyPressed(m->hotCues2[i])) {
            // Handle hot cue mock
        }
    }
}
