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
static void* seq_handle;
static int in_port;
#elif defined(_WIN32)
bool WinMIDI_Init(void (*cb)(uint8_t, uint8_t, uint8_t), char* outDeviceName);
int WinMIDI_GetDevices(char outNames[16][64]);
bool WinMIDI_OpenDevice(int devId, char* outDeviceName);
bool WinMIDI_PopEvent(uint8_t *status, uint8_t *data1, uint8_t *data2);
bool WinMIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2);
void WinMIDI_Close(void);
#elif defined(__ANDROID__)
#endif

static MidiMapping global_mapping;

static uint8_t lastStatus = 0;
static uint8_t lastMidino = 0;
static bool lastMsgSet = false;

void MIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2) {
    (void)status; (void)data1; (void)data2;
    // Read-only mode: MIDI OUT transmission disabled
}

void MIDI_UpdateLEDs(MidiContext *ctx, DeckState *d1, DeckState *d2, AudioEngine *engine, void *appPtr) {
    (void)ctx; (void)d1; (void)d2; (void)engine; (void)appPtr;
    // Read-only mode: LED and VU updates disabled to prevent driver hangs
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

    if (MIDI_ScanControllers("controllers", deviceName, &global_mapping)) {
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
    }

    if (success) {
        char toastMsg[160];
        snprintf(toastMsg, sizeof(toastMsg), "MIDI READ-ONLY CONNECTED: %s", deviceName);
        Toast_Show(toastMsg, 3.5f, (Color){40, 200, 80, 255});
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
    return false;
#endif
#elif defined(_WIN32)
    WinMIDI_OpenDevice(0, deviceName);
#elif defined(__ANDROID__)
#endif

    ctx->currentDevId = 0;
    strncpy(ctx->activeDeviceName, deviceName, 127);

    if (!MIDI_ScanControllers("controllers", deviceName, &global_mapping)) {
        if (!MIDI_LoadMapping(&global_mapping, "controllers/LoopMIDI.midi.xml")) {
            MIDI_LoadMapping(&global_mapping, "mapping.midi.xml");
        }
    }
    
    ctx->initialized = true;

    char toastMsg[160];
    snprintf(toastMsg, sizeof(toastMsg), "MIDI READ-ONLY CONNECTED: %s", deviceName);
    Toast_Show(toastMsg, 3.5f, (Color){40, 200, 80, 255});

    return true;
}

void MIDI_Close(MidiContext *ctx) {
    if (!ctx || !ctx->initialized) return;

    char toastMsg[160];
    snprintf(toastMsg, sizeof(toastMsg), "MIDI DISCONNECTED: %s",
             ctx->activeDeviceName[0] ? ctx->activeDeviceName : "Controller");
    Toast_Show(toastMsg, 4.0f, (Color){240, 50, 50, 255});

#if defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
    snd_seq_close((snd_seq_t*)seq_handle);
#endif
#elif defined(_WIN32)
    WinMIDI_Close();
#elif defined(__ANDROID__)
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
    if (!ctx || !ctx->initialized) return;

#if defined(_WIN32)
    uint8_t status = 0, data1 = 0, data2 = 0;
    while (WinMIDI_PopEvent(&status, &data1, &data2)) {
        lastStatus = status;
        lastMidino = data1;
        lastMsgSet = true;
        MIDI_HandleMapping(&global_mapping, status, data1, (float)data2 / 127.0f);
    }
#elif defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
    snd_seq_event_t *ev;
    while (snd_seq_event_input_pending((snd_seq_t*)seq_handle, 1) > 0) {
        snd_seq_event_input((snd_seq_t*)seq_handle, &ev);
        if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
            lastStatus = 0xB0 | ev->data.control.channel;
            lastMidino = ev->data.control.param;
            lastMsgSet = true;
            MIDI_HandleMapping(&global_mapping, lastStatus, lastMidino, (float)ev->data.control.value / 127.0f);
        } else if (ev->type == SND_SEQ_EVENT_NOTEON) {
            lastStatus = 0x90 | ev->data.note.channel;
            lastMidino = ev->data.note.note;
            lastMsgSet = true;
            MIDI_HandleMapping(&global_mapping, lastStatus, lastMidino, (float)ev->data.note.velocity / 127.0f);
        }
        snd_seq_free_event(ev);
    }
#endif
#endif
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
