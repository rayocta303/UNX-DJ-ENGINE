#ifndef MIDI_MAPPER_H
#define MIDI_MAPPER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t status;
    uint8_t midino;
    char group[32];
    char key[64];
} MappingEntry;

typedef struct {
    char name[128]; // Mapped device name
    MappingEntry entries[256];
    int count;
} MidiMapping;

bool MIDI_LoadMapping(MidiMapping *map, const char *path);
bool MIDI_ScanControllers(const char *dir, const char *deviceName, MidiMapping *out);
void MIDI_HandleMapping(MidiMapping *map, uint8_t status, uint8_t midino, float normalizedValue);

#endif
