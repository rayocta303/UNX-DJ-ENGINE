#ifndef MIDI_MAPPER_H
#define MIDI_MAPPER_H

#include <stdint.h>
#include <stdbool.h>

#define MIDI_OPT_RELATIVE   (1 << 0)  // 0x01: <selectknob/>
#define MIDI_OPT_SCRIPT     (1 << 2)  // 0x04: <script-binding/>
#define MIDI_OPT_14BIT_MSB  (1 << 3)  // 0x08: <fourteen-bit-msb/>
#define MIDI_OPT_14BIT_LSB  (1 << 4)  // 0x10: <fourteen-bit-lsb/>
#define MIDI_OPT_INVERT     (1 << 5)  // 0x20: <invert/>
#define MIDI_OPT_BUTTON     (1 << 6)  // 0x40: <button/>
#define MIDI_OPT_SWITCH     (1 << 7)  // 0x80: <switch/>
#define MIDI_OPT_ROT64      (1 << 8)  // 0x100: <rot64/>
#define MIDI_OPT_DIFF       (1 << 9)  // 0x200: <diff/>
#define MIDI_OPT_ROT64INV   (1 << 10) // 0x400: <rot64inv/>
#define MIDI_OPT_ROT64FAST  (1 << 11) // 0x800: <rot64fast/>
#define MIDI_OPT_HERCJOG    (1 << 12) // 0x1000: <hercjog/>
#define MIDI_OPT_HERCJOGFAST (1 << 13) // 0x2000: <hercjogfast/>
#define MIDI_OPT_SPREAD64   (1 << 14) // 0x4000: <spread64/>

typedef enum {
    SCRIPT_ACTION_UNKNOWN = 0,
    SCRIPT_ACTION_SHIFT,
    SCRIPT_ACTION_JOG_TURN,
    SCRIPT_ACTION_JOG_SEARCH,
    SCRIPT_ACTION_JOG_TOUCH,
    SCRIPT_ACTION_BEAT_TAP,
    SCRIPT_ACTION_BEATFX_NEXT,
    SCRIPT_ACTION_BEATFX_PREV,
    SCRIPT_ACTION_BEATFX_DEPTH,
    SCRIPT_ACTION_BEATFX_TOGGLE,
    SCRIPT_ACTION_PAD_MODE,
    SCRIPT_ACTION_SAMPLER_PAD,
    SCRIPT_ACTION_LOOP_IN_ADJUST,
    SCRIPT_ACTION_LOOP_OUT_ADJUST,
    SCRIPT_ACTION_CUE_LOOP_LEFT,
    SCRIPT_ACTION_CUE_LOOP_RIGHT,
    SCRIPT_ACTION_TEMPO_MSB,
    SCRIPT_ACTION_TEMPO_LSB,
    SCRIPT_ACTION_TEMPO_RANGE,
    SCRIPT_ACTION_SYNC,
    SCRIPT_ACTION_QUANTIZE,
    SCRIPT_ACTION_SLIP,
    SCRIPT_ACTION_MERGE_FX_TURN,
    SCRIPT_ACTION_MERGE_FX_PRESS,
    SCRIPT_ACTION_LOAD_TRACK,
    SCRIPT_ACTION_BROWSE_CLICK,
    SCRIPT_ACTION_BROWSE_TOGGLE,
    SCRIPT_ACTION_HEAD_MIX,
    SCRIPT_ACTION_BEATJUMP_PAD,
    SCRIPT_ACTION_BEATJUMP_DEC,
    SCRIPT_ACTION_BEATJUMP_INC,
    SCRIPT_ACTION_DECK_CONTROL_L,
    SCRIPT_ACTION_DECK_CONTROL_R,
    SCRIPT_ACTION_KEYBOARD_BTN,
    SCRIPT_ACTION_BROWSE_SCROLL
} MidiScriptAction;

typedef struct {
    char group[64];
    char key[64];
    uint8_t status;
    uint8_t midino;
    uint32_t options;
    char scriptFunction[128]; // For Script-Binding
    int scriptActionId;       // Fast O(1) enum for Script-Binding

    void *cachedCO;           // Pointer to ControlObject for fast O(1) access
} MappingEntry;

typedef struct {
    char group[64];
    char key[64];
    uint8_t status;
    uint8_t midino;
    uint8_t on;
    uint8_t off;
    float minimum;
    float maximum;
    int lastSentVal;
} MidiOutputEntry;

typedef struct {
    char name[128];
    char author[128];
    char description[256];
    char scriptFiles[8][128];
    int scriptCount;
    bool modifiers[16];        // Modifier states (e.g. Shift, DeckLayer)
    MappingEntry entries[2048]; // Capacity for full controller mapping
    int count;
    MidiOutputEntry outputs[1024];
    int outputCount;
    int lookupTable[256][128]; // O(1) fast lookup table [status][midino] -> index in entries
} MidiMapping;

bool MIDI_LoadMapping(MidiMapping *map, const char *path);
bool MIDI_ScanControllers(const char *dir, const char *deviceName, MidiMapping *out);
int MIDI_ListControllers(const char *dir, char outNames[32][64], char outPaths[32][256]);
void MIDI_HandleMapping(MidiMapping *map, uint8_t status, uint8_t midino, float normalizedValue);
bool MIDI_SaveMapping(MidiMapping *map, const char *path);
void MIDI_CreateTemplate(MidiMapping *out);
bool MIDI_GetRegisterAddress(const MidiMapping *map, const char *group, const char *keySubstr, uint8_t *outStatus, uint8_t *outMidino);

#endif // MIDI_MAPPER_H
