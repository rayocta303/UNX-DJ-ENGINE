#include "core/midi/midi_translation.h"
#include <string.h>

static const MidiTranslation translations[] = {
    {"[Library]", "MoveVertical", "[Library]", "browse"},
    {"[Library]", "MoveFocusForward", "[Library]", "enter"},
    {"[Library]", "MoveFocusBackward", "[Library]", "back"},
    {"[Channel1]", "LoadSelectedTrack", "[Library]", "loadA"},
    {"[Channel2]", "LoadSelectedTrack", "[Library]", "loadB"},
    {"[Channel3]", "LoadSelectedTrack", "[Library]", "loadA"},
    {"[Channel4]", "LoadSelectedTrack", "[Library]", "loadB"},
    
    // EQ mapping
    {"[EqualizerRack1_[Channel1]_Effect1]", "parameter3", "[Channel1]", "filterHigh"},
    {"[EqualizerRack1_[Channel1]_Effect1]", "parameter2", "[Channel1]", "filterMid"},
    {"[EqualizerRack1_[Channel1]_Effect1]", "parameter1", "[Channel1]", "filterLow"},
    
    {"[EqualizerRack1_[Channel2]_Effect1]", "parameter3", "[Channel2]", "filterHigh"},
    {"[EqualizerRack1_[Channel2]_Effect1]", "parameter2", "[Channel2]", "filterMid"},
    {"[EqualizerRack1_[Channel2]_Effect1]", "parameter1", "[Channel2]", "filterLow"},

    {"[EqualizerRack1_[Channel3]_Effect1]", "parameter3", "[Channel1]", "filterHigh"},
    {"[EqualizerRack1_[Channel3]_Effect1]", "parameter2", "[Channel1]", "filterMid"},
    {"[EqualizerRack1_[Channel3]_Effect1]", "parameter1", "[Channel1]", "filterLow"},

    {"[EqualizerRack1_[Channel4]_Effect1]", "parameter3", "[Channel2]", "filterHigh"},
    {"[EqualizerRack1_[Channel4]_Effect1]", "parameter2", "[Channel2]", "filterMid"},
    {"[EqualizerRack1_[Channel4]_Effect1]", "parameter1", "[Channel2]", "filterLow"},

    // Gain
    {"[Channel1]", "pregain", "[Channel1]", "volume"},
    {"[Channel2]", "pregain", "[Channel2]", "volume"},
    {"[Channel3]", "pregain", "[Channel1]", "volume"},
    {"[Channel4]", "pregain", "[Channel2]", "volume"},

    // Volume Fader / Trim redirect
    {"[Channel1]", "volume", "[Channel1]", "fader"},
    {"[Channel2]", "volume", "[Channel2]", "fader"},
    {"[Channel3]", "volume", "[Channel1]", "fader"},
    {"[Channel4]", "volume", "[Channel2]", "fader"},

    // Color FX / Filter
    {"[QuickEffectRack1_[Channel1]]", "super1", "[Channel1]", "colorfx_value"},
    {"[QuickEffectRack1_[Channel2]]", "super1", "[Channel2]", "colorfx_value"},
    {"[QuickEffectRack1_[Channel3]]", "super1", "[Channel3]", "colorfx_value"},
    {"[QuickEffectRack1_[Channel4]]", "super1", "[Channel4]", "colorfx_value"},

    // Loop Reloop / Exit
    {"[Channel1]", "reloop_toggle", "[Channel1]", "loop_exit"},
    {"[Channel2]", "reloop_toggle", "[Channel2]", "loop_exit"},
    {"[Channel3]", "reloop_toggle", "[Channel3]", "loop_exit"},
    {"[Channel4]", "reloop_toggle", "[Channel4]", "loop_exit"},

    {"[Channel3]", "play", "[Channel1]", "play"},
    {"[Channel4]", "play", "[Channel2]", "play"},
    {"[Channel3]", "cue", "[Channel1]", "cue"},
    {"[Channel4]", "cue", "[Channel2]", "cue"},
    {"[Channel3]", "cue_default", "[Channel1]", "cue_default"},
    {"[Channel4]", "cue_default", "[Channel2]", "cue_default"},
    {"[Channel3]", "jog", "[Channel1]", "jog"},
    {"[Channel4]", "jog", "[Channel2]", "jog"},
    {"[Channel3]", "touch", "[Channel1]", "touch"},
    {"[Channel4]", "touch", "[Channel2]", "touch"},
    {"[Channel3]", "slip", "[Channel1]", "slip"},
    {"[Channel4]", "slip", "[Channel2]", "slip"},
    {"[Channel3]", "vinyl_mode", "[Channel1]", "vinyl_mode"},
    {"[Channel4]", "vinyl_mode", "[Channel2]", "vinyl_mode"},
    {"[Channel1]", "sync_enabled", "[Channel1]", "sync"},
    {"[Channel2]", "sync_enabled", "[Channel2]", "sync"},
    {"[Channel3]", "sync_enabled", "[Channel1]", "sync"},
    {"[Channel4]", "sync_enabled", "[Channel2]", "sync"},
    {"[Channel3]", "sync", "[Channel1]", "sync"},
    {"[Channel4]", "sync", "[Channel2]", "sync"},
    {"[Channel3]", "loop_in", "[Channel1]", "loop_in"},
    {"[Channel4]", "loop_in", "[Channel2]", "loop_in"},
    {"[Channel3]", "loop_out", "[Channel1]", "loop_out"},
    {"[Channel4]", "loop_out", "[Channel2]", "loop_out"},

    // Beat FX Dry/Wet
    {"[EffectRack1_EffectUnit1_Effect1]", "meta", "[Master]", "beatfx_drywet"},
    {"[EffectRack1_EffectUnit2_Effect1]", "meta", "[Master]", "beatfx_drywet"},

    // Hot Cues

    {"[Channel1]", "hotcue_1_activate", "[Channel1]", "hotcue_1"},
    {"[Channel1]", "hotcue_2_activate", "[Channel1]", "hotcue_2"},
    {"[Channel1]", "hotcue_3_activate", "[Channel1]", "hotcue_3"},
    {"[Channel1]", "hotcue_4_activate", "[Channel1]", "hotcue_4"},
    {"[Channel1]", "hotcue_5_activate", "[Channel1]", "hotcue_5"},
    {"[Channel1]", "hotcue_6_activate", "[Channel1]", "hotcue_6"},
    {"[Channel1]", "hotcue_7_activate", "[Channel1]", "hotcue_7"},
    {"[Channel1]", "hotcue_8_activate", "[Channel1]", "hotcue_8"},

    {"[Channel2]", "hotcue_1_activate", "[Channel2]", "hotcue_1"},
    {"[Channel2]", "hotcue_2_activate", "[Channel2]", "hotcue_2"},
    {"[Channel2]", "hotcue_3_activate", "[Channel2]", "hotcue_3"},
    {"[Channel2]", "hotcue_4_activate", "[Channel2]", "hotcue_4"},
    {"[Channel2]", "hotcue_5_activate", "[Channel2]", "hotcue_5"},
    {"[Channel2]", "hotcue_6_activate", "[Channel2]", "hotcue_6"},
    {"[Channel2]", "hotcue_7_activate", "[Channel2]", "hotcue_7"},
    {"[Channel2]", "hotcue_8_activate", "[Channel2]", "hotcue_8"},

    {"[Channel3]", "hotcue_1_activate", "[Channel1]", "hotcue_1"},
    {"[Channel3]", "hotcue_2_activate", "[Channel1]", "hotcue_2"},
    {"[Channel3]", "hotcue_3_activate", "[Channel1]", "hotcue_3"},
    {"[Channel3]", "hotcue_4_activate", "[Channel1]", "hotcue_4"},
    {"[Channel3]", "hotcue_5_activate", "[Channel1]", "hotcue_5"},
    {"[Channel3]", "hotcue_6_activate", "[Channel1]", "hotcue_6"},
    {"[Channel3]", "hotcue_7_activate", "[Channel1]", "hotcue_7"},
    {"[Channel3]", "hotcue_8_activate", "[Channel1]", "hotcue_8"},

    {"[Channel4]", "hotcue_1_activate", "[Channel2]", "hotcue_1"},
    {"[Channel4]", "hotcue_2_activate", "[Channel2]", "hotcue_2"},
    {"[Channel4]", "hotcue_3_activate", "[Channel2]", "hotcue_3"},
    {"[Channel4]", "hotcue_4_activate", "[Channel2]", "hotcue_4"},
    {"[Channel4]", "hotcue_5_activate", "[Channel2]", "hotcue_5"},
    {"[Channel4]", "hotcue_6_activate", "[Channel2]", "hotcue_6"},
    {"[Channel4]", "hotcue_7_activate", "[Channel2]", "hotcue_7"},
    {"[Channel4]", "hotcue_8_activate", "[Channel2]", "hotcue_8"},

    {"[Channel1]", "hotcue_1_set", "[Channel1]", "hotcue_1"},
    {"[Channel1]", "hotcue_2_set", "[Channel1]", "hotcue_2"},
    {"[Channel1]", "hotcue_3_set", "[Channel1]", "hotcue_3"},
    {"[Channel1]", "hotcue_4_set", "[Channel1]", "hotcue_4"},
    {"[Channel1]", "hotcue_5_set", "[Channel1]", "hotcue_5"},
    {"[Channel1]", "hotcue_6_set", "[Channel1]", "hotcue_6"},
    {"[Channel1]", "hotcue_7_set", "[Channel1]", "hotcue_7"},
    {"[Channel1]", "hotcue_8_set", "[Channel1]", "hotcue_8"},

    {"[Channel2]", "hotcue_1_set", "[Channel2]", "hotcue_1"},
    {"[Channel2]", "hotcue_2_set", "[Channel2]", "hotcue_2"},
    {"[Channel2]", "hotcue_3_set", "[Channel2]", "hotcue_3"},
    {"[Channel2]", "hotcue_4_set", "[Channel2]", "hotcue_4"},
    {"[Channel2]", "hotcue_5_set", "[Channel2]", "hotcue_5"},
    {"[Channel2]", "hotcue_6_set", "[Channel2]", "hotcue_6"},
    {"[Channel2]", "hotcue_7_set", "[Channel2]", "hotcue_7"},
    {"[Channel2]", "hotcue_8_set", "[Channel2]", "hotcue_8"},

    {"[Channel3]", "hotcue_1_set", "[Channel1]", "hotcue_1"},
    {"[Channel3]", "hotcue_2_set", "[Channel1]", "hotcue_2"},
    {"[Channel3]", "hotcue_3_set", "[Channel1]", "hotcue_3"},
    {"[Channel3]", "hotcue_4_set", "[Channel1]", "hotcue_4"},
    {"[Channel3]", "hotcue_5_set", "[Channel1]", "hotcue_5"},
    {"[Channel3]", "hotcue_6_set", "[Channel1]", "hotcue_6"},
    {"[Channel3]", "hotcue_7_set", "[Channel1]", "hotcue_7"},
    {"[Channel3]", "hotcue_8_set", "[Channel1]", "hotcue_8"},

    {"[Channel4]", "hotcue_1_set", "[Channel2]", "hotcue_1"},
    {"[Channel4]", "hotcue_2_set", "[Channel2]", "hotcue_2"},
    {"[Channel4]", "hotcue_3_set", "[Channel2]", "hotcue_3"},
    {"[Channel4]", "hotcue_4_set", "[Channel2]", "hotcue_4"},
    {"[Channel4]", "hotcue_5_set", "[Channel2]", "hotcue_5"},
    {"[Channel4]", "hotcue_6_set", "[Channel2]", "hotcue_6"},
    {"[Channel4]", "hotcue_7_set", "[Channel2]", "hotcue_7"},
    {"[Channel4]", "hotcue_8_set", "[Channel2]", "hotcue_8"},

    // Pitch Fader (rate)
    {"[Channel1]", "rate", "[Channel1]", "tempo_percent"},
    {"[Channel2]", "rate", "[Channel2]", "tempo_percent"},
    {"[Channel3]", "rate", "[Channel1]", "tempo_percent"},
    {"[Channel4]", "rate", "[Channel2]", "tempo_percent"},

    // Quantize & Keylock/MasterTempo
    {"[Channel1]", "keylock", "[Channel1]", "master_tempo"},
    {"[Channel2]", "keylock", "[Channel2]", "master_tempo"},
    {"[Channel3]", "keylock", "[Channel1]", "master_tempo"},
    {"[Channel4]", "keylock", "[Channel2]", "master_tempo"},
    {"[Channel1]", "slip_enabled", "[Channel1]", "slip"},
    {"[Channel2]", "slip_enabled", "[Channel2]", "slip"},
    {"[Channel3]", "slip_enabled", "[Channel1]", "slip"},
    {"[Channel4]", "slip_enabled", "[Channel2]", "slip"},

    // Beat Loops
    {"[Channel1]", "beatloop_1_activate", "[Channel1]", "autoloop_1"},
    {"[Channel1]", "beatloop_2_activate", "[Channel1]", "autoloop_2"},
    {"[Channel1]", "beatloop_4_activate", "[Channel1]", "autoloop_4"},
    {"[Channel1]", "beatloop_8_activate", "[Channel1]", "autoloop_8"},
    {"[Channel1]", "beatloop_16_activate", "[Channel1]", "autoloop_16"},

    {"[Channel2]", "beatloop_1_activate", "[Channel2]", "autoloop_1"},
    {"[Channel2]", "beatloop_2_activate", "[Channel2]", "autoloop_2"},
    {"[Channel2]", "beatloop_4_activate", "[Channel2]", "autoloop_4"},
    {"[Channel2]", "beatloop_8_activate", "[Channel2]", "autoloop_8"},
    {"[Channel2]", "beatloop_16_activate", "[Channel2]", "autoloop_16"},

    {"[Channel1]", "beatloop_1_toggle", "[Channel1]", "autoloop_1"},
    {"[Channel1]", "beatloop_2_toggle", "[Channel1]", "autoloop_2"},
    {"[Channel1]", "beatloop_4_toggle", "[Channel1]", "autoloop_4"},
    {"[Channel1]", "beatloop_8_toggle", "[Channel1]", "autoloop_8"},
    {"[Channel1]", "beatloop_16_toggle", "[Channel1]", "autoloop_16"},

    {"[Channel2]", "beatloop_1_toggle", "[Channel2]", "autoloop_1"},
    {"[Channel2]", "beatloop_2_toggle", "[Channel2]", "autoloop_2"},
    {"[Channel2]", "beatloop_4_toggle", "[Channel2]", "autoloop_4"},
    {"[Channel2]", "beatloop_8_toggle", "[Channel2]", "autoloop_8"},
    {"[Channel2]", "beatloop_16_toggle", "[Channel2]", "autoloop_16"},

    {"[Channel3]", "beatloop_1_toggle", "[Channel1]", "autoloop_1"},
    {"[Channel3]", "beatloop_2_toggle", "[Channel1]", "autoloop_2"},
    {"[Channel3]", "beatloop_4_toggle", "[Channel1]", "autoloop_4"},
    {"[Channel3]", "beatloop_8_toggle", "[Channel1]", "autoloop_8"},
    {"[Channel3]", "beatloop_16_toggle", "[Channel1]", "autoloop_16"},

    {"[Channel4]", "beatloop_1_toggle", "[Channel2]", "autoloop_1"},
    {"[Channel4]", "beatloop_2_toggle", "[Channel2]", "autoloop_2"},
    {"[Channel4]", "beatloop_4_toggle", "[Channel2]", "autoloop_4"},
    {"[Channel4]", "beatloop_8_toggle", "[Channel2]", "autoloop_8"},
    {"[Channel4]", "beatloop_16_toggle", "[Channel2]", "autoloop_16"},

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
