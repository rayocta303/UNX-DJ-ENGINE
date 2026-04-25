#ifndef MIDI_TRANSLATION_H
#define MIDI_TRANSLATION_H

typedef struct {
    const char *srcGroup;
    const char *srcKey;
    const char *unxGroup;
    const char *unxKey;
} MidiTranslation;

const MidiTranslation* MIDI_GetTranslation(const char *group, const char *key);

#endif
