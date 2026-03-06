#pragma once
#include "ui/player/player_state.h"
#include "audio/engine.h"

#ifdef __cplusplus
extern "C" {
#endif

void Sync_Update(DeckState *deckA, DeckState *deckB, AudioEngine *audioEngine);
void DeckState_SetHardwarePitch(DeckState *deck, float hardwarePercent);

#ifdef __cplusplus
}
#endif
