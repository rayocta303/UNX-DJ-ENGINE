#include "input/keyboard.h"
#include "raylib.h"

#include "logic/quantize.h"
#include "logic/sync.h"

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
    m.toggleMixer = KEY_M;
    m.back = KEY_ESCAPE;
    
    return m;
}

void HandleKeyboardInputs(KeyboardMapping *m, DeckState *d1, DeckState *d2, AudioEngine *engine) {
    // Deck 1
    if (IsKeyPressed(m->playPause1)) {
        if (engine) {
            bool starting = !engine->Decks[0].IsMotorOn;
            DeckAudio_SetPlaying(&engine->Decks[0], starting);
            if (starting) {
                if (d1->SyncMode == 2 && !d1->IsMaster) {
                    Sync_RequestPhaseSnap(d1, d2, engine);
                }
            } else {
                // Revert to BPM sync on stop to avoid snapping during motor brake
                if (d1->SyncMode == 2) d1->SyncMode = 1;
            }
        }
    }
    for (int i = 0; i < 5; i++) {
        if (IsKeyPressed(m->hotCues1[i])) {
            int targetID = i + 1; // 1, 2, 3, 4, 5
            if (d1->LoadedTrack) {
                for (int j = 0; j < d1->LoadedTrack->HotCuesCount; j++) {
                    if (d1->LoadedTrack->HotCues[j].ID == (unsigned int)targetID) {
                        uint32_t targetMs = d1->LoadedTrack->HotCues[j].Start;
                        if (d1->QuantizeEnabled) {
                            int32_t waitMs = Quantize_GetWaitMs(d1->LoadedTrack, d1->PositionMs);
                            DeckAudio_QueueJumpMs(&engine->Decks[0], targetMs, (uint32_t)waitMs);
                        } else {
                            DeckAudio_JumpToMs(&engine->Decks[0], targetMs);
                            DeckAudio_InstantPlay(&engine->Decks[0]);
                        }
                        break;
                    }
                }
            }
        }
    }

    // Deck 2
    if (IsKeyPressed(m->playPause2)) {
        if (engine) {
            bool starting = !engine->Decks[1].IsMotorOn;
            DeckAudio_SetPlaying(&engine->Decks[1], starting);
            if (starting) {
                if (d2->SyncMode == 2 && !d2->IsMaster) {
                    Sync_RequestPhaseSnap(d2, d1, engine);
                }
            } else {
                // Revert to BPM sync on stop to avoid snapping during motor brake
                if (d2->SyncMode == 2) d2->SyncMode = 1;
            }
        }
    }
    for (int i = 0; i < 5; i++) {
        if (IsKeyPressed(m->hotCues2[i])) {
            int targetID = i + 1; // Mapping 6,7,8,9,0 keys to HotCues 1,2,3,4,5
            if (d2->LoadedTrack) {
                for (int j = 0; j < d2->LoadedTrack->HotCuesCount; j++) {
                    if (d2->LoadedTrack->HotCues[j].ID == (unsigned int)targetID) {
                        uint32_t targetMs = d2->LoadedTrack->HotCues[j].Start;
                        if (d2->QuantizeEnabled) {
                            int32_t waitMs = Quantize_GetWaitMs(d2->LoadedTrack, d2->PositionMs);
                            DeckAudio_QueueJumpMs(&engine->Decks[1], targetMs, (uint32_t)waitMs);
                        } else {
                            DeckAudio_JumpToMs(&engine->Decks[1], targetMs);
                            DeckAudio_InstantPlay(&engine->Decks[1]);
                        }
                        break;
                    }
                }
            }
        }
    }
}
