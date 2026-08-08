#include "core/midi/midi_handler.h"
#include "core/logic/control_object.h"
#include "core/midi/midi_mapper.h"
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
#elif defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
  if (seq_handle && out_port >= 0 && data && length > 0) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, out_port);
    snd_seq_ev_set_subs(&ev);
    if (dest_client >= 0 && dest_port >= 0) {
      ev.dest.client = (uint8_t)dest_client;
      ev.dest.port = (uint8_t)dest_port;
    }
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_sysex(&ev, length, (void *)data);

    snd_seq_event_output((snd_seq_t *)seq_handle, &ev);
    snd_seq_drain_output((snd_seq_t *)seq_handle);
  }
#endif
#endif
}

void MIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2) {
#if defined(_WIN32)
  WinMIDI_SendShortMsg(status, data1, data2);
#elif defined(__linux__) && !defined(__ANDROID__)
#ifdef HAS_ALSA
  if (seq_handle && out_port >= 0) {
    if (!midi_coder) {
      snd_midi_event_new(256, (snd_midi_event_t **)&midi_coder);
    }
    if (midi_coder) {
      uint8_t buf[3] = {status, data1, data2};
      snd_seq_event_t ev;
      snd_seq_ev_clear(&ev);
      snd_seq_ev_set_source(&ev, out_port);
      snd_seq_ev_set_subs(&ev);
      if (dest_client >= 0 && dest_port >= 0) {
        ev.dest.client = (uint8_t)dest_client;
        ev.dest.port = (uint8_t)dest_port;
      }
      snd_seq_ev_set_direct(&ev);

      snd_midi_event_init(midi_coder);
      long ret = snd_midi_event_encode(midi_coder, buf, 3, &ev);
      if (ret > 0) {
        snd_seq_event_output((snd_seq_t *)seq_handle, &ev);
        snd_seq_drain_output((snd_seq_t *)seq_handle);
      }
    }
  }
#endif
#endif
}

typedef struct {
  char text[128];
} MidiLogMessage;

#define MIDI_LOG_RING_SIZE 256
static MidiLogMessage s_midiLogRing[MIDI_LOG_RING_SIZE];
static volatile int s_midiLogHead = 0;
static volatile int s_midiLogTail = 0;

static void MIDI_LogDebugAsync(const char *fmt, ...) {
  int nextHead = (s_midiLogHead + 1) % MIDI_LOG_RING_SIZE;
  if (nextHead == s_midiLogTail) {
    // Buffer full, drop log to maintain non-blocking realtime execution
    return;
  }
  va_list args;
  va_start(args, fmt);
  vsnprintf(s_midiLogRing[s_midiLogHead].text,
            sizeof(s_midiLogRing[s_midiLogHead].text), fmt, args);
  va_end(args);
  s_midiLogHead = nextHead;
}

void MIDI_FlushDebugLogs(void) {
  while (s_midiLogTail != s_midiLogHead) {
    puts(s_midiLogRing[s_midiLogTail].text);
    s_midiLogTail = (s_midiLogTail + 1) % MIDI_LOG_RING_SIZE;
  }
}

static void MIDI_SendShortMsgNamed(uint8_t status, uint8_t data1, uint8_t data2,
                                   const char *name) {
  MIDI_SendShortMsg(status, data1, data2);
  //   if (name) {
  //     MIDI_LogDebugAsync(
  //         "[MIDI OUT] %-28s | Status: 0x%02X, Note/CC: 0x%02X, Val: 0x%02X
  //         (%d)", name, status, data1, data2, data2);
  //   }
}

void MIDI_UpdateLEDs(MidiContext *ctx, DeckState *d1, DeckState *d2,
                     AudioEngine *engine, void *appPtr) {
  (void)ctx;
  (void)appPtr;
  if (!d1 || !d2)
    return;

  static double lastSendTime = 0;
  static double lastFullRefresh = 0;
  static double lastBlinkTime = 0;
  static bool blinkState = false;
  static bool connectionHandshakeDone = false;
  static void *lastLoadedTrack[4] = {NULL, NULL, NULL, NULL};
  double now = GetTime();

  if (now - lastBlinkTime > 0.300) {
    lastBlinkTime = now;
    blinkState = !blinkState;
  }

  // 1. One-time Connection Handshake when device connects (Wakeup hardware LED drivers)
  if (!connectionHandshakeDone) {
    connectionHandshakeDone = true;
    MIDI_SendShortMsgNamed(0x9F, 0x00, 0x7F, "Connect Handshake Master 0x9F");
    MIDI_SendShortMsgNamed(0x90, 0x7F, 0x7F, "Connect Handshake Ch 1");
    MIDI_SendShortMsgNamed(0x91, 0x7F, 0x7F, "Connect Handshake Ch 2");
    MIDI_SendShortMsgNamed(0x92, 0x7F, 0x7F, "Connect Handshake Ch 3");
    MIDI_SendShortMsgNamed(0x93, 0x7F, 0x7F, "Connect Handshake Ch 4");
  }

  bool forceRefresh = false;
  if (lastFullRefresh == 0 || now - lastFullRefresh > 2.0) {
    lastFullRefresh = now;
    forceRefresh = true;
  }

  // 2. Pioneer SysEx Keep-Alive every 1.5 seconds
  static double lastSysEx = 0;
  if (now - lastSysEx > 1.5) {
    lastSysEx = now;
    static const uint8_t PIONEER_SYSEX_KEEPALIVE[12] = {
        0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xF7};
    MIDI_SendSysEx(PIONEER_SYSEX_KEEPALIVE, 12);
  }

  // Rate-limit short MIDI OUT updates to ~60 FPS (0.016s) for smooth LED spinner animation
  if (!forceRefresh && (now - lastSendTime < 0.016))
    return;
  lastSendTime = now;

  static uint8_t lastPlay[4] = {255, 255, 255, 255};
  static uint8_t lastCue[4] = {255, 255, 255, 255};
  static uint8_t lastVinyl[4] = {255, 255, 255, 255};
  static uint8_t lastVu[4] = {255, 255, 255, 255};
  static uint8_t lastJog[4] = {255, 255, 255, 255};
  static uint8_t lastHotCue[4][8] = {{255, 255, 255, 255, 255, 255, 255, 255},
                                     {255, 255, 255, 255, 255, 255, 255, 255},
                                     {255, 255, 255, 255, 255, 255, 255, 255},
                                     {255, 255, 255, 255, 255, 255, 255, 255}};

  // 3. Deck State LEDs for up to 4 Decks (Channel 1..4)
  MidiMapping *map = MIDI_GetGlobalMapping();

  for (int i = 0; i < 4; i++) {
    DeckState *deck = (i == 0) ? d1 : (i == 1 ? d2 : (i == 2 ? d1 : d2));
    if (!deck)
      continue;

    // Trigger Handshake Deck ONCE when track loaded on this deck
    if (deck->LoadedTrack != lastLoadedTrack[i]) {
      lastLoadedTrack[i] = deck->LoadedTrack;
      if (deck->LoadedTrack != NULL) {
        char lblDeckHs[64];
        snprintf(lblDeckHs, sizeof(lblDeckHs),
                 "Handshake Deck %d (Track Loaded)", i + 1);
        MIDI_SendShortMsgNamed(0x9F, (uint8_t)i, 0x7F, lblDeckHs);
      }
    }

    const char *groupNames[4] = {"[Channel1]", "[Channel2]", "[Channel3]",
                                 "[Channel4]"};
    const char *group = groupNames[i];

    uint8_t playStatus = 0x90 + i, playNote = 0x0B;
    uint8_t cueStatus = 0x90 + i, cueNote = 0x0C;
    uint8_t vinylStatus = 0x90 + i, vinylNote = 0x0E;
    uint8_t loopInStatus = 0x90 + i, loopInNote = 0x10;
    uint8_t loopOutStatus = 0x90 + i, loopOutNote = 0x11;
    uint8_t reloopStatus = 0x90 + i, reloopNote = 0x4D;
    uint8_t vuStatus = 0xB0 + i, vuControl = 0x02;

    if (map) {
      uint8_t s, m;
      if (MIDI_GetRegisterAddress(map, group, "play", &s, &m)) {
        playStatus = s;
        playNote = m;
      }
      if (MIDI_GetRegisterAddress(map, group, "cue_default", &s, &m) ||
          MIDI_GetRegisterAddress(map, group, "cue", &s, &m)) {
        cueStatus = s;
        cueNote = m;
      }
      if (MIDI_GetRegisterAddress(map, group, "vinyl", &s, &m)) {
        vinylStatus = s;
        vinylNote = m;
      }
      if (MIDI_GetRegisterAddress(map, group, "loop_in", &s, &m)) {
        loopInStatus = s;
        loopInNote = m;
      }
      if (MIDI_GetRegisterAddress(map, group, "loop_out", &s, &m)) {
        loopOutStatus = s;
        loopOutNote = m;
      }
      if (MIDI_GetRegisterAddress(map, group, "reloop", &s, &m)) {
        reloopStatus = s;
        reloopNote = m;
      }
      if (MIDI_GetRegisterAddress(map, group, "vuMeterUpdate", &s, &m) ||
          MIDI_GetRegisterAddress(map, group, "vu", &s, &m)) {
        vuStatus = s;
        vuControl = m;
      }
    }

    // Play/Pause LED: Solid ON when playing; Blinking when paused at Cue; OFF when stopped
    uint8_t playVal = 0x00;
    if (deck->IsPlaying) {
      playVal = 0x7F;
    } else if (deck->PositionMs <= deck->MainCueMs + 50 && deck->LoadedTrack) {
      playVal = blinkState ? 0x7F : 0x00;
    }
    if (forceRefresh || playVal != lastPlay[i]) {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "Play LED [Deck %d]", i + 1);
      MIDI_SendShortMsgNamed(playStatus, playNote, playVal, lbl);
      lastPlay[i] = playVal;
    }

    // Cue LED: Solid ON when holding cue / paused at Cue; Blinking when playing; OFF when empty
    uint8_t cueVal = 0x00;
    if (deck->IsCueActive || deck->IsCueHeld ||
        (!deck->IsPlaying && deck->PositionMs <= deck->MainCueMs + 50)) {
      cueVal = 0x7F;
    } else if (deck->IsPlaying && deck->LoadedTrack) {
      cueVal = blinkState ? 0x7F : 0x00;
    }
    if (forceRefresh || cueVal != lastCue[i]) {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "Cue LED [Deck %d]", i + 1);
      MIDI_SendShortMsgNamed(cueStatus, cueNote, cueVal, lbl);
      lastCue[i] = cueVal;
    }

    // Vinyl Mode LED
    uint8_t vinylVal = deck->VinylModeEnabled ? 0x7F : 0x00;
    if (forceRefresh || vinylVal != lastVinyl[i]) {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "Vinyl LED [Deck %d]", i + 1);
      MIDI_SendShortMsgNamed(vinylStatus, vinylNote, vinylVal, lbl);
      lastVinyl[i] = vinylVal;
    }

    // Loop In / Out & Reloop LEDs
    uint8_t loopVal = 0x00;
    if (deck->LoopAdjustIn || deck->LoopAdjustOut) {
      loopVal = blinkState ? 0x7F : 0x00;
    } else if (deck->IsLooping) {
      loopVal = 0x7F;
    }
    MIDI_SendShortMsg(loopInStatus, loopInNote, loopVal);   // Loop In
    MIDI_SendShortMsg(loopOutStatus, loopOutNote, loopVal); // Loop Out
    MIDI_SendShortMsg(loopInStatus, 0x4C, loopVal);         // Shift Loop In
    MIDI_SendShortMsg(loopOutStatus, 0x4E, loopVal);        // Shift Loop Out
    MIDI_SendShortMsg(reloopStatus, reloopNote,
                      deck->IsLooping ? 0x7F : 0x00); // Reloop

    // Channel VU Meter Level (Channel 1 = 0xB0, Channel 2 = 0xB1, Channel 3 = 0xB2, Channel 4 = 0xB3, CC 0x02)
    float rms = 0.0f;
    if (engine && i < 2 && (engine->Decks[i].VuMeterL > 0.001f || engine->Decks[i].VuMeterR > 0.001f)) {
      float vuL = engine->Decks[i].VuMeterL;
      float vuR = engine->Decks[i].VuMeterR;
      rms = (vuL > vuR ? vuL : vuR);
    } else if (deck->LoadedTrack && deck->IsPlaying) {
      // Dynamic fallback simulation when active playback is running
      rms = 0.70f + 0.20f * (float)sin(now * 15.0 + i * 2.0);
    }
    float fader = (engine && i < 2) ? engine->Decks[i].Fader : 1.0f;
    float level = (fader > 0.01f) ? (rms * fader) : (deck->IsCueActive ? rms : (rms * fader));
    uint8_t meterVal = (uint8_t)(level * 127.0f);
    if (meterVal > 127)
      meterVal = 127;
    if (forceRefresh || meterVal != lastVu[i]) {
      char lblVu[64];
      snprintf(lblVu, sizeof(lblVu), "Channel VU LED [Deck %d]", i + 1);
      MIDI_SendShortMsgNamed(vuStatus, vuControl, meterVal, lblVu);
      lastVu[i] = meterVal;
    }

    // Hot Cue Pad LEDs (Deck 1=0x97, Deck 2=0x99, Deck 3=0x98, Deck 4=0x9A)
    uint8_t padStatus =
        (i == 0) ? 0x97 : (i == 1 ? 0x99 : (i == 2 ? 0x98 : 0x9A));
    for (int p = 0; p < 8; p++) {
      uint8_t padNote = (uint8_t)p;
      bool hasHotCue = false;
      if (deck->LoadedTrack) {
        for (int h = 0; h < deck->LoadedTrack->HotCuesCount; h++) {
          if (deck->LoadedTrack->HotCues[h].ID == (unsigned int)(p + 1)) {
            hasHotCue = true;
            break;
          }
        }
        if (!hasHotCue && deck->LoadedTrack->Analysis.CueCount > 0) {
          for (uint32_t c = 0; c < deck->LoadedTrack->Analysis.CueCount; c++) {
            if (deck->LoadedTrack->Analysis.Cues[c].ID ==
                (unsigned int)(p + 1)) {
              hasHotCue = true;
              break;
            }
          }
        }
      }
      uint8_t padVal = hasHotCue ? 0x7F : 0x00;
      if (forceRefresh || padVal != lastHotCue[i][p]) {
        char lblPad[64];
        snprintf(lblPad, sizeof(lblPad), "HotCue %d LED [Deck %d]", p + 1,
                 i + 1);
        MIDI_SendShortMsgNamed(padStatus, padNote, padVal, lblPad);
        MIDI_SendShortMsg(padStatus, 0x30 + padNote, padVal);
        lastHotCue[i][p] = padVal;
      }
    }
  }

  // 4. Beat FX On/Off LED
  if (engine) {
    static uint8_t lastFxOn = 255;
    uint8_t fxVal = engine->BeatFX.isFxOn ? 0x7F : 0x00;
    if (forceRefresh || fxVal != lastFxOn) {
      MIDI_SendShortMsgNamed(0x94, 0x47, fxVal, "Beat FX On/Off LED");
      MIDI_SendShortMsg(0x94, 0x43, fxVal); // Shift Beat FX On/Off LED
      lastFxOn = fxVal;
    }
  }

  // 5. Master VU Meter Level (Master L = 0xBA CC 0x00, Master R = 0xBA CC 0x01)
  if (engine) {
    static uint8_t lastMasterL = 255, lastMasterR = 255;
    float mL_val = engine->MasterVuL;
    float mR_val = engine->MasterVuR;
    if (mL_val <= 0.001f && (d1->IsPlaying || d2->IsPlaying)) {
      mL_val = 0.75f + 0.15f * (float)sin(now * 12.0);
      mR_val = 0.75f + 0.15f * (float)cos(now * 12.0);
    }
    uint8_t mL = (uint8_t)(mL_val * 127.0f);
    uint8_t mR = (uint8_t)(mR_val * 127.0f);
    if (mL > 127)
      mL = 127;
    if (mR > 127)
      mR = 127;
    if (forceRefresh || mL != lastMasterL) {
      MIDI_SendShortMsgNamed(0xBA, 0x00, mL, "Master VU Left LED");
      lastMasterL = mL;
    }
    if (forceRefresh || mR != lastMasterR) {
      MIDI_SendShortMsgNamed(0xBA, 0x01, mR, "Master VU Right LED");
      lastMasterR = mR;
    }
  }

  // 6. Jog Wheel Rings (Pioneer DDJ-FLX6 status 0xBB, control 0x00..0x03, value 0x01..0x48)
  // Formula matching Mixxx playPositionUpdate: (posSec * 72 * 0.6075) % 72 + 1
  for (int i = 0; i < 4; i++) {
    DeckState *deck = (i == 0) ? d1 : (i == 1 ? d2 : (i == 2 ? d1 : d2));
    if (!deck)
      continue;

    double posSec = (double)deck->PositionMs / 1000.0;
    if (posSec <= 0.0001 && engine && i < 2) {
      posSec = engine->Decks[i].Position / (double)(engine->Decks[i].SampleRate
                                               ? engine->Decks[i].SampleRate
                                               : 44100);
    }

    double jogStep = fmod(posSec * 72.0 * 0.6075, 72.0);
    if (jogStep < 0.0)
      jogStep += 72.0;
    uint8_t pos = 0x01 + (uint8_t)jogStep;
    if (pos > 0x48)
      pos = 0x48;

    if (forceRefresh || pos != lastJog[i]) {
      char lblJog[64];
      snprintf(lblJog, sizeof(lblJog), "Jog Ring LED [Deck %d]", i + 1);
      MIDI_SendShortMsgNamed(0xBB, (uint8_t)i, pos, lblJog);
      lastJog[i] = pos;
    }
  }
  MIDI_FlushDebugLogs();
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
