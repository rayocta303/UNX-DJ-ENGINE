#ifndef MIDI_LED_HELPER_H
#define MIDI_LED_HELPER_H

#include <stdbool.h>
#include <stdint.h>
#include "audio/engine.h"
#include "ui/player/player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for MIDI OUT functions
void MIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2);
void MIDI_SendSysEx(const uint8_t *data, uint32_t length);

typedef struct {
    uint8_t playStatus;
    uint8_t playNote;
    uint8_t cueStatus;
    uint8_t cueNote;
    uint8_t vinylStatus;
    uint8_t vinylNote;
    uint8_t loopInStatus;
    uint8_t loopInNote;
    uint8_t loopOutStatus;
    uint8_t loopOutNote;
    uint8_t reloopStatus;
    uint8_t reloopNote;
    uint8_t vuStatus;
    uint8_t vuControl;
} MidiDeckAddresses;

typedef struct {
    uint8_t lastPlay[4];
    uint8_t lastCue[4];
    uint8_t lastVinyl[4];
    uint8_t lastVu[4];
    uint8_t lastJog[4];
    uint8_t lastHotCue[4][8];
    uint8_t lastMasterL;
    uint8_t lastMasterR;
    uint8_t lastFxOn;
    
    MidiDeckAddresses deckAddr[4];
    const void *resolvedMapping;

    double lastSendTime;
    double lastFullRefresh;
    double lastBlinkTime;
    double lastSysExTime;
    bool blinkState;
    bool connectionHandshakeDone;
    void *lastLoadedTrack[4];
} MidiLedCache;

/**
 * Reset LED state cache (forces full refresh on next update)
 */
void MidiLed_Reset(MidiLedCache *cache);

/**
 * Focus helper: Update Channel 1-4 and Master L/R VU Meters exclusively
 */
void MidiLed_UpdateVUMeters(MidiLedCache *cache, DeckState *d1, DeckState *d2, AudioEngine *engine, double nowTime, bool forceRefresh);

/**
 * Calculate 72-position Jog Wheel Ring rotation (Pioneer 0x01..0x48)
 * Matches Mixxx playPositionUpdate formula: (posSec * 72 * 0.6075) % 72 + 1
 */
uint8_t MidiLed_CalcJogPosition(double posSec);

/**
 * Calculate VU meter LED output (0 to 127) accounting for Trim/Fader/Cue and playback state
 */
uint8_t MidiLed_CalcVuLevel(float peakL, float peakR, float fader, bool isCueActive);

/**
 * Non-blocking LED update loop for Pioneer DDJ controllers
 * Uses differential caching (only sends MIDI when state changes) and ~60 FPS rate-limiting.
 */
void MidiLed_Update(MidiLedCache *cache, DeckState *d1, DeckState *d2, AudioEngine *engine, double nowTime);

#ifdef __cplusplus
}
#endif

#endif // MIDI_LED_HELPER_H
