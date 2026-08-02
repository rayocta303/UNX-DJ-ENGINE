#include "core/midi/midi_handler.h"
#include "core/midi/midi_mapper.h"
#include "core/logic/control_object.h"
#include "ui/components/helpers.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#if defined(__linux__) && !defined(__ANDROID__)
#if __has_include(<alsa/asoundlib.h>)
#include <alsa/asoundlib.h>
#define HAS_ALSA
#endif
static void* seq_handle; // Generic pointer if no ALSA
static int in_port;
#elif defined(_WIN32)
// Forward declarations for win backend
bool WinMIDI_Init(void (*cb)(uint8_t, uint8_t, uint8_t), char* outDeviceName);
void WinMIDI_SetCallback(void (*cb)(uint8_t, uint8_t, uint8_t));
int WinMIDI_GetDevices(char outNames[16][64]);
bool WinMIDI_OpenDevice(int devId, char* outDeviceName);
bool WinMIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2);
void WinMIDI_Close();
#elif defined(__ANDROID__)
// MIDI mapping on Android is not implemented yet.
#endif

static MidiMapping global_mapping;

// Internal callback/queue for messages
#define MAX_MIDI_QUEUE 128
static MidiMessage midiQueue[MAX_MIDI_QUEUE];
static int queueHead = 0;
static int queueTail = 0;
static uint8_t lastStatus = 0;
static uint8_t lastMidino = 0;
static bool lastMsgSet = false;

void MIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2) {
#if defined(_WIN32)
    WinMIDI_SendShortMsg(status, data1, data2);
#else
    (void)status; (void)data1; (void)data2;
#endif
}

void MIDI_UpdateLEDs(MidiContext *ctx, DeckState *d1, DeckState *d2, AudioEngine *engine, void *appPtr) {
    (void)appPtr;
    if (!ctx || !ctx->initialized) return;

    // --- Mixxx Static XML Output Mapping LED Processor ---
    MidiMapping *map = &global_mapping;
    if (map && map->outputCount > 0) {
        for (int i = 0; i < map->outputCount; i++) {
            MidiOutputEntry *out = &map->outputs[i];
            float val = CO_GetValue(out->group, out->key);
            
            uint8_t targetByte = out->off;
            if (val >= out->minimum && val <= out->maximum) {
                targetByte = out->on;
            }
            
            if ((int)targetByte != out->lastSentVal) {
                if (targetByte != 0xFF) {
                    MIDI_SendShortMsg(out->status, out->midino, targetByte);
                }
                out->lastSentVal = (int)targetByte;
            }
        }
    }

    // --- VU Meters ---
    float level1 = 0.0f, level2 = 0.0f, masterLevel = 0.0f;
    if (engine) {
        float p1 = fmaxf(engine->Decks[0].VuMeterL, engine->Decks[0].VuMeterR);
        float p2 = fmaxf(engine->Decks[1].VuMeterL, engine->Decks[1].VuMeterR);

        float fader1 = engine->Decks[0].Fader;
        float fader2 = engine->Decks[1].Fader;

        if (fader1 > 0.01f) {
            level1 = p1 * fader1;
        } else if (engine->Decks[0].IsCueActive) {
            level1 = p1; // Show gain stage level during headphones cueing
        } else {
            level1 = p1 * fader1;
        }

        if (fader2 > 0.01f) {
            level2 = p2 * fader2;
        } else if (engine->Decks[1].IsCueActive) {
            level2 = p2;
        } else {
            level2 = p2 * fader2;
        }

        if (level1 > 1.0f) level1 = 1.0f;
        if (level2 > 1.0f) level2 = 1.0f;

        masterLevel = fmaxf(engine->MasterVuL, engine->MasterVuR);
        if (masterLevel > 1.0f) masterLevel = 1.0f;

        // Register in ControlObjects for Mixxx XML mappings
        CO_SetValue("[Channel1]", "VuMeter", level1);
        CO_SetValue("[Channel2]", "VuMeter", level2);
        CO_SetValue("[Master]", "VuMeterL", engine->MasterVuL);
        CO_SetValue("[Master]", "VuMeterR", engine->MasterVuR);
    }

    // Send VU Meter CCs for Channel 1..4 & Master
    MIDI_SendShortMsg(0xB0, 0x02, (uint8_t)(level1 * 127.0f));
    MIDI_SendShortMsg(0xB1, 0x02, (uint8_t)(level2 * 127.0f));
    MIDI_SendShortMsg(0xB2, 0x02, (uint8_t)(level1 * 127.0f));
    MIDI_SendShortMsg(0xB3, 0x02, (uint8_t)(level2 * 127.0f));
    MIDI_SendShortMsg(0xBF, 0x02, (uint8_t)(masterLevel * 127.0f));

    // --- Deck A (Channel 1 / Deck 1 & 3) LEDs ---
    if (d1) {
        // Play & Cue
        MIDI_SendShortMsg(0x90, 0x0B, d1->IsPlaying ? 0x7F : 0x00);
        MIDI_SendShortMsg(0x90, 0x0C, (d1->IsCueHeld || (engine && engine->Decks[0].IsCueActive)) ? 0x7F : (d1->MainCueMs > 0 ? 0x7F : 0x00));
        // Sync & Master Tempo & Slip
        MIDI_SendShortMsg(0x90, 0x58, d1->MidiRequestSync ? 0x7F : 0x00);
        MIDI_SendShortMsg(0x90, 0x1A, d1->MasterTempo ? 0x7F : 0x00);
        MIDI_SendShortMsg(0x90, 0x3F, (engine && engine->Decks[0].SlipActive) ? 0x7F : 0x00);
        // Track Loaded LED
        MIDI_SendShortMsg(0x9F, 0x00, d1->LoadedTrack ? 0x7F : 0x00);
        // Loop In / Out LEDs
        MIDI_SendShortMsg(0x90, 0x10, (engine && engine->Decks[0].IsLooping) ? 0x7F : 0x00);
        MIDI_SendShortMsg(0x90, 0x11, (engine && engine->Decks[0].IsLooping) ? 0x7F : 0x00);
        // Headphone / PFL Cue LED
        MIDI_SendShortMsg(0x90, 0x54, (engine && engine->Decks[0].IsCueActive) ? 0x7F : 0x00);

        // Hot Cues 1-8 LEDs (Channel 7 / Channel 0 Note 0x00..0x07)
        for (int h = 0; h < 8; h++) {
            bool cueSet = (d1->LoadedTrack && d1->LoadedTrack->HotCues[h].Start > 0);
            MIDI_SendShortMsg(0x97, (uint8_t)h, cueSet ? 0x7F : 0x00);
        }
    }

    // --- Deck B (Channel 2 / Deck 2 & 4) LEDs ---
    if (d2) {
        // Play & Cue
        MIDI_SendShortMsg(0x91, 0x0B, d2->IsPlaying ? 0x7F : 0x00);
        MIDI_SendShortMsg(0x91, 0x0C, (d2->IsCueHeld || (engine && engine->Decks[1].IsCueActive)) ? 0x7F : (d2->MainCueMs > 0 ? 0x7F : 0x00));
        // Sync & Master Tempo & Slip
        MIDI_SendShortMsg(0x91, 0x58, d2->MidiRequestSync ? 0x7F : 0x00);
        MIDI_SendShortMsg(0x91, 0x1A, d2->MasterTempo ? 0x7F : 0x00);
        MIDI_SendShortMsg(0x91, 0x3F, (engine && engine->Decks[1].SlipActive) ? 0x7F : 0x00);
        // Track Loaded LED
        MIDI_SendShortMsg(0x9F, 0x01, d2->LoadedTrack ? 0x7F : 0x00);
        // Loop In / Out LEDs
        MIDI_SendShortMsg(0x91, 0x10, (engine && engine->Decks[1].IsLooping) ? 0x7F : 0x00);
        MIDI_SendShortMsg(0x91, 0x11, (engine && engine->Decks[1].IsLooping) ? 0x7F : 0x00);
        // Headphone / PFL Cue LED
        MIDI_SendShortMsg(0x91, 0x54, (engine && engine->Decks[1].IsCueActive) ? 0x7F : 0x00);

        // Hot Cues 1-8 LEDs (Channel 8 / Channel 1 Note 0x00..0x07)
        for (int h = 0; h < 8; h++) {
            bool cueSet = (d2->LoadedTrack && d2->LoadedTrack->HotCues[h].Start > 0);
            MIDI_SendShortMsg(0x98, (uint8_t)h, cueSet ? 0x7F : 0x00);
        }
    }
}

static void EnqueueMIDI(uint8_t b1, uint8_t b2, uint8_t b3) {

    printf("[MIDI RECV] Status: 0x%02X | Note/CC: 0x%02X | Val: %d\n", b1, b2, b3);
    int next = (queueHead + 1) % MAX_MIDI_QUEUE;
    if (next == queueTail) return; // Full
    if (Midi_Parse(b1, b2, b3, &midiQueue[queueHead])) {
        queueHead = next;
    }
}

int MIDI_GetDeviceList(char outNames[16][64]) {
#if defined(_WIN32)
    return WinMIDI_GetDevices(outNames);
#else
    (void)outNames;
    return 0;
#endif
}

bool MIDI_SelectDevice(MidiContext *ctx, int deviceIndex, char *outDeviceName, char *outPresetPath) {
    char deviceName[256] = "Generic MIDI";
    bool success = false;
#if defined(_WIN32)
    WinMIDI_SetCallback(EnqueueMIDI);
    success = WinMIDI_OpenDevice(deviceIndex, deviceName);
#else
    (void)deviceIndex;
#endif

    if (outDeviceName) {
        strncpy(outDeviceName, deviceName, 127);
        outDeviceName[127] = '\0';
    }
    if (ctx) {
        ctx->currentDevId = deviceIndex;
        strncpy(ctx->activeDeviceName, deviceName, 127);
    }

    // Auto scan & load mapping for the connected device name
    if (MIDI_ScanControllers("controllers", deviceName, &global_mapping)) {
        printf("[MIDI] Connected to '%s' using auto-detected mapping '%s' (%d entries).\n",
               deviceName, global_mapping.name, global_mapping.count);
        if (outPresetPath) {
            char names[32][64];
            char paths[32][256];
            int count = MIDI_ListControllers("controllers", names, paths);
            for (int i = 0; i < count; i++) {
                if (strstr(names[i], global_mapping.name) || strstr(global_mapping.name, names[i])) {
                    strncpy(outPresetPath, paths[i], 255);
                    break;
                }
            }
        }
    } else {
        printf("[MIDI] Warning: No specific mapping XML matching '%s' found in controllers/.\n", deviceName);
    }

    if (success) {
        char toastMsg[160];
        snprintf(toastMsg, sizeof(toastMsg), "MIDI CONNECTED: %s", deviceName);
        Toast_Show(toastMsg, 3.5f, (Color){40, 200, 80, 255}); // Green Toast
    }

    return success;
}

bool MIDI_Init(MidiContext *ctx) {
    if (ctx->initialized) return true;
    
    char deviceName[256] = "Generic MIDI";

#if defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
    if (snd_seq_open((snd_seq_t**)&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0) return false;
    snd_seq_set_client_name((snd_seq_t*)seq_handle, "UNX-DJ-OS MIDI");
    in_port = snd_seq_create_simple_port((snd_seq_t*)seq_handle, "Input",
                SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
#else
    return false; // No ALSA support in this build
#endif
#elif defined(_WIN32)
    WinMIDI_SetCallback(EnqueueMIDI);
    WinMIDI_OpenDevice(0, deviceName);
#elif defined(__ANDROID__)
    // Stub
#endif

    ctx->currentDevId = 0;
    strncpy(ctx->activeDeviceName, deviceName, 127);

    // Organize mappings in a 'controllers' folder like Engine
    if (!MIDI_ScanControllers("controllers", deviceName, &global_mapping)) {
        printf("[MIDI] Warning: No specific mapping found for '%s'. Trying fallback.\n", deviceName);
        if (!MIDI_LoadMapping(&global_mapping, "controllers/LoopMIDI.midi.xml")) {
            MIDI_LoadMapping(&global_mapping, "mapping.midi.xml");
        }
    } else {
        printf("[MIDI] Multi-mode: Connected to '%s' using mapping '%s' (%d entries).\n", 
               deviceName, global_mapping.name, global_mapping.count);
    }
    
    ctx->initialized = true;

    char toastMsg[160];
    snprintf(toastMsg, sizeof(toastMsg), "MIDI CONNECTED: %s", deviceName);
    Toast_Show(toastMsg, 3.5f, (Color){40, 200, 80, 255}); // Green Toast

    return true;
}


void MIDI_Close(MidiContext *ctx) {
    if (!ctx || !ctx->initialized) return;

    char toastMsg[160];
    snprintf(toastMsg, sizeof(toastMsg), "MIDI DISCONNECTED: %s",
             ctx->activeDeviceName[0] ? ctx->activeDeviceName : "Controller");
    Toast_Show(toastMsg, 4.0f, (Color){240, 50, 50, 255}); // Red Alert Toast

#if defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
    snd_seq_close((snd_seq_t*)seq_handle);
#endif
#elif defined(_WIN32)
    WinMIDI_Close();
#elif defined(__ANDROID__)
    // Stub
#endif
    ctx->initialized = false;
}

void MIDI_CheckHotplug(MidiContext *ctx) {
    if (!ctx) return;

    static int lastDevCount = -1;
    char devNames[16][64];
    int count = MIDI_GetDeviceList(devNames);

    if (lastDevCount == -1) {
        lastDevCount = count;
        return;
    }

    if (ctx->initialized) {
        bool found = false;
        for (int i = 0; i < count; i++) {
            if (strcmp(devNames[i], ctx->activeDeviceName) == 0) {
                found = true;
                break;
            }
        }
        if (!found && count < lastDevCount) {
            MIDI_Close(ctx);
            ctx->activeDeviceName[0] = '\0';
        }
    } else {
        if (count > 0 && count > lastDevCount) {
            char outDev[128];
            MIDI_SelectDevice(ctx, 0, outDev, NULL);
        }
    }

    lastDevCount = count;
}

void MIDI_Update(MidiContext *ctx, DeckState *d1, DeckState *d2, AudioEngine *engine) {
    (void)d1; (void)d2; (void)engine;
    if (!ctx->initialized) return;

#if defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
    snd_seq_event_t *ev;
    while (snd_seq_event_input_pending((snd_seq_t*)seq_handle, 1) > 0) {
        snd_seq_event_input((snd_seq_t*)seq_handle, &ev);
        if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
            EnqueueMIDI(0xB0 | ev->data.control.channel, ev->data.control.param, ev->data.control.value);
        } else if (ev->type == SND_SEQ_EVENT_NOTEON) {
            EnqueueMIDI(0x90 | ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
        }
        snd_seq_free_event(ev);
    }
#endif
#endif

    while (queueTail != queueHead) {
        MidiMessage *m = &midiQueue[queueTail];
        lastStatus = m->status;
        lastMidino = m->control;
        lastMsgSet = true;
        MIDI_HandleMapping(&global_mapping, m->status, m->control, m->value / 127.0f);
        queueTail = (queueTail + 1) % MAX_MIDI_QUEUE;
    }
}

MidiMapping* MIDI_GetGlobalMapping(void) {
    return &global_mapping;
}

void MIDI_RefreshMapping(const char *path) {
    if (path) {
        MIDI_LoadMapping(&global_mapping, path);
    }
}

bool MIDI_GetLastMessage(uint8_t *status, uint8_t *midino) {
    if (!lastMsgSet) return false;
    *status = lastStatus;
    *midino = lastMidino;
    lastMsgSet = false;
    return true;
}

bool MIDI_SaveCurrentMapping(const char *name) {
    char path[512];
    snprintf(path, 512, "controllers/%s.midi.xml", name);
    strncpy(global_mapping.name, name, 127);
    return MIDI_SaveMapping(&global_mapping, path);
}

bool MIDI_PeekLastMessage(uint8_t *status, uint8_t *midino) {
    if (!lastMsgSet) return false;
    *status = lastStatus;
    *midino = lastMidino;
    return true;
}
