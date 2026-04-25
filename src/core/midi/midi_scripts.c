#include "core/midi/midi_scripts.h"
#include "core/logic/control_object.h"
#include <string.h>
#include <stdio.h>



void MIDI_ExecuteScript(MidiMapping *map, const char *function, uint8_t status, uint8_t midino, uint8_t value) {
    int deck = (status & 0x0F);
    const char *group = (deck == 0) ? "[Channel1]" : "[Channel2]";
    
    if (strstr(function, "shiftButton") || strstr(function, "shiftPressed")) {
        map->modifiers[0] = (value > 0); // Modifier 0 is Shift
        printf("[MIDI-SCRIPT] Shift %d: %s\n", deck + 1, map->modifiers[0] ? "ON" : "OFF");
    } 
    else if (strstr(function, "jogTurn")) {
        float delta = (float)(value - 64);
        if (map->modifiers[0]) {
            // Fast seek if shift held
            CO_AddValue(group, "jog", delta * 5.0f);
        } else {
            CO_AddValue(group, "jog", delta);
        }
    }
    else if (strstr(function, "jogTouch")) {
        // value > 0 is touch
        printf("[MIDI-SCRIPT] Jog %d Touch: %d\n", deck + 1, value);
    }
    else if (strstr(function, "loadSelectedTrack")) {
        CO_SetValue("[Library]", deck == 0 ? "loadA" : "loadB", 1.0f);
    }
}
