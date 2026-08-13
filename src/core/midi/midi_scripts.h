#ifndef MIDI_SCRIPTS_H
#define MIDI_SCRIPTS_H

#include <stdint.h>
#include <stdbool.h>

#include "core/midi/midi_mapper.h"
#include "audio/engine.h"

typedef struct DeckState DeckState;

void MIDI_ExecuteScript(MidiMapping *map, const char *function, uint8_t status, uint8_t midino, uint8_t value);

/**
 * Updates MIDI OUT VU meters for hardware controllers (Pioneer DDJ-FLX4 / FLX6 style).
 * Reads audio peak levels from AudioEngine and dispatches CC messages (0xB0..0xB3, CC 0x02).
 */
void MIDI_UpdateVuMeters(AudioEngine *engine, bool forceSend);

/**
 * Updates MIDI OUT Loop Button LEDs, HotCue Pad LEDs, and Play/Cue/Sync status LEDs for hardware controllers.
 */
void MIDI_UpdateLoopAndPadLEDs(DeckState *d1, DeckState *d2, AudioEngine *engine, bool forceSend);

/**
 * Resets all MIDI OUT VU meters to 0 (off state).
 */
void MIDI_ResetVuMeters(void);

/**
 * Resets all hardware LEDs (VU meters, Loop LEDs, Pad LEDs, Play/Cue/Sync) to off state.
 */
void MIDI_ResetAllLEDs(void);

/**
 * Sets the active MidiMapping reference so that LED register addresses
 * (status byte and CC/midino) are resolved from the loaded mapping file
 * instead of being hardcoded. Call this once after MIDI_LoadMapping.
 *
 * Falls back to Pioneer DDJ-FLX6 defaults if a key is not found in the mapping.
 */
void MIDI_SetMappingRef(const MidiMapping *map);

#endif


