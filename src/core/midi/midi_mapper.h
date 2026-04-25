#ifndef MIDI_MAPPER_H
#define MIDI_MAPPER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char group[64];
    char key[64];
    uint8_t status;
    uint8_t midino;
    uint32_t options;
    char scriptFunction[128]; // For Script-Binding
} MappingEntry;

typedef struct {
    char name[128];
    char author[128];
    char description[256];
    char scriptFiles[8][128];
    int scriptCount;
    bool modifiers[16]; // Modifier states (e.g. Shift, DeckLayer)
    MappingEntry entries[512]; // Increased capacity
    int count;
} MidiMapping;

bool MIDI_LoadMapping(MidiMapping *map, const char *path);
bool MIDI_ScanControllers(const char *dir, const char *deviceName, MidiMapping *out);
int MIDI_ListControllers(const char *dir, char outNames[32][64], char outPaths[32][256]);
void MIDI_HandleMapping(MidiMapping *map, uint8_t status, uint8_t midino, float normalizedValue);

#endif
