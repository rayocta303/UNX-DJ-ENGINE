#ifndef MIDI_MESSAGE_H
#define MIDI_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * MIDI Operation Codes (Borrowed from Mixxx)
 */
typedef enum {
    MIDI_NOTE_OFF                 = 0x8,
    MIDI_NOTE_ON                  = 0x9,
    MIDI_POLYPHONIC_KEY_PRESSURE  = 0xA,
    MIDI_CONTROL_CHANGE           = 0xB,
    MIDI_PROGRAM_CHANGE           = 0xC,
    MIDI_CHANNEL_PRESSURE         = 0xD,
    MIDI_PITCH_BEND               = 0xE,
    MIDI_SYSTEM_EXCLUSIVE         = 0xF
} MidiOpCode;

typedef enum {
    MIDI_OPTION_NONE              = 0,
    MIDI_OPTION_INVERT            = 1 << 0,
    MIDI_OPTION_ROT64             = 1 << 1, // Relative rotation (Center 64)
    MIDI_OPTION_DIFF              = 1 << 2, // Sign-magnitude
    MIDI_OPTION_BUTTON            = 1 << 3,
    MIDI_OPTION_SWITCH            = 1 << 4,
    MIDI_OPTION_SOFT_TAKEOVER     = 1 << 5,
    MIDI_OPTION_14BIT             = 1 << 6
} MidiOption;

typedef struct {
    uint8_t status;
    uint8_t channel;
    uint8_t opcode;
    uint8_t control;
    uint8_t value;
    uint16_t value14bit;
    double timestamp;
} MidiMessage;

/**
 * Parses Raw Bytes into a MidiMessage
 * Logic derived from Mixxx MidiUtils
 */
static inline bool Midi_Parse(uint8_t b1, uint8_t b2, uint8_t b3, MidiMessage *msg) {
    if (!(b1 & 0x80)) return false; // First byte must have high bit set
    
    msg->status = b1;
    msg->opcode = (b1 >> 4) & 0x0F;
    msg->channel = b1 & 0x0F;
    msg->control = b2 & 0x7F;
    msg->value = b3 & 0x7F;
    
    // Handle 14-bit (L/M SB) if needed elsewhere
    msg->value14bit = (msg->value << 7) | msg->control;
    
    return true;
}

#endif // MIDI_MESSAGE_H
