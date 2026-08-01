#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include "core/midi/midi_message.h"
#include "core/midi/midi_mapper.h"
#include "audio/engine.h"
#include "ui/player/player.h" // For DeckState etc

typedef struct {
    bool initialized;
    int deviceHandle; // Used by backend
    int currentDevId;
    char activeDeviceName[128];
} MidiContext;

/**
 * Initializes the MIDI subsystem (platform specific)
 */
bool MIDI_Init(MidiContext *ctx);

/**
 * Enumerates connected physical MIDI devices
 */
int MIDI_GetDeviceList(char outNames[16][64]);

/**
 * Connects to a specific MIDI device index and auto-loads matching XML mapping preset if available
 */
bool MIDI_SelectDevice(MidiContext *ctx, int deviceIndex, char *outDeviceName, char *outPresetPath);

/**
 * Shuts down the MIDI subsystem
 */
void MIDI_Close(MidiContext *ctx);

/**
 * Checks for new MIDI messages and applies them to the engine
 */
void MIDI_Update(MidiContext *ctx, DeckState *d1, DeckState *d2, AudioEngine *engine);
void MIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2);
void MIDI_UpdateLEDs(MidiContext *ctx, DeckState *d1, DeckState *d2, AudioEngine *engine, void *appPtr);
MidiMapping* MIDI_GetGlobalMapping(void);
void MIDI_RefreshMapping(const char *path);
bool MIDI_GetLastMessage(uint8_t *status, uint8_t *midino);
bool MIDI_PeekLastMessage(uint8_t *status, uint8_t *midino);
void MIDI_CheckHotplug(MidiContext *ctx);
bool MIDI_SaveCurrentMapping(const char *name);

#endif // MIDI_HANDLER_H

