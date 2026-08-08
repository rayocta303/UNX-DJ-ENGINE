#include "core/midi/midi_handler.h"
#include "core/logic/control_object.h"
#include "core/midi/midi_mapper.h"
#include "core/midi/midi_scripts.h"
#include "ui/components/helpers.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__) && !defined(__ANDROID__)
#include <alsa/asoundlib.h>
#ifndef HAS_ALSA
#define HAS_ALSA 1
#endif

typedef struct _snd_midi_event snd_midi_event_t;
int snd_midi_event_new(size_t bufsize, snd_midi_event_t **rdev);
void snd_midi_event_free(snd_midi_event_t *dev);
void snd_midi_event_init(snd_midi_event_t *dev);
long snd_midi_event_encode(snd_midi_event_t *dev, const unsigned char *buf,
                           long count, snd_seq_event_t *ev);

static void *seq_handle = NULL;
static snd_midi_event_t *midi_coder = NULL;
static int in_port = -1;
static int out_port = -1;
static int dest_client = -1;
static int dest_port = -1;

static void LinuxMIDI_AutoConnectPorts(char *outDetectedName) {
#ifdef HAS_ALSA
  if (!seq_handle)
    return;
  snd_seq_t *seq = (snd_seq_t *)seq_handle;
  snd_seq_client_info_t *cinfo;
  snd_seq_port_info_t *pinfo;

  snd_seq_client_info_alloca(&cinfo);
  snd_seq_port_info_alloca(&pinfo);

  snd_seq_client_info_set_client(cinfo, -1);
  while (snd_seq_query_next_client(seq, cinfo) >= 0) {
    int client = snd_seq_client_info_get_client(cinfo);
    if (client == snd_seq_client_id(seq) || client == SND_SEQ_CLIENT_SYSTEM)
      continue;

    const char *cName = snd_seq_client_info_get_name(cinfo);
    if (!cName || strstr(cName, "Midi Through"))
      continue;

    snd_seq_port_info_set_client(pinfo, client);
    snd_seq_port_info_set_port(pinfo, -1);
    while (snd_seq_query_next_port(seq, pinfo) >= 0) {
      int port = snd_seq_port_info_get_port(pinfo);
      unsigned int caps = snd_seq_port_info_get_capability(pinfo);

      if ((caps & SND_SEQ_PORT_CAP_READ) ||
          (caps & SND_SEQ_PORT_CAP_SUBS_READ)) {
        if (in_port >= 0) {
          int err1 = snd_seq_connect_from(seq, in_port, client, port);
          printf("[MIDI] ALSA connect_from (in_port %d <- client %d:%d): %d "
                 "(%s)\n",
                 in_port, client, port, err1, cName);
          if (outDetectedName && outDetectedName[0] == '\0') {
            strncpy(outDetectedName, cName, 127);
          }
        }
      }
      if ((caps & SND_SEQ_PORT_CAP_WRITE) ||
          (caps & SND_SEQ_PORT_CAP_SUBS_WRITE)) {
        if (out_port >= 0) {
          int err2 = snd_seq_connect_to(seq, out_port, client, port);
          printf(
              "[MIDI] ALSA connect_to (out_port %d -> client %d:%d): %d (%s)\n",
              out_port, client, port, err2, cName);
          dest_client = client;
          dest_port = port;
        }
      }
    }
  }
#else
  (void)outDetectedName;
#endif
}
#elif defined(_WIN32)
bool WinMIDI_Init(void (*cb)(uint8_t, uint8_t, uint8_t), char *outDeviceName);
int WinMIDI_GetDevices(char outNames[16][64]);
bool WinMIDI_OpenDevice(int devId, char *outDeviceName);
bool WinMIDI_PopEvent(uint8_t *status, uint8_t *data1, uint8_t *data2);
bool WinMIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2);
bool WinMIDI_SendSysEx(const uint8_t *data, uint32_t length);
void WinMIDI_Close(void);
#elif defined(__ANDROID__)
#endif

static MidiMapping global_mapping;

static uint8_t lastStatus = 0;
static uint8_t lastMidino = 0;
static bool lastMsgSet = false;

void MIDI_SendSysEx(const uint8_t *data, uint32_t length) {
#if defined(_WIN32)
  WinMIDI_SendSysEx(data, length);
#else
  (void)data; (void)length;
#endif
}

void MIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2) {
#if defined(_WIN32)
  WinMIDI_SendShortMsg(status, data1, data2);
#elif defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
  if (!seq_handle)
    return;
  snd_seq_event_t ev;
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, out_port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  uint8_t type = status & 0xF0;
  if (type == 0xB0) {
    snd_seq_ev_set_controller(&ev, status & 0x0F, data1, data2);
  } else if (type == 0x90) {
    snd_seq_ev_set_noteon(&ev, status & 0x0F, data1, data2);
  } else if (type == 0x80) {
    snd_seq_ev_set_noteoff(&ev, status & 0x0F, data1, data2);
  } else {
    snd_seq_ev_set_controller(&ev, status & 0x0F, data1, data2);
  }
  snd_seq_event_output((snd_seq_t *)seq_handle, &ev);
  snd_seq_drain_output((snd_seq_t *)seq_handle);
#endif
#else
  (void)status; (void)data1; (void)data2;
#endif
}

void MIDI_FlushDebugLogs(void) {
}

void MIDI_UpdateLEDs(MidiContext *ctx, DeckState *d1, DeckState *d2,
                     AudioEngine *engine, void *appPtr) {
  (void)ctx; (void)appPtr;
  if (engine) {
    MIDI_UpdateVuMeters(engine, false);
  }
  MIDI_UpdateLoopAndPadLEDs(d1, d2, engine, false);
}


int MIDI_GetDeviceList(char outNames[16][64]) {
#if defined(_WIN32)
  return WinMIDI_GetDevices(outNames);
#elif defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
  int count = 0;
  snd_seq_t *seq = NULL;
  if (seq_handle) {
    seq = (snd_seq_t *)seq_handle;
  } else {
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_READ, 0) < 0)
      return 0;
  }

  snd_seq_client_info_t *cinfo;
  snd_seq_client_info_alloca(&cinfo);
  snd_seq_client_info_set_client(cinfo, -1);
  while (snd_seq_query_next_client(seq, cinfo) >= 0 && count < 16) {
    int client = snd_seq_client_info_get_client(cinfo);
    if (client == SND_SEQ_CLIENT_SYSTEM)
      continue;
    if (seq_handle && client == snd_seq_client_id((snd_seq_t *)seq_handle))
      continue;

    const char *name = snd_seq_client_info_get_name(cinfo);
    if (name && strlen(name) > 0 && strstr(name, "Midi Through") == NULL) {
      strncpy(outNames[count], name, 63);
      outNames[count][63] = '\0';
      count++;
    }
  }
  if (!seq_handle && seq) {
    snd_seq_close(seq);
  }
  return count;
#else
  (void)outNames;
  return 0;
#endif
#else
  (void)outNames;
  return 0;
#endif
}

bool MIDI_SelectDevice(MidiContext *ctx, int deviceIndex, char *outDeviceName,
                       char *outPresetPath) {
  char deviceName[256] = "Generic MIDI";
  bool success = false;
#if defined(_WIN32)
  success = WinMIDI_OpenDevice(deviceIndex, deviceName);
#elif defined(__linux__) && !defined(__ANDROID__)
  char devNames[16][64];
  int devCount = MIDI_GetDeviceList(devNames);
  if (deviceIndex >= 0 && deviceIndex < devCount) {
    strncpy(deviceName, devNames[deviceIndex], 255);
    success = true;
  }
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
        if (strstr(names[i], global_mapping.name) ||
            strstr(global_mapping.name, names[i])) {
          strncpy(outPresetPath, paths[i], 255);
          break;
        }
      }
    }
  }

  if (success) {
    char toastMsg[160];
    snprintf(toastMsg, sizeof(toastMsg), "MIDI CONNECTED: %s", deviceName);
    Toast_Show(toastMsg, 3.5f, (Color){40, 200, 80, 255});
  }

  return success;
}

bool MIDI_Init(MidiContext *ctx) {
  if (ctx->initialized)
    return true;

  char deviceName[256] = "";

#if defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
  if (!seq_handle) {
    snd_seq_t *seq = NULL;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) >= 0) {
      seq_handle = seq;
      snd_seq_set_client_name(seq, "UNX-DJ-OS MIDI");
      in_port = snd_seq_create_simple_port(
          seq, "Input", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
          SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
      out_port = snd_seq_create_simple_port(
          seq, "Output", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
          SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    }
  }
  if (seq_handle) {
    LinuxMIDI_AutoConnectPorts(deviceName);
    // Dispatch Pioneer FLX/DDJ LED enable handshakes
    MIDI_SendShortMsg(0x9F, 0x00, 0x7F);
    MIDI_SendShortMsg(0x90, 0x00, 0x7F);
    MIDI_SendShortMsg(0x91, 0x00, 0x7F);
    static const uint8_t sysexEnable[] = {0xF0, 0x00, 0x20, 0x2B, 0x07, 0x00,
                                          0x00, 0x00, 0x01, 0x00, 0x00, 0xF7};
    MIDI_SendSysEx(sysexEnable, sizeof(sysexEnable));
  } else {
    return false;
  }
#else
  return false;
#endif
#elif defined(_WIN32)
  WinMIDI_OpenDevice(0, deviceName);
#elif defined(__ANDROID__)
#endif

  if (deviceName[0] == '\0') {
    strncpy(deviceName, "Generic MIDI", 255);
  }

  ctx->currentDevId = 0;
  strncpy(ctx->activeDeviceName, deviceName, 127);

  if (!MIDI_ScanControllers("controllers", deviceName, &global_mapping)) {
    if (!MIDI_LoadMapping(&global_mapping,
                          "controllers/Pioneer-DDJ-FLX6.midi.xml")) {
      if (!MIDI_LoadMapping(&global_mapping, "controllers/LoopMIDI.midi.xml")) {
        MIDI_LoadMapping(&global_mapping, "mapping.midi.xml");
      }
    }
  }

  ctx->initialized = true;

  // Pioneer DDJ Hardware Init / LED Enable Handshake
  MIDI_SendShortMsg(0x9F, 0x00, 0x7F);
  MIDI_SendShortMsg(0x9F, 0x01, 0x7F);
  MIDI_SendShortMsg(0x90, 0x7F, 0x7F);
  MIDI_SendShortMsg(0x91, 0x7F, 0x7F);
  MIDI_SendShortMsg(0x93, 0x7F, 0x7F);
  MIDI_SendShortMsg(0x94, 0x7F, 0x7F);

  char toastMsg[160];
  snprintf(toastMsg, sizeof(toastMsg), "MIDI CONNECTED: %s", deviceName);
  Toast_Show(toastMsg, 3.5f, (Color){40, 200, 80, 255});

  return true;
}

void MIDI_Close(MidiContext *ctx) {
  if (!ctx || !ctx->initialized)
    return;

  char toastMsg[160];
  snprintf(toastMsg, sizeof(toastMsg), "MIDI DISCONNECTED: %s",
           ctx->activeDeviceName[0] ? ctx->activeDeviceName : "Controller");
  Toast_Show(toastMsg, 4.0f, (Color){240, 50, 50, 255});

#if defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
  // Keep seq_handle open to allow instant auto-reconnect without re-allocating
  // ALSA client
#endif
#elif defined(_WIN32)
  WinMIDI_Close();
#elif defined(__ANDROID__)
#endif
  ctx->initialized = false;
}

void MIDI_CheckHotplug(MidiContext *ctx) {
  if (!ctx)
    return;

  static double lastCheckTime = 0;
  double now = GetTime();
  if (now - lastCheckTime < 0.4)
    return; // Rate-limit hotplug checks to every 400ms
  lastCheckTime = now;

  char devNames[16][64];
  int count = MIDI_GetDeviceList(devNames);

  if (ctx->initialized) {
    bool activeFound = false;
    for (int i = 0; i < count; i++) {
      if (ctx->activeDeviceName[0] != '\0' &&
          (strstr(devNames[i], ctx->activeDeviceName) ||
           strstr(ctx->activeDeviceName, devNames[i]))) {
        activeFound = true;
        break;
      }
    }

#if defined(__linux__) && !defined(__ANDROID__)
    char autoDetected[128] = "";
    LinuxMIDI_AutoConnectPorts(autoDetected);
    if (!activeFound && autoDetected[0] != '\0') {
      activeFound = true;
      strncpy(ctx->activeDeviceName, autoDetected, 127);
    }
#endif

    if (!activeFound && count == 0) {
      MIDI_Close(ctx);
      ctx->activeDeviceName[0] = '\0';
    }
  } else {
    if (count > 0) {
      char deviceName[256] = "";
#if defined(__linux__) && !defined(__ANDROID__)
      LinuxMIDI_AutoConnectPorts(deviceName);
#elif defined(_WIN32)
      WinMIDI_OpenDevice(0, deviceName);
#endif
      if (deviceName[0] == '\0' && count > 0) {
        strncpy(deviceName, devNames[0], 255);
      }

      ctx->currentDevId = 0;
      strncpy(ctx->activeDeviceName, deviceName, 127);

      if (!MIDI_ScanControllers("controllers", deviceName, &global_mapping)) {
        if (!MIDI_LoadMapping(&global_mapping,
                              "controllers/Pioneer-DDJ-FLX6.midi.xml")) {
          if (!MIDI_LoadMapping(&global_mapping,
                                "controllers/LoopMIDI.midi.xml")) {
            MIDI_LoadMapping(&global_mapping, "mapping.midi.xml");
          }
        }
      }
      ctx->initialized = true;

      char toastMsg[160];
      snprintf(toastMsg, sizeof(toastMsg), "MIDI RECONNECTED: %s", deviceName);
      Toast_Show(toastMsg, 3.5f, (Color){40, 200, 80, 255});
    }
  }
}

void MIDI_Update(MidiContext *ctx, DeckState *d1, DeckState *d2,
                 AudioEngine *engine) {
  (void)d1;
  (void)d2;
  (void)engine;
  if (!ctx || !ctx->initialized)
    return;

#if defined(_WIN32)
  uint8_t status = 0, data1 = 0, data2 = 0;
  while (WinMIDI_PopEvent(&status, &data1, &data2)) {
    lastStatus = status;
    lastMidino = data1;
    lastMsgSet = true;
    // MIDI_LogDebugAsync("[MIDI IN ] Status: 0x%02X, Note/CC: 0x%02X, Val:
    // 0x%02X (%d)",
    //                    status, data1, data2, data2);
    MIDI_HandleMapping(&global_mapping, status, data1, (float)data2 / 127.0f);
  }
#elif defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
  if (!seq_handle)
    return;
  snd_seq_event_t *ev = NULL;
  while (snd_seq_event_input_pending((snd_seq_t *)seq_handle, 1) > 0) {
    if (snd_seq_event_input((snd_seq_t *)seq_handle, &ev) >= 0 && ev) {
      if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
        lastStatus = 0xB0 | ev->data.control.channel;
        lastMidino = ev->data.control.param;
        lastMsgSet = true;
        // MIDI_LogDebugAsync("[MIDI IN ] Status: 0x%02X, CC: 0x%02X, Val: %d",
        //                    lastStatus, lastMidino, ev->data.control.value);
        MIDI_HandleMapping(&global_mapping, lastStatus, lastMidino,
                           (float)ev->data.control.value / 127.0f);
      } else if (ev->type == SND_SEQ_EVENT_NOTEON) {
        lastStatus = 0x90 | ev->data.note.channel;
        lastMidino = ev->data.note.note;
        lastMsgSet = true;
        // MIDI_LogDebugAsync("[MIDI IN ] Status: 0x%02X, Note: 0x%02X, Vel:
        // %d",
        //                    lastStatus, lastMidino, ev->data.note.velocity);
        MIDI_HandleMapping(&global_mapping, lastStatus, lastMidino,
                           (float)ev->data.note.velocity / 127.0f);
      } else if (ev->type == SND_SEQ_EVENT_NOTEOFF) {
        lastStatus = 0x80 | ev->data.note.channel;
        lastMidino = ev->data.note.note;
        lastMsgSet = true;
        // MIDI_LogDebugAsync("[MIDI IN ] Status: 0x%02X, Note: 0x%02X, Off",
        //                    lastStatus, lastMidino);
        MIDI_HandleMapping(&global_mapping, lastStatus, lastMidino, 0.0f);
      } else if (ev->type == SND_SEQ_EVENT_PITCHBEND) {
        lastStatus = 0xE0 | ev->data.note.channel;
        lastMidino = 0;
        lastMsgSet = true;
        float normVal = (float)(ev->data.control.value + 8192) / 16383.0f;
        // MIDI_LogDebugAsync("[MIDI IN ] Status: 0x%02X, PitchBend: %d",
        //                    lastStatus, ev->data.control.value);
        MIDI_HandleMapping(&global_mapping, lastStatus, lastMidino, normVal);
      }
      snd_seq_free_event(ev);
    }
  }
#endif
#endif
  MIDI_FlushDebugLogs();
}

MidiMapping *MIDI_GetGlobalMapping(void) { return &global_mapping; }

void MIDI_RefreshMapping(const char *path) {
  if (path) {
    MIDI_LoadMapping(&global_mapping, path);
  }
}

bool MIDI_GetLastMessage(uint8_t *status, uint8_t *midino) {
  if (!lastMsgSet)
    return false;
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
  if (!lastMsgSet)
    return false;
  *status = lastStatus;
  *midino = lastMidino;
  return true;
}
