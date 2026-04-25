#include "core/midi/midi_translation.h"
#include <string.h>

static const MidiTranslation translations[] = {
    {"[Library]", "MoveVertical", "[Library]", "browse"},
    {"[Library]", "MoveFocusForward", "[Library]", "enter"},
    {"[Library]", "MoveFocusBackward", "[Library]", "back"},
    {"[Channel1]", "LoadSelectedTrack", "[Library]", "loadA"},
    {"[Channel2]", "LoadSelectedTrack", "[Library]", "loadB"},
    
    // EQ mapping
    {"[EqualizerRack1_[Channel1]_Effect1]", "parameter3", "[Channel1]", "filterHigh"},
    {"[EqualizerRack1_[Channel1]_Effect1]", "parameter2", "[Channel1]", "filterMid"},
    {"[EqualizerRack1_[Channel1]_Effect1]", "parameter1", "[Channel1]", "filterLow"},
    
    {"[EqualizerRack1_[Channel2]_Effect1]", "parameter3", "[Channel2]", "filterHigh"},
    {"[EqualizerRack1_[Channel2]_Effect1]", "parameter2", "[Channel2]", "filterMid"},
    {"[EqualizerRack1_[Channel2]_Effect1]", "parameter1", "[Channel2]", "filterLow"},

    // Gain
    {"[Channel1]", "pregain", "[Channel1]", "volume"},
    {"[Channel2]", "pregain", "[Channel2]", "volume"},

    {NULL, NULL, NULL, NULL}
};

const MidiTranslation* MIDI_GetTranslation(const char *group, const char *key) {
    for (int i = 0; translations[i].srcGroup != NULL; i++) {
        if (strcmp(group, translations[i].srcGroup) == 0 && 
            strcmp(key, translations[i].srcKey) == 0) {
            return &translations[i];
        }
    }
    return NULL;
}
