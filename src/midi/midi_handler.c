#include "midi/midi_handler.h"
#include "midi/midi_mapper.h"
#include <stdio.h>

#if defined(__linux__)
#include <alsa/asoundlib.h>
static snd_seq_t *seq_handle;
static int in_port;
#elif defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
static HMIDIIN hMidiIn = NULL;
#endif

static MidiMapping global_mapping;

// Internal callback/queue for messages
#define MAX_MIDI_QUEUE 128
static MidiMessage midiQueue[MAX_MIDI_QUEUE];
static int queueHead = 0;
static int queueTail = 0;

static void EnqueueMIDI(uint8_t b1, uint8_t b2, uint8_t b3) {
    int next = (queueHead + 1) % MAX_MIDI_QUEUE;
    if (next == queueTail) return; // Full
    if (Midi_Parse(b1, b2, b3, &midiQueue[queueHead])) {
        queueHead = next;
    }
}

#if defined(_WIN32)
void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (wMsg == MIM_DATA) {
        EnqueueMIDI((uint8_t)(dwParam1 & 0xFF), (uint8_t)((dwParam1 >> 8) & 0xFF), (uint8_t)((dwParam1 >> 16) & 0xFF));
    }
}
#endif

bool MIDI_Init(MidiContext *ctx) {
    if (ctx->initialized) return true;
    
    char deviceName[256] = "Generic MIDI";

#if defined(__linux__)
    if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0) return false;
    snd_seq_set_client_name(seq_handle, "UNX-DJ-OS MIDI");
    in_port = snd_seq_create_simple_port(seq_handle, "Input",
                SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    // On Linux ALSA, we can try to get the first available device name if needed, 
    // but for now we'll scan based on the port name or remain generic.
#elif defined(_WIN32)
    if (midiInGetNumDevs() > 0) {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(0, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR) {
            strncpy(deviceName, caps.szPname, 255);
        }
        if (midiInOpen(&hMidiIn, 0, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
            midiInStart(hMidiIn);
        }
    }
#endif

    // Organize mappings in a 'controllers' folder like Mixxx
    if (!MIDI_ScanControllers("controllers", deviceName, &global_mapping)) {
        printf("[MIDI] Warning: No specific mapping found for '%s'. Trying fallback.\n", deviceName);
        if (!MIDI_LoadMapping(&global_mapping, "mapping.midi.xml")) {
            printf("[MIDI] Error: No fallback mapping.midi.xml found.\n");
        }
    } else {
        printf("[MIDI] Multi-mode: Connected to '%s' using mapping '%s' (%d entries).\n", 
               deviceName, global_mapping.name, global_mapping.count);
    }
    
    ctx->initialized = true;
    return true;
}


void MIDI_Close(MidiContext *ctx) {
    if (!ctx->initialized) return;
#if defined(__linux__)
    snd_seq_close(seq_handle);
#elif defined(_WIN32)
    if (hMidiIn) {
        midiInStop(hMidiIn);
        midiInClose(hMidiIn);
    }
#endif
    ctx->initialized = false;
}

void MIDI_Update(MidiContext *ctx, DeckState *d1, DeckState *d2, AudioEngine *engine) {
    (void)d1; (void)d2; (void)engine;
    if (!ctx->initialized) return;

#if defined(__linux__)
    snd_seq_event_t *ev;
    while (snd_seq_event_input_pending(seq_handle, 1) > 0) {
        snd_seq_event_input(seq_handle, &ev);
        if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
            EnqueueMIDI(0xB0 | ev->data.control.channel, ev->data.control.param, ev->data.control.value);
        } else if (ev->type == SND_SEQ_EVENT_NOTEON) {
            EnqueueMIDI(0x90 | ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
        }
        snd_seq_free_event(ev);
    }
#endif

    while (queueTail != queueHead) {
        MidiMessage *m = &midiQueue[queueTail];
        MIDI_HandleMapping(&global_mapping, m->status, m->control, m->value / 127.0f);
        queueTail = (queueTail + 1) % MAX_MIDI_QUEUE;
    }
}

