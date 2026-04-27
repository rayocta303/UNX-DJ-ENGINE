#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include "core/midi/midi_message.h"
#include "core/midi/midi_mapper.h"
#include "audio/engine.h"
#include "ui/player/player.h" // For DeckState etc

typedef struct {
    bool initialized;
    int deviceHandle; // Used by backend
    // Optional: Multiple ports/devices if needed
} MidiContext;

/**
 * Initializes the MIDI subsystem (platform specific)
 */
bool MIDI_Init(MidiContext *ctx);

/**
 * Shuts down the MIDI subsystem
 */
void MIDI_Close(MidiContext *ctx);

/**
 * Checks for new MIDI messages and applies them to the engine
 */
void MIDI_Update(MidiContext *ctx, DeckState *d1, DeckState *d2, AudioEngine *engine);
MidiMapping* MIDI_GetGlobalMapping(void);
void MIDI_RefreshMapping(const char *path);
bool MIDI_GetLastMessage(uint8_t *status, uint8_t *midino);
bool MIDI_PeekLastMessage(uint8_t *status, uint8_t *midino);
bool MIDI_SaveCurrentMapping(const char *name);

#endif // MIDI_HANDLER_H
