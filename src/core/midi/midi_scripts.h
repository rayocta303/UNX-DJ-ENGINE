#ifndef MIDI_SCRIPTS_H
#define MIDI_SCRIPTS_H

#include <stdint.h>

#include "core/midi/midi_mapper.h"

void MIDI_ExecuteScript(MidiMapping *map, const char *function, uint8_t status, uint8_t midino, uint8_t value);

#endif
