#include "core/audio_backend.h"
#include "core/logger.h"
#include "core/logic/control_object.h"
#include "core/logic/settings_io.h"
#include "core/logic/sync.h"
#include "core/midi/midi_handler.h"
#include "core/system_info.h"
#include "input/keyboard.h"
#include "raylib.h"
#include "rlgl.h"
#include "ui/browser/browser.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/player.h"
#include "ui/views/about.h"
#include "ui/views/credits.h"
#include "ui/views/debug_ios.h"
#include "ui/views/info.h"
#include "ui/views/mixer.h"
#include "ui/views/pad.h"
#include "ui/views/settings.h"
#include "ui/views/splash.h"
#include "ui/views/topbar.h"
#include "core/logic/quantize.h"
#include "version.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define Rectangle WinRectangle
#define CloseWindow WinCloseWindow
#define ShowCursor WinShowCursor
#define DrawText WinDrawText
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#else
#include <unistd.h>
#endif

#ifdef _WIN32
#define STBI_NO_SIMD
#define STBI_ASSERT(x)
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Custom loader to bypass Raylib DLL format limitations (e.g. missing JPG
// support)
Image LoadImageManual(const char *path) {
  int width, height, channels;
  unsigned char *data = stbi_load(path, &width, &height, &channels, 4);
  if (!data) {
    printf("[STBI] Failed to load '%s': %s\n", path, stbi_failure_reason());
    return (Image){0};
  }

  // Create a Raylib-compatible image and copy data to avoid memory management
  // issues
  Image img = {.data = RL_MALLOC(width * height * 4),
               .width = width,
               .height = height,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  memcpy(img.data, data, width * height * 4);
  stbi_image_free(data);

  return img;
}
#else
// On other platforms (Android, iOS), we build Raylib from source with full
// format support
Image LoadImageManual(const char *path) { return LoadImage(path); }
#endif

typedef struct {
  CurrentScreen screen;
  int splashCounter;

  DeckState deckA;
  DeckState deckB;
  BeatFXState fxState;
  BrowserState browserState;
  InfoState infoState;
  SettingsState settingsState;
  AboutState aboutState;
  CreditsState creditsState;
  MixerState mixerState;
  PadState padState;

  TopBar topbar;
  DeckStrip stripA;
  DeckStrip stripB;
  PlayerRenderer player;
  BrowserRenderer browser;
  InfoRenderer info;
  SettingsRenderer settings;
  AboutRenderer about;
  CreditsRenderer credits;
  MixerRenderer mixer;
  PadRenderer pad;
  SplashRenderer splash;
  DebugIOSView debugView;
  KeyboardMapping keyMap;
  MidiContext midiCtx;
  bool showExitConfirm;
  AudioBackendConfig activeAudioConfig;

  bool MidiRequestSettings;
  bool MidiRequestInfo;
  bool MidiRequestMixer;
  bool MidiRequestBrowser;

  char midiPresetPaths[32][256];
  int midiPresetCount;
  char activeControllerPath[256];
} App;

void PopulateMidiSettings(App *a);

AudioEngine *globalAudioEngine = NULL;
App *globalApp = NULL; // Needed for iOS callbacks

void AudioProcessCallback(float *buffer, unsigned int frames);
void UpdateDrawFrame(App *app);

#if defined(PLATFORM_IOS)
// ghera/raylib-iOS callbacks
void ios_ready(void) {
  UNX_LOG_INFO("[IOS] ios_ready: Initializing Window...");

  // 1. Initialize Window
  InitWindow(0, 0, APP_NAME);

  // Stability delay for iOS surface binding
  UNX_LOG_INFO("[IOS] ios_ready: Window Init called. Waiting for driver "
               "stabilization...");
  usleep(100000); // 100ms

  SetTargetFPS(60);

  // 2. Initialize Fonts
  UNX_LOG_INFO("[IOS] ios_ready: Initializing Fonts...");
#ifndef DEBUG_IOS_GUI
  UIFonts_Init();
#endif

  // 3. Start Audio Backend
  UNX_LOG_INFO("[IOS] ios_ready: Initializing Audio...");
  if (globalApp && globalApp->activeAudioConfig.SampleRate > 0) {
    AudioBackend_Start(globalApp->activeAudioConfig, AudioProcessCallback);
  }

  // 4. Force a clear frame to bind GPU surface
  UNX_LOG_INFO("[IOS] ios_ready: Performing initial clear...");
  BeginDrawing();
  ClearBackground(ORANGE);
  DrawText("XDJ-UNX INITIALIZING...", 40, 40, 20, BLACK);
  EndDrawing();

  UNX_LOG_INFO("[IOS] ios_ready: Final Window Size: %dx%d (Ready: %d)",
               GetScreenWidth(), GetScreenHeight(), IsWindowReady());
}

void ios_update(void) {
  if (globalApp)
    UpdateDrawFrame(globalApp);
}

void ios_destroy(void) { UNX_LOG_INFO("[IOS] ios_destroy called."); }
#endif

void AudioProcessCallback(float *buffer, unsigned int frames) {
  if (globalAudioEngine) {
    AudioEngine_Process(globalAudioEngine, buffer, frames);
  }
}

void OnSettingsClose(void *ctx) {
  App *a = (App *)ctx;
  a->screen = ScreenPlayer;
  a->settingsState.IsActive = false;
}

void OnSettingsApply(void *ctx) {
  App *a = (App *)ctx;

  // Sync UI items back to deck states
  int styleIdx = a->settingsState.Items[2].Current;
  a->deckA.Waveform.Style = (WaveformStyle)styleIdx;
  a->deckB.Waveform.Style = (WaveformStyle)styleIdx;

  a->deckA.Waveform.GainLow = 0.1f + (a->settingsState.Items[3].Current * 0.1f);
  a->deckA.Waveform.GainMid = 0.1f + (a->settingsState.Items[4].Current * 0.1f);
  a->deckA.Waveform.GainHigh = 0.1f + (a->settingsState.Items[5].Current * 0.1f);

  a->deckB.Waveform = a->deckA.Waveform;

  a->deckA.Waveform.LoadLock = (a->settingsState.Items[1].Current == 1);
  a->deckA.Waveform.VinylStartMs = a->settingsState.Items[6].Value;
  a->deckA.Waveform.VinylStopMs = a->settingsState.Items[7].Value;
  a->deckB.Waveform = a->deckA.Waveform;

  UNX_LOG_INFO(
      "[SETTINGS] Applied Style: %d, Gains: L%.2f M%.2f H%.2f, Start: %.0f, "
      "Stop: %.0f, Lock: %d",
      a->deckA.Waveform.Style, a->deckA.Waveform.GainLow,
      a->deckA.Waveform.GainMid, a->deckA.Waveform.GainHigh,
      a->deckA.Waveform.VinylStartMs, a->deckA.Waveform.VinylStopMs,
      a->deckA.Waveform.LoadLock);

  // Apply Audio backend settings
  AudioBackendConfig aconf = {
      .DeviceIndex =
          a->settingsState.Items[8].Current - 1, // 0 is System Default
      .MasterOutL = a->settingsState.Items[9].Current,
      .MasterOutR = a->settingsState.Items[10].Current,
      // Cue items have "Blank" at index 0, so subtract 1 for physical channel
      .CueOutL = a->settingsState.Items[11].Current - 1,
      .CueOutR = a->settingsState.Items[12].Current - 1,
      .SampleRate = (a->settingsState.Items[14].Current == 0) ? 44100 : 48000,
      .PCMBitDepth = (a->settingsState.Items[15].Current == 0) ? 16 : 24,
  };
  int bufMap[] = {128, 256, 512, 1024};
  aconf.BufferSizeFrames = bufMap[a->settingsState.Items[13].Current];

  // Auto-restart audio ONLY if hardware-critical config changed
  bool audioChanged =
      (aconf.DeviceIndex != a->activeAudioConfig.DeviceIndex) ||
      (aconf.SampleRate != a->activeAudioConfig.SampleRate) ||
      (aconf.BufferSizeFrames != a->activeAudioConfig.BufferSizeFrames) ||
      (aconf.MasterOutL != a->activeAudioConfig.MasterOutL) ||
      (aconf.MasterOutR != a->activeAudioConfig.MasterOutR) ||
      (aconf.CueOutL != a->activeAudioConfig.CueOutL) ||
      (aconf.CueOutR != a->activeAudioConfig.CueOutR);

  if (audioChanged) {
    UNX_LOG_INFO(
        "[SETTINGS] Audio Hardware config changed, restarting backend...");
    if (AudioBackend_Start(aconf, AudioProcessCallback)) {
      a->activeAudioConfig = aconf;

      // Sync actual hardware sample rate back to engine
      int actualSR = 0;
      AudioBackend_GetActiveInfo(NULL, &actualSR, NULL, NULL);
      if (globalAudioEngine)
        AudioEngine_SetOutputSampleRate(globalAudioEngine, actualSR);

      // Update bit depth in engine
      if (globalAudioEngine)
        AudioEngine_SetPCMBitDepth(globalAudioEngine, aconf.PCMBitDepth);

      // Update active driver info in About screen
      AudioBackend_GetActiveInfo(NULL, NULL, a->aboutState.AudioDriver,
                                 a->aboutState.AudioDevice);
    }
  } else {
    // Hardware didn't change, but software preference might have
    if (aconf.PCMBitDepth != a->activeAudioConfig.PCMBitDepth) {
      a->activeAudioConfig.PCMBitDepth = aconf.PCMBitDepth;
      if (globalAudioEngine)
        AudioEngine_SetPCMBitDepth(globalAudioEngine, aconf.PCMBitDepth);
      UNX_LOG_INFO("[SETTINGS] PCM Bit Depth changed to %d-bit",
                   aconf.PCMBitDepth);
    }
  }

  Settings_Save(a->deckA.Waveform, a->deckB.Waveform, a->activeAudioConfig,
                a->activeControllerPath);
}

void UpdateChannelOptions(App *a, int deviceIdx) {
  AudioDeviceInfo devs[MAX_AUDIO_DEVICES];
  int devCount = AudioBackend_GetDevices(devs, MAX_AUDIO_DEVICES);

  int channels = 2; // Fallback for System Default
  if (deviceIdx >= 0 && deviceIdx < devCount) {
    channels = devs[deviceIdx].NativeChannels;
  }

  // Common init for channel items
  strcpy(a->settingsState.Items[9].Label, "MASTER LEFT");
  a->settingsState.Items[9].Type = SETTING_TYPE_LIST;
  strcpy(a->settingsState.Items[10].Label, "MASTER RIGHT");
  a->settingsState.Items[10].Type = SETTING_TYPE_LIST;
  strcpy(a->settingsState.Items[11].Label, "CUE LEFT");
  a->settingsState.Items[11].Type = SETTING_TYPE_LIST;
  strcpy(a->settingsState.Items[12].Label, "CUE RIGHT");
  a->settingsState.Items[12].Type = SETTING_TYPE_LIST;

  a->settingsState.Items[9].Category = SETTING_CAT_AUDIO;
  a->settingsState.Items[10].Category = SETTING_CAT_AUDIO;
  a->settingsState.Items[11].Category = SETTING_CAT_AUDIO;
  a->settingsState.Items[12].Category = SETTING_CAT_AUDIO;

  // Master L/R: CH 1..N
  a->settingsState.Items[9].OptionsCount = channels;
  a->settingsState.Items[10].OptionsCount = channels;
  for (int i = 0; i < channels && i < MAX_SETTING_OPTIONS; i++) {
    sprintf(a->settingsState.Items[9].Options[i], "CH %d", i + 1);
    sprintf(a->settingsState.Items[10].Options[i], "CH %d", i + 1);
  }

  // Cue L/R: Blank, CH 1..N
  a->settingsState.Items[11].OptionsCount = channels + 1;
  a->settingsState.Items[12].OptionsCount = channels + 1;
  strcpy(a->settingsState.Items[11].Options[0], "Blank");
  strcpy(a->settingsState.Items[12].Options[0], "Blank");
  for (int i = 0; i < channels && (i + 1) < MAX_SETTING_OPTIONS; i++) {
    sprintf(a->settingsState.Items[11].Options[i + 1], "CH %d", i + 1);
    sprintf(a->settingsState.Items[12].Options[i + 1], "CH %d", i + 1);
  }

  // Auto-Select defaults
  // Auto-Select defaults: Start with 2-channel mirrored setup (CH 1/2 for
  // Master and Cue) to prevent errors on fresh settings, even if more channels
  // are available.
  a->settingsState.Items[9].Current = 0; // Master L -> CH1
  a->settingsState.Items[10].Current =
      (channels > 1) ? 1 : 0; // Master R -> CH2

  a->settingsState.Items[11].Current = 1; // Cue L -> CH 1 (Index 0 is Blank)
  a->settingsState.Items[12].Current = (channels > 1) ? 2 : 1; // Cue R -> CH 2
}

void OnSettingsValueChanged(void *ctx, int idx) {
  App *a = (App *)ctx;
  if (idx == 8) { // AUDIO DEVICE changed
    UpdateChannelOptions(a, a->settingsState.Items[8].Current - 1);
  } else if (idx == 17) { // MIDI PRESET changed
    int presetIdx = a->settingsState.Items[17].Current;
    if (presetIdx < a->midiPresetCount) {
      strncpy(a->activeControllerPath, a->midiPresetPaths[presetIdx], 255);
      MIDI_RefreshMapping(a->activeControllerPath);
      // Refresh the MIDI mapping tab items
      PopulateMidiSettings(a);
      // Save immediately when preset changes
      Settings_Save(a->deckA.Waveform, a->deckB.Waveform, a->activeAudioConfig,
                    a->activeControllerPath);
    }
  }
}

void OnSettingsAction(void *ctx, int idx) {
  App *a = (App *)ctx;
  if (idx == 16) { // ABOUT
    a->screen = ScreenAbout;
    a->aboutState.IsActive = true;
  } else if (idx == 17) { // CREDITS
    a->screen = ScreenCredits;
    a->creditsState.IsActive = true;
  } else if (idx == 18) { // EXIT APPLICATION
    a->showExitConfirm = true;
  } else if (idx == 20) { // CREATE NEW MAPPING
    MIDI_CreateTemplate(MIDI_GetGlobalMapping());
    PopulateMidiSettings(a);
  } else if (idx == 21) { // SAVE CURRENT AS
    // For simplicity, we use the current map name or a default
    MIDI_GetGlobalMapping(); // Refresh without assigning if unused
    char nameBuf[128];
    static int saveCounter = 0;
    snprintf(nameBuf, 128, "Custom_%d", ++saveCounter);
    if (MIDI_SaveCurrentMapping(nameBuf)) {
      printf("[MIDI] Mapping saved to controllers/%s.midi.xml\n", nameBuf);
    }
    PopulateMidiSettings(a);
  }
}

void PopulateMidiSettings(App *a) {
  // --- PRESET SELECTION ITEM (Index 19) ---
  char names[32][64];
  a->midiPresetCount =
      MIDI_ListControllers("controllers", names, a->midiPresetPaths);

  strcpy(a->settingsState.Items[19].Label, "MAPPING PRESET");
  a->settingsState.Items[19].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[19].Category = SETTING_CAT_CONTROLLERS;
  a->settingsState.Items[19].OptionsCount = a->midiPresetCount;
  a->settingsState.Items[19].Current = 0;

  for (int i = 0; i < a->midiPresetCount; i++) {
    strncpy(a->settingsState.Items[19].Options[i], names[i], 31);
    if (strcmp(a->activeControllerPath, a->midiPresetPaths[i]) == 0) {
      a->settingsState.Items[19].Current = i;
    }
  }

  // --- ACTIONS (20, 21) ---
  strcpy(a->settingsState.Items[20].Label, "CREATE NEW MAPPING");
  a->settingsState.Items[20].Type = SETTING_TYPE_ACTION;
  a->settingsState.Items[20].Category = SETTING_CAT_CONTROLLERS;
  strcpy(a->settingsState.Items[20].Unit, "RESET");

  strcpy(a->settingsState.Items[21].Label, "SAVE CURRENT MAPPING");
  a->settingsState.Items[21].Type = SETTING_TYPE_ACTION;
  a->settingsState.Items[21].Category = SETTING_CAT_CONTROLLERS;
  strcpy(a->settingsState.Items[21].Unit, "SAVE");

  // --- MAPPING ENTRIES (Starting from Index 22) ---
  MidiMapping *map = MIDI_GetGlobalMapping();
  if (!map)
    return;

  int baseIdx = 22;
  for (int i = 0; i < map->count && (baseIdx + i) < MAX_SETTINGS_ITEMS; i++) {
    MappingEntry *e = &map->entries[i];
    snprintf(a->settingsState.Items[baseIdx + i].Label, 64, "%s %s", e->group,
             e->key);
    a->settingsState.Items[baseIdx + i].Type = SETTING_TYPE_ACTION;
    a->settingsState.Items[baseIdx + i].Category = SETTING_CAT_CONTROLLERS;

    snprintf(a->settingsState.Items[baseIdx + i].Unit, 16, "0x%02X:0x%02X",
             e->status, e->midino);

    // Store type in Options[0] for display
    if (e->options & 4)
      strcpy(a->settingsState.Items[baseIdx + i].Options[0], "SCRIPT");
    else if (e->options & 1)
      strcpy(a->settingsState.Items[baseIdx + i].Options[0], "REL");
    else if (e->options & 8 || e->options & 16)
      strcpy(a->settingsState.Items[baseIdx + i].Options[0], "14BIT");
    else
      strcpy(a->settingsState.Items[baseIdx + i].Options[0], "NORM");
  }

  int totalCount = baseIdx + map->count;
  if (totalCount > MAX_SETTINGS_ITEMS)
    totalCount = MAX_SETTINGS_ITEMS;
  a->settingsState.ItemsCount = totalCount;
}

void TopBar_OnBrowse(void *ctx) {
  App *a = (App *)ctx;
  if (a->screen == ScreenBrowser) {
    a->screen = ScreenPlayer;
    a->browserState.IsActive = false;
  } else {
    a->screen = ScreenBrowser;
    a->browserState.IsActive = true;
  }
}

void TopBar_OnMixer(void *ctx) {
  App *a = (App *)ctx;
  if (a->screen == ScreenMixer) {
    a->screen = ScreenPlayer;
    a->mixerState.IsActive = false;
  } else {
    a->screen = ScreenMixer;
    a->mixerState.IsActive = true;
    a->mixerState.AudioPlugin = globalAudioEngine;
  }
}

void TopBar_OnInfo(void *ctx) {
  App *a = (App *)ctx;
  if (a->screen == ScreenInfo) {
    a->screen = ScreenPlayer;
    a->infoState.IsActive = false;
  } else {
    a->screen = ScreenInfo;
    a->infoState.IsActive = true;

    // Sync Info State
    for (int i = 0; i < 2; i++) {
      DeckState *ds = (i == 0) ? &a->deckA : &a->deckB;
      InfoTrack *it = &a->infoState.Tracks[i];
      strcpy(it->Title, ds->TrackTitle);
      strcpy(it->Artist, ds->ArtistName);
      it->BPM = ds->OriginalBPM;
      strcpy(it->Key, ds->TrackKey);
      it->Duration = ds->TrackLengthMs / 1000;
      strcpy(it->Source, ds->SourceName);
    }
  }
}

void TopBar_OnPad(void *ctx) {
  App *a = (App *)ctx;
  if (a->screen == ScreenPad) {
    a->screen = ScreenPlayer;
    a->padState.IsActive = false;
  } else {
    a->screen = ScreenPad;
    a->padState.IsActive = true;
  }
}

void OnPadPress(void *ctx, int deckIdx, int padIdx) {
  App *a = (App *)ctx;
  DeckState *ds = (deckIdx == 0) ? &a->deckA : &a->deckB;
  DeckAudioState *audio = &globalAudioEngine->Decks[deckIdx];
  PadMode mode = a->padState.Mode[deckIdx];

  if (!ds->LoadedTrack)
    return;

  if (mode == PAD_MODE_HOT_CUE) {
    if (padIdx < ds->LoadedTrack->HotCuesCount) {
      HotCue hc = ds->LoadedTrack->HotCues[padIdx];
      ds->SeekMs = hc.Start;
      ds->HasSeekRequest = true;

      // Start playback immediately without motor ramp
      DeckAudio_InstantPlay(audio);
      (void)ds; // Mark as used
    }
  } else if (mode == PAD_MODE_BEAT_LOOP || mode == PAD_MODE_SLIP_LOOP) {
    if (mode == PAD_MODE_BEAT_LOOP &&
        a->padState.ActiveLoopIdx[deckIdx] == padIdx) {
      // Toggle OFF
      DeckAudio_ExitLoop(audio);
      a->padState.ActiveLoopIdx[deckIdx] = -1;
      return;
    }

    // Loop lengths in beats
    static float beatLengths[] = {0.25f, 0.5f, 1.0f,  2.0f,
                                  4.0f,  8.0f, 16.0f, 32.0f};
    float beats = beatLengths[padIdx];

    double startPos = audio->Position;
    bool isResizing = audio->IsLooping;
    if (isResizing) {
      startPos = audio->LoopStartPos;
    }

    double loopLengthSamples = 0;
    double trackSR = (double)audio->SampleRate;

    if (ds->QuantizeEnabled && ds->LoadedTrack &&
        ds->LoadedTrack->BeatGridCount > 0) {
      // Find nearest beat index
      double currentMs = (startPos / trackSR) * 1000.0;
      int nearestIdx = 0;
      double minDist = 10000.0;
      for (int i = 0; i < ds->LoadedTrack->BeatGridCount; i++) {
        double d = fabs(ds->LoadedTrack->BeatGrid[i].Time - currentMs);
        if (d < minDist) {
          minDist = d;
          nearestIdx = i;
        }
      }

      startPos =
          (ds->LoadedTrack->BeatGrid[nearestIdx].Time / 1000.0) * trackSR;

      // Calculate end pos using grid if possible (assuming 1 grid entry per
      // beat)
      int beatsCount = (int)beats;
      if (beats < 1.0f) {
        // Fractional beat loop (e.g. 1/4, 1/2)
        double nextBeatMs = (nearestIdx + 1 < ds->LoadedTrack->BeatGridCount)
                                ? ds->LoadedTrack->BeatGrid[nearestIdx + 1].Time
                                : (ds->LoadedTrack->BeatGrid[nearestIdx].Time +
                                   (60000.0 / ds->CurrentBPM));
        double beatDurationMs =
            nextBeatMs - ds->LoadedTrack->BeatGrid[nearestIdx].Time;
        loopLengthSamples = (beats * beatDurationMs / 1000.0) * trackSR;
      } else {
        int endIdx = nearestIdx + beatsCount;
        if (endIdx < ds->LoadedTrack->BeatGridCount) {
          double endMs = ds->LoadedTrack->BeatGrid[endIdx].Time;
          loopLengthSamples =
              ((endMs - ds->LoadedTrack->BeatGrid[nearestIdx].Time) / 1000.0) *
              trackSR;
        } else {
          // Fallback to BPM
          double beatDurationMs =
              60000.0 / (ds->CurrentBPM > 0 ? ds->CurrentBPM : 120.0);
          loopLengthSamples = (beats * beatDurationMs / 1000.0) * trackSR;
        }
      }
    } else {
      // Standard BPM based loop
      double beatDurationMs =
          60000.0 / (ds->CurrentBPM > 0 ? ds->CurrentBPM : 120.0);
      loopLengthSamples = (beats * beatDurationMs / 1000.0) * trackSR;
    }

    if (mode == PAD_MODE_SLIP_LOOP) {
      DeckAudio_SetSlip(audio, true);
    } else {
      a->padState.ActiveLoopIdx[deckIdx] = padIdx;
    }

    DeckAudio_SetLoop(audio, true, startPos, startPos + loopLengthSamples);

    // For NEW loops, jump to start. For resizing, we don't jump so the playback
    // phase is preserved.
    if (!isResizing) {
      audio->Position = startPos;
      audio->MT_ReadPos = startPos;
      DeckAudio_ClearMT(audio);
    } else {
      // If resizing a normal loop, and we are now beyond the new endPos, wrap
      // immediately
      if (audio->Position >= startPos + loopLengthSamples) {
        audio->Position = startPos;
        audio->MT_ReadPos = startPos;
        DeckAudio_ClearMT(audio);
      }
    }
  } else if (mode == PAD_MODE_BEAT_JUMP) {
    // Left 4 pads: Jump Back, Right 4 pads: Jump Forward (4, 8, 16, 32)
    static float jumpBeats[] = {-4.0f, -8.0f, -16.0f, -32.0f,
                                4.0f,  8.0f,  16.0f,  32.0f};
    float beats = jumpBeats[padIdx];
    float beatDurationMs =
        60000.0f / (ds->CurrentBPM > 0 ? ds->CurrentBPM : 120.0f);

    double trackSR = (double)audio->SampleRate;
    if (trackSR < 100)
      trackSR = 44100.0;

    // Perform jump IMMEDIATELY on engine for maximum responsiveness
    double currentMs = (audio->Position / trackSR) * 1000.0;
    DeckAudio_JumpToMs(audio, (int64_t)(currentMs + (beats * beatDurationMs)));

    a->padState.ActiveLoopIdx[deckIdx] = -1;
  } else if (mode == PAD_MODE_GATE_CUE) {
    if (padIdx < ds->LoadedTrack->HotCuesCount) {
      HotCue hc = ds->LoadedTrack->HotCues[padIdx];
      ds->SeekMs = hc.Start;
      ds->HasSeekRequest = true;
      DeckAudio_InstantPlay(audio);
    }
  } else if (mode == PAD_MODE_RELEASE_FX) {
    if (padIdx == 0)
      DeckAudio_TriggerReleaseFX(audio, 1); // Brake S
    else if (padIdx == 1)
      DeckAudio_TriggerReleaseFX(audio, 2); // Brake L
    else if (padIdx == 2)
      DeckAudio_TriggerReleaseFX(audio, 3); // Spin S
    else if (padIdx == 3)
      DeckAudio_TriggerReleaseFX(audio, 4); // Spin L
    else if (padIdx >= 4 && padIdx <= 6) {
      // Echo Out
      static int ePadIdx[] = {2, 4,
                              5}; // Indices for 1/2, 1, 2 beats in XPadRatios
      a->fxState.SelectedChannel = deckIdx + 1;
      a->fxState.IsFXOn = true;
      a->fxState.SelectedFX = 1; // ECHO (BEATFX_ECHO)
      a->fxState.SelectedPad = ePadIdx[padIdx - 4];
      a->fxState.LevelDepth = 0.2f;
      DeckAudio_TriggerReleaseFX(audio, 5); // Stop deck
    } else if (padIdx == 7) {
      // Mute / Instant Stop
      DeckAudio_Stop(audio);
      audio->VinylStopAccel = 1.0f;
    }
  }
}

void OnPadRelease(void *ctx, int deckIdx, int padIdx) {
  (void)padIdx;

  App *a = (App *)ctx;
  DeckAudioState *audio = &globalAudioEngine->Decks[deckIdx];
  PadMode mode = a->padState.Mode[deckIdx];

  if (mode == PAD_MODE_SLIP_LOOP) {
    DeckAudio_ExitLoop(audio);
    DeckAudio_SetSlip(audio, false);
    a->padState.ActiveLoopIdx[deckIdx] = -1;
  } else if (mode == PAD_MODE_GATE_CUE) {
    DeckAudio_Stop(audio);
  }
}

void TopBar_OnSettings(void *ctx) {
  App *a = (App *)ctx;
  if (a->screen == ScreenSettings) {
    a->screen = ScreenPlayer;
    a->settingsState.IsActive = false;
  } else {
    a->screen = ScreenSettings;
    a->settingsState.IsActive = true;
  }
}

void App_Init(App *a) {
  UNX_LOG_INFO("[APP] App_Init starting...");
#if defined(__ANDROID__) || defined(PLATFORM_IOS)
  SetGesturesEnabled(GESTURE_PINCH_IN | GESTURE_PINCH_OUT);
#endif
  a->screen = ScreenSplash;
  a->splashCounter = 120; // 2 seconds at 60 FPS

  // Init Deck States
  UNX_LOG_INFO("[APP] Initializing Decks...");
  memset(&a->deckA, 0, sizeof(DeckState));
  a->deckA.ID = 0;
  strcpy(a->deckA.SourceName, "USB1");
  a->deckA.PositionMs = 0;
  a->deckA.TrackLengthMs = 0;
  a->deckA.IsMaster = true;
  a->deckA.VinylModeEnabled = true;
  a->deckA.MasterTempo = false;
  a->deckA.ZoomScale = 2.0f;

  memset(&a->deckB, 0, sizeof(DeckState));
  a->deckB.ID = 1;
  strcpy(a->deckB.SourceName, "USB1");
  a->deckB.PositionMs = 0;
  a->deckB.TrackLengthMs = 0;
  a->deckB.VinylModeEnabled = true;
  a->deckB.MasterTempo = false;
  a->deckB.ZoomScale = 2.0f;

  // Init Browser State
  UNX_LOG_INFO("[APP] Initializing Browser...");
  memset(&a->browserState, 0, sizeof(BrowserState));
  a->browserState.IsActive = false;
  a->browserState.BrowseLevel = 3; // Source level
  for (int i = 0; i < 3; i++)
    a->browserState.PlaylistBank[i].PlaylistIdx = -1;

  Browser_RefreshStorages(&a->browserState);
  UNX_LOG_INFO("[APP] App_Init completed.");

  // Init Settings State
  memset(&a->settingsState, 0, sizeof(SettingsState));
  a->settingsState.IsActive = false;
  a->settingsState.ItemsCount = 8;

  strcpy(a->settingsState.Items[0].Label, "PLAY MODE");
  strcpy(a->settingsState.Items[0].Options[0], "CONTINUE");
  strcpy(a->settingsState.Items[0].Options[1], "SINGLE");
  a->settingsState.Items[0].OptionsCount = 2;
  a->settingsState.Items[0].Category = SETTING_CAT_DECK;

  strcpy(a->settingsState.Items[1].Label, "LOAD LOCK");
  strcpy(a->settingsState.Items[1].Options[0], "OFF");
  strcpy(a->settingsState.Items[1].Options[1], "ON");
  a->settingsState.Items[1].OptionsCount = 2;
  a->settingsState.Items[1].Category = SETTING_CAT_DECK;

  // Load persisted settings
  Settings_Load(&a->deckA.Waveform, &a->deckB.Waveform, &a->activeAudioConfig,
                a->activeControllerPath);
  if (a->activeControllerPath[0] != '\0') {
    MIDI_RefreshMapping(a->activeControllerPath);
  }

  strcpy(a->settingsState.Items[2].Label, "WFM STYLE");
  strcpy(a->settingsState.Items[2].Options[0], "BLUE");
  strcpy(a->settingsState.Items[2].Options[1], "RGB");
  strcpy(a->settingsState.Items[2].Options[2], "3-BAND");
  a->settingsState.Items[2].OptionsCount = 3;
  a->settingsState.Items[2].Current = a->deckA.Waveform.Style;
  a->settingsState.Items[2].Category = SETTING_CAT_VIEW;

  strcpy(a->settingsState.Items[3].Label, "WFM LOW GAIN");
  a->settingsState.Items[3].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[3].OptionsCount = 25; // 0.1 to 2.5
  for (int i = 0; i < 25; i++) {
    float v = 0.1f + (i * 0.1f);
    sprintf(a->settingsState.Items[3].Options[i], "%.1fx", v);
    if (fabs(a->deckA.Waveform.GainLow - v) < 0.05f)
      a->settingsState.Items[3].Current = i;
  }
  a->settingsState.Items[3].Category = SETTING_CAT_VIEW;

  strcpy(a->settingsState.Items[4].Label, "WFM MID GAIN");
  a->settingsState.Items[4].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[4].OptionsCount = 25;
  for (int i = 0; i < 25; i++) {
    float v = 0.1f + (i * 0.1f);
    sprintf(a->settingsState.Items[4].Options[i], "%.1fx", v);
    if (fabs(a->deckA.Waveform.GainMid - v) < 0.05f)
      a->settingsState.Items[4].Current = i;
  }
  a->settingsState.Items[4].Category = SETTING_CAT_VIEW;

  strcpy(a->settingsState.Items[5].Label, "WFM HIGH GAIN");
  a->settingsState.Items[5].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[5].OptionsCount = 25;
  for (int i = 0; i < 25; i++) {
    float v = 0.1f + (i * 0.1f);
    sprintf(a->settingsState.Items[5].Options[i], "%.1fx", v);
    if (fabs(a->deckA.Waveform.GainHigh - v) < 0.05f)
      a->settingsState.Items[5].Current = i;
  }
  a->settingsState.Items[5].Category = SETTING_CAT_VIEW;

  strcpy(a->settingsState.Items[6].Label, "JOG START TIME");
  a->settingsState.Items[6].Type = SETTING_TYPE_KNOB;
  a->settingsState.Items[6].Min = 0;
  a->settingsState.Items[6].Max = 2000;
  a->settingsState.Items[6].Value = a->deckA.Waveform.VinylStartMs;
  strcpy(a->settingsState.Items[6].Unit, "ms");
  a->settingsState.Items[6].Category = SETTING_CAT_DECK;

  strcpy(a->settingsState.Items[7].Label, "JOG RELEASE TIME");
  a->settingsState.Items[7].Type = SETTING_TYPE_KNOB;
  a->settingsState.Items[7].Min = 0;
  a->settingsState.Items[7].Max = 2000;
  a->settingsState.Items[7].Value = a->deckA.Waveform.VinylStopMs;
  strcpy(a->settingsState.Items[7].Unit, "ms");
  a->settingsState.Items[7].Category = SETTING_CAT_DECK;

  // --- Audio Configurations ---
  AudioDeviceInfo devs[MAX_AUDIO_DEVICES];
  int devCount = AudioBackend_GetDevices(devs, MAX_AUDIO_DEVICES);

  strcpy(a->settingsState.Items[8].Label, "AUDIO DEVICE");
  a->settingsState.Items[8].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[8].OptionsCount = devCount + 1;
  strcpy(a->settingsState.Items[8].Options[0], "System Default");
  for (int i = 0; i < devCount && i < 31; i++) {
    // 31 characters limit for options. Format: "2CH Output Name"
    snprintf(a->settingsState.Items[8].Options[i + 1], 32, "%dCH %s",
             devs[i].NativeChannels, devs[i].Name);
  }
  a->settingsState.Items[8].Category = SETTING_CAT_AUDIO;

  // Initial population based on loaded config
  UpdateChannelOptions(a, a->activeAudioConfig.DeviceIndex);

  // Back-sync from loaded config to UI selection items
  a->settingsState.Items[8].Current = a->activeAudioConfig.DeviceIndex + 1;
  a->settingsState.Items[9].Current = a->activeAudioConfig.MasterOutL;
  a->settingsState.Items[10].Current = a->activeAudioConfig.MasterOutR;
  a->settingsState.Items[11].Current = a->activeAudioConfig.CueOutL + 1;
  a->settingsState.Items[12].Current = a->activeAudioConfig.CueOutR + 1;

  int bufMap[] = {128, 256, 512, 1024};
  a->settingsState.Items[13].Current = 1; // Default 256
  for (int i = 0; i < 4; i++) {
    if (a->activeAudioConfig.BufferSizeFrames == bufMap[i])
      a->settingsState.Items[13].Current = i;
  }
  a->settingsState.Items[14].Current =
      (a->activeAudioConfig.SampleRate == 44100) ? 0 : 1;

  strcpy(a->settingsState.Items[13].Label, "BUFFER SIZE");
  a->settingsState.Items[13].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[13].OptionsCount = 4;
  strcpy(a->settingsState.Items[13].Options[0], "128");
  strcpy(a->settingsState.Items[13].Options[1], "256");
  strcpy(a->settingsState.Items[13].Options[2], "512");
  strcpy(a->settingsState.Items[13].Options[3], "1024");
  a->settingsState.Items[13].Category = SETTING_CAT_AUDIO;

  strcpy(a->settingsState.Items[14].Label, "SAMPLE RATE");
  a->settingsState.Items[14].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[14].OptionsCount = 2;
  strcpy(a->settingsState.Items[14].Options[0], "44100 Hz");
  strcpy(a->settingsState.Items[14].Options[1], "48000 Hz");
  a->settingsState.Items[14].Category = SETTING_CAT_AUDIO;

  // Sync again to make sure labels/options are set before setting Current
  a->settingsState.Items[13].Current = 1; // Default 256
  for (int i = 0; i < 4; i++) {
    if (a->activeAudioConfig.BufferSizeFrames == bufMap[i])
      a->settingsState.Items[13].Current = i;
  }
  a->settingsState.Items[14].Current =
      (a->activeAudioConfig.SampleRate == 44100) ? 0 : 1;

  strcpy(a->settingsState.Items[15].Label, "PCM BIT DEPTH");
  a->settingsState.Items[15].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[15].OptionsCount = 2;
  strcpy(a->settingsState.Items[15].Options[0], "16-bit (RAM Save)");
  strcpy(a->settingsState.Items[15].Options[1], "24-bit (Quality)");
  a->settingsState.Items[15].Current =
      (a->activeAudioConfig.PCMBitDepth == 24) ? 1 : 0;
  a->settingsState.Items[15].Category = SETTING_CAT_AUDIO;

  strcpy(a->settingsState.Items[16].Label, "ABOUT");
  a->settingsState.Items[16].Type = SETTING_TYPE_ACTION;
  a->settingsState.Items[16].Category = SETTING_CAT_SYSTEM;

  strcpy(a->settingsState.Items[17].Label, "CREDITS");
  a->settingsState.Items[17].Type = SETTING_TYPE_ACTION;
  a->settingsState.Items[17].Category = SETTING_CAT_SYSTEM;

  strcpy(a->settingsState.Items[18].Label, "EXIT APPLICATION");
  a->settingsState.Items[18].Type = SETTING_TYPE_ACTION;
  a->settingsState.Items[18].Category = SETTING_CAT_SYSTEM;
  a->settingsState.ItemsCount = 19;

  // Set Load Lock current opt
  a->settingsState.Items[1].Current = a->deckA.Waveform.LoadLock ? 1 : 0;

  // Init Info State
  memset(&a->infoState, 0, sizeof(InfoState));
  a->infoState.IsActive = false;

  // Init FX State
  memset(&a->fxState, 0, sizeof(BeatFXState));
  a->fxState.LevelDepth = 0.5f;

  // Init About State
  memset(&a->aboutState, 0, sizeof(AboutState));
  a->aboutState.IsActive = false;
  strcpy(a->aboutState.Version, APP_VERSION);
  strcpy(a->aboutState.Developer, APP_DEVELOPER);
  strcpy(a->aboutState.Instagram, APP_INSTAGRAM);

  // Init Components
  TopBar_Init(&a->topbar);
  a->topbar.callbackCtx = a;
  a->topbar.OnBrowse = TopBar_OnBrowse;
  a->topbar.OnMixer = TopBar_OnMixer;
  a->topbar.OnInfo = TopBar_OnInfo;
  a->topbar.OnSettings = TopBar_OnSettings;
  a->topbar.OnPad = TopBar_OnPad;

  DeckStrip_Init(&a->stripA, 0, &a->deckA);
  DeckStrip_Init(&a->stripB, 1, &a->deckB);
  PlayerRenderer_Init(&a->player, &a->deckA, &a->deckB, &a->fxState, NULL);
  BrowserRenderer_Init(&a->browser, &a->browserState);
  InfoRenderer_Init(&a->info, &a->infoState);
  SettingsRenderer_Init(&a->settings, &a->settingsState);
  a->settings.OnApply = OnSettingsApply;
  a->settings.OnClose = OnSettingsClose;
  a->settings.OnAction = OnSettingsAction;
  a->settings.OnValueChanged = OnSettingsValueChanged;
  a->settings.callbackCtx = a;

  // Settings_Load (at line 360) already populated this.
  // We only set hardcoded defaults if you want a fallback before loading.

  AboutRenderer_Init(&a->about, &a->aboutState);
  CreditsRenderer_Init(&a->credits, &a->creditsState);
  a->mixerState.FXState = &a->fxState;
  MixerRenderer_Init(&a->mixer, &a->mixerState);

  // Init Pad View
  memset(&a->padState, 0, sizeof(PadState));
  a->padState.Decks[0] = &a->deckA;
  a->padState.Decks[1] = &a->deckB;
  a->padState.ActiveLoopIdx[0] = -1;
  a->padState.ActiveLoopIdx[1] = -1;
  PadRenderer_Init(&a->pad, &a->padState);
  a->pad.OnPadPress = OnPadPress;
  a->pad.OnPadRelease = OnPadRelease;
  a->pad.callbackCtx = a;

  SplashRenderer_Init(&a->splash, &a->splashCounter);
  a->keyMap = GetDefaultMapping();
  memset(&a->midiCtx, 0, sizeof(MidiContext));

  PopulateMidiSettings(a);
}

#if defined(PLATFORM_IOS)
int raylib_main(int argc, char *argv[]) {
#else
int main(void) {
#endif
  SetTraceLogLevel(LOG_ALL);
  Log_Init();
  UNX_LOG_INFO("!!! [DEBUG] ENTRY POINT main() !!!");

#if defined(_WIN32)
  // Disable QuickEdit mode to prevent application from freezing when console is
  // clicked
  HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
  DWORD prev_mode;
  if (GetConsoleMode(hInput, &prev_mode)) {
    SetConsoleMode(hInput, prev_mode & ~ENABLE_QUICK_EDIT_MODE);
  }
#endif

  // Standard 1080p 16:9 Resolution (iPhone 8 Plus Native)
#if defined(_WIN32)
  // int startWidth = 1280;
  // int startHeight = 720;
#else
  // int startWidth = 1920;
  // int startHeight = 1080;
#endif

#if !defined(PLATFORM_IOS)
  printf("[MAIN] Application starting...\n");
  UNX_LOG_INFO("Application starting...");
#endif

#if defined(PLATFORM_DRM)
  // ... (DRM logic omitted for brevity in replace)
#elif defined(__ANDROID__)
  UNX_LOG_INFO("[MAIN] Platform: ANDROID. Attempting InitWindow...");
  int w = GetScreenWidth();
  int h = GetScreenHeight();
  if (w <= 0)
    w = 1920; // Fallback to common resolution
  if (h <= 0)
    h = 1080;
  InitWindow(w, h, APP_NAME);
  UNX_LOG_INFO("[MAIN] InitWindow finished. Result: %s",
               IsWindowReady() ? "SUCCESS" : "FAILED");
  SetTargetFPS(60);
#else
// Desktop (X11, Wayland, Windows)
printf("[MAIN] Platform: DESKTOP (X11/Wayland/Windows)\n");
SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);

// Start in windowed mode (e.g., 1280x720) like Windows version
int winWidth = 1280;
int winHeight = 720;

printf("[MAIN] Initializing Window (%dx%d)...\n", winWidth, winHeight);
InitWindow(winWidth, winHeight, APP_NAME);

if (IsWindowReady()) {
  printf("[MAIN] InitWindow SUCCESS. Window size: %dx%d\n", GetScreenWidth(),
         GetScreenHeight());
  UNX_LOG_INFO("[DESKTOP] InitWindow SUCCESS.");
} else {
  printf("[MAIN] InitWindow FAILED!\n");
  UNX_LOG_ERR("[DESKTOP] InitWindow FAILED!");
}

SetWindowMinSize(REF_WIDTH, REF_HEIGHT);
SetTargetFPS(60);
#endif

  SetExitKey(KEY_NULL); // ESC is for 'back'

#if defined(__ANDROID__)
  UNX_LOG_INFO("[MAIN] Stability delay for hardware driver...");
  usleep(200000); // 200ms delay
#endif

  UNX_LOG_INFO("[MAIN] Initializing Fonts...");
  UIFonts_Init();
  UNX_LOG_INFO("[MAIN] Fonts initialized.");

#if defined(PLATFORM_IOS)
  extern const char *ios_get_documents_path(const char *filename);
  UNX_LOG_INFO("[MAIN] iOS Stability delay...");
  usleep(500000); // 500ms delay to ensure surface is ready
#endif

#if defined(__ANDROID__)
  UNX_LOG_INFO("[MAIN] Disabling MSAA for legacy driver compatibility...");
  // ClearWindowState is more standard for runtime changes if supported
  ClearWindowState(FLAG_MSAA_4X_HINT);
  usleep(100000); // 100ms
#endif

  // Initialize Audio Backend FIRST so App_Init can enumerate real devices
  UNX_LOG_INFO("[MAIN] Initializing Audio Backend...");
  AudioBackend_Init();
  UNX_LOG_INFO("[MAIN] Audio Backend initialized.");

  UNX_LOG_INFO("[MAIN] Initializing App (Heap)...");
  UNX_LOG_INFO("[MAIN] Structure Sizes - App: %.2f MB, AudioEngine: %.2f MB",
               (float)sizeof(App) / (1024.0f * 1024.0f),
               (float)sizeof(AudioEngine) / (1024.0f * 1024.0f));

  App *app = (App *)malloc(sizeof(App));
  if (!app) {
    UNX_LOG_ERR("[CRITICAL] Failed to allocate App on heap!");
    return -1;
  }
  memset(app, 0, sizeof(App));
  globalApp = app;

#ifdef DEBUG_IOS_GUI
  app->screen = ScreenDebug;
  DebugIOS_Init(&app->debugView);
  UNX_LOG_INFO("[MAIN] Debug GUI Mode Active. Skipping heavy init.");
#if defined(PLATFORM_IOS)
  return 0; // Return early only on iOS, ios_ready will handle the rest
#endif
#endif

  App_Init(app);

  MIDI_Init(&app->midiCtx);

  int bufMap[] = {128, 256, 512, 1024};
  AudioBackendConfig initialAudioCfg = {
      .DeviceIndex = app->settingsState.Items[8].Current - 1,
      .MasterOutL = app->settingsState.Items[9].Current,
      .MasterOutR = app->settingsState.Items[10].Current,
      .CueOutL = app->settingsState.Items[11].Current - 1,
      .CueOutR = app->settingsState.Items[12].Current - 1,
      .SampleRate = (app->settingsState.Items[14].Current == 0) ? 44100 : 48000,
      .BufferSizeFrames = bufMap[app->settingsState.Items[13].Current]};

#if defined(PLATFORM_IOS)
  // Force safer defaults for iOS to prevent driver instability/crashes
  initialAudioCfg.SampleRate = 44100;
  initialAudioCfg.BufferSizeFrames = 512;
#endif

  app->activeAudioConfig = initialAudioCfg;
  UNX_LOG_INFO("[MAIN] Audio config prepared. SR: %d, Buf: %d",
               initialAudioCfg.SampleRate, initialAudioCfg.BufferSizeFrames);
  UNX_LOG_INFO("[MAIN] Retrieving active audio info...");
  // Set initial Audio Driver name for the UI
  AudioBackend_GetActiveInfo(NULL, NULL, app->aboutState.AudioDriver,
                             app->aboutState.AudioDevice);

  UNX_LOG_INFO("[MAIN] Initializing Audio Engine (Heap)...");

  AudioEngine *audioEngine = (AudioEngine *)malloc(sizeof(AudioEngine));
  if (!audioEngine) {
    UNX_LOG_ERR("[CRITICAL] Failed to allocate AudioEngine on heap!");
    return -1;
  }
  AudioEngine_Init(audioEngine, initialAudioCfg.SampleRate);
  app->browserState.AudioPlugin = (struct AudioEngine *)audioEngine;
  app->browserState.DeckA = (struct DeckState *)&app->deckA;
  app->browserState.DeckB = (struct DeckState *)&app->deckB;
  app->player.AudioPlugin = audioEngine;
  app->player.InfoA.Engine = audioEngine;
  app->player.InfoB.Engine = audioEngine;
  app->player.BeatFX.AudioPlugin = audioEngine;
  app->player.FXBar.AudioPlugin = audioEngine;

  UNX_LOG_INFO("[MAIN] Audio Engine initialized. Total RAM: %.2f MB",
               Log_GetRAMUsage());

  // Register Controls after Init
  CO_Init();
  CO_Register("[Channel1]", "play", CO_TYPE_BOOL,
              &audioEngine->Decks[0].IsMotorOn, 0, 1);
  CO_Register("[Channel1]", "volume", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].Trim, 0, 2.0f);
  CO_Register("[Channel1]", "filterHigh", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqHigh, 0, 1.0f);
  CO_Register("[Channel1]", "filterMid", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqMid, 0, 1.0f);
  CO_Register("[Channel1]", "filterLow", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqLow, 0, 1.0f);
  CO_Register("[Channel1]", "cue_default", CO_TYPE_BOOL,
              &audioEngine->Decks[0].IsCueActive, 0, 1);
  CO_Register("[Channel1]", "pfl", CO_TYPE_BOOL,
              &audioEngine->Decks[0].IsCueActive, 0, 1);
  CO_Register("[Channel1]", "fader", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].Fader, 0, 1.0f);
  CO_Register("[Channel1]", "jog", CO_TYPE_FLOAT, &app->deckA.JogDelta, -100.0f,
              100.0f);
  CO_Register("[Channel1]", "loadA", CO_TYPE_BOOL, &app->deckA.IsLoading, 0, 1);
  CO_Register("[Channel1]", "slip", CO_TYPE_BOOL,
              &audioEngine->Decks[0].SlipActive, 0, 1);
  CO_Register("[Channel1]", "vinyl_mode", CO_TYPE_BOOL,
              &app->deckA.VinylModeEnabled, 0, 1);
  CO_Register("[Channel1]", "master_tempo", CO_TYPE_BOOL,
              &app->deckA.MasterTempo, 0, 1);
  CO_Register("[Channel1]", "quantize", CO_TYPE_BOOL,
              &app->deckA.QuantizeEnabled, 0, 1);
  CO_Register("[Channel1]", "sync", CO_TYPE_BOOL, &app->deckA.MidiRequestSync,
              0, 1);
  CO_Register("[Channel1]", "master", CO_TYPE_BOOL,
              &app->deckA.MidiRequestMaster, 0, 1);

  // Loops
  CO_Register("[Channel1]", "loop_in", CO_TYPE_BOOL,
              &app->deckA.MidiRequestLoopIn, 0, 1);
  CO_Register("[Channel1]", "loop_out", CO_TYPE_BOOL,
              &app->deckA.MidiRequestLoopOut, 0, 1);
  CO_Register("[Channel1]", "loop_exit", CO_TYPE_BOOL,
              &app->deckA.MidiRequestLoopExit, 0, 1);
  CO_Register("[Channel1]", "loop_halve", CO_TYPE_BOOL,
              &app->deckA.MidiRequestLoopHalve, 0, 1);
  CO_Register("[Channel1]", "loop_double", CO_TYPE_BOOL,
              &app->deckA.MidiRequestLoopDouble, 0, 1);

  // Pitch Bend
  CO_Register("[Channel1]", "pitch_bend_plus", CO_TYPE_BOOL,
              &app->deckA.MidiRequestPitchBendPlus, 0, 1);
  CO_Register("[Channel1]", "pitch_bend_minus", CO_TYPE_BOOL,
              &app->deckA.MidiRequestPitchBendMinus, 0, 1);

  // Hot Cues 1-8
  CO_Register("[Channel1]", "hotcue_1", CO_TYPE_BOOL,
              &app->deckA.MidiRequestHotCue[0], 0, 1);
  CO_Register("[Channel1]", "hotcue_2", CO_TYPE_BOOL,
              &app->deckA.MidiRequestHotCue[1], 0, 1);
  CO_Register("[Channel1]", "hotcue_3", CO_TYPE_BOOL,
              &app->deckA.MidiRequestHotCue[2], 0, 1);
  CO_Register("[Channel1]", "hotcue_4", CO_TYPE_BOOL,
              &app->deckA.MidiRequestHotCue[3], 0, 1);
  CO_Register("[Channel1]", "hotcue_5", CO_TYPE_BOOL,
              &app->deckA.MidiRequestHotCue[4], 0, 1);
  CO_Register("[Channel1]", "hotcue_6", CO_TYPE_BOOL,
              &app->deckA.MidiRequestHotCue[5], 0, 1);
  CO_Register("[Channel1]", "hotcue_7", CO_TYPE_BOOL,
              &app->deckA.MidiRequestHotCue[6], 0, 1);
  CO_Register("[Channel1]", "hotcue_8", CO_TYPE_BOOL,
              &app->deckA.MidiRequestHotCue[7], 0, 1);

  // Beat Jump & Auto Loop
  CO_Register("[Channel1]", "beatjump_forward", CO_TYPE_BOOL,
              &app->deckA.MidiRequestBeatJumpForward, 0, 1);
  CO_Register("[Channel1]", "beatjump_backward", CO_TYPE_BOOL,
              &app->deckA.MidiRequestBeatJumpBackward, 0, 1);
  CO_Register("[Channel1]", "autoloop_1", CO_TYPE_BOOL,
              &app->deckA.MidiRequestAutoLoop[0], 0, 1);
  CO_Register("[Channel1]", "autoloop_2", CO_TYPE_BOOL,
              &app->deckA.MidiRequestAutoLoop[1], 0, 1);
  CO_Register("[Channel1]", "autoloop_4", CO_TYPE_BOOL,
              &app->deckA.MidiRequestAutoLoop[2], 0, 1);
  CO_Register("[Channel1]", "autoloop_8", CO_TYPE_BOOL,
              &app->deckA.MidiRequestAutoLoop[3], 0, 1);
  CO_Register("[Channel1]", "autoloop_16", CO_TYPE_BOOL,
              &app->deckA.MidiRequestAutoLoop[4], 0, 1);

  CO_Register("[Channel2]", "play", CO_TYPE_BOOL,
              &audioEngine->Decks[1].IsMotorOn, 0, 1);
  CO_Register("[Channel2]", "volume", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].Trim, 0, 2.0f);
  CO_Register("[Channel2]", "filterHigh", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqHigh, 0, 1.0f);
  CO_Register("[Channel2]", "filterMid", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqMid, 0, 1.0f);
  CO_Register("[Channel2]", "filterLow", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqLow, 0, 1.0f);
  CO_Register("[Channel2]", "cue_default", CO_TYPE_BOOL,
              &audioEngine->Decks[1].IsCueActive, 0, 1);
  CO_Register("[Channel2]", "pfl", CO_TYPE_BOOL,
              &audioEngine->Decks[1].IsCueActive, 0, 1);
  CO_Register("[Channel2]", "fader", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].Fader, 0, 1.0f);
  CO_Register("[Channel2]", "jog", CO_TYPE_FLOAT, &app->deckB.JogDelta, -100.0f,
              100.0f);
  CO_Register("[Channel2]", "loadB", CO_TYPE_BOOL, &app->deckB.IsLoading, 0, 1);
  CO_Register("[Channel2]", "slip", CO_TYPE_BOOL,
              &audioEngine->Decks[1].SlipActive, 0, 1);
  CO_Register("[Channel2]", "vinyl_mode", CO_TYPE_BOOL,
              &app->deckB.VinylModeEnabled, 0, 1);
  CO_Register("[Channel2]", "master_tempo", CO_TYPE_BOOL,
              &app->deckB.MasterTempo, 0, 1);
  CO_Register("[Channel2]", "quantize", CO_TYPE_BOOL,
              &app->deckB.QuantizeEnabled, 0, 1);
  CO_Register("[Channel2]", "sync", CO_TYPE_BOOL, &app->deckB.MidiRequestSync,
              0, 1);
  CO_Register("[Channel2]", "master", CO_TYPE_BOOL,
              &app->deckB.MidiRequestMaster, 0, 1);

  // Loops
  CO_Register("[Channel2]", "loop_in", CO_TYPE_BOOL,
              &app->deckB.MidiRequestLoopIn, 0, 1);
  CO_Register("[Channel2]", "loop_out", CO_TYPE_BOOL,
              &app->deckB.MidiRequestLoopOut, 0, 1);
  CO_Register("[Channel2]", "loop_exit", CO_TYPE_BOOL,
              &app->deckB.MidiRequestLoopExit, 0, 1);
  CO_Register("[Channel2]", "loop_halve", CO_TYPE_BOOL,
              &app->deckB.MidiRequestLoopHalve, 0, 1);
  CO_Register("[Channel2]", "loop_double", CO_TYPE_BOOL,
              &app->deckB.MidiRequestLoopDouble, 0, 1);

  // Pitch Bend
  CO_Register("[Channel2]", "pitch_bend_plus", CO_TYPE_BOOL,
              &app->deckB.MidiRequestPitchBendPlus, 0, 1);
  CO_Register("[Channel2]", "pitch_bend_minus", CO_TYPE_BOOL,
              &app->deckB.MidiRequestPitchBendMinus, 0, 1);

  // Hot Cues 1-8
  CO_Register("[Channel2]", "hotcue_1", CO_TYPE_BOOL,
              &app->deckB.MidiRequestHotCue[0], 0, 1);
  CO_Register("[Channel2]", "hotcue_2", CO_TYPE_BOOL,
              &app->deckB.MidiRequestHotCue[1], 0, 1);
  CO_Register("[Channel2]", "hotcue_3", CO_TYPE_BOOL,
              &app->deckB.MidiRequestHotCue[2], 0, 1);
  CO_Register("[Channel2]", "hotcue_4", CO_TYPE_BOOL,
              &app->deckB.MidiRequestHotCue[3], 0, 1);
  CO_Register("[Channel2]", "hotcue_5", CO_TYPE_BOOL,
              &app->deckB.MidiRequestHotCue[4], 0, 1);
  CO_Register("[Channel2]", "hotcue_6", CO_TYPE_BOOL,
              &app->deckB.MidiRequestHotCue[5], 0, 1);
  CO_Register("[Channel2]", "hotcue_7", CO_TYPE_BOOL,
              &app->deckB.MidiRequestHotCue[6], 0, 1);
  CO_Register("[Channel2]", "hotcue_8", CO_TYPE_BOOL,
              &app->deckB.MidiRequestHotCue[7], 0, 1);

  // Beat Jump & Auto Loop
  CO_Register("[Channel2]", "beatjump_forward", CO_TYPE_BOOL,
              &app->deckB.MidiRequestBeatJumpForward, 0, 1);
  CO_Register("[Channel2]", "beatjump_backward", CO_TYPE_BOOL,
              &app->deckB.MidiRequestBeatJumpBackward, 0, 1);
  CO_Register("[Channel2]", "autoloop_1", CO_TYPE_BOOL,
              &app->deckB.MidiRequestAutoLoop[0], 0, 1);
  CO_Register("[Channel2]", "autoloop_2", CO_TYPE_BOOL,
              &app->deckB.MidiRequestAutoLoop[1], 0, 1);
  CO_Register("[Channel2]", "autoloop_4", CO_TYPE_BOOL,
              &app->deckB.MidiRequestAutoLoop[2], 0, 1);
  CO_Register("[Channel2]", "autoloop_8", CO_TYPE_BOOL,
              &app->deckB.MidiRequestAutoLoop[3], 0, 1);
  CO_Register("[Channel2]", "autoloop_16", CO_TYPE_BOOL,
              &app->deckB.MidiRequestAutoLoop[4], 0, 1);
  CO_Register("[Master]", "crossfader", CO_TYPE_FLOAT, &audioEngine->Crossfader,
              -1.0f, 1.0f);
  CO_Register("[Master]", "volume", CO_TYPE_FLOAT, &audioEngine->MasterVolume,
              0, 1.0f);

  // --- Color FX ---
  CO_Register("[Channel1]", "colorfx_select", CO_TYPE_INT,
              &audioEngine->Decks[0].ColorFX.activeFX, 0, 6);
  CO_Register("[Channel1]", "colorfx_parameter", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].ColorFX.parameter, 0, 1.0f);
  CO_Register("[Channel1]", "colorfx_value", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].ColorFX.colorValue, -1.0f, 1.0f);
  CO_Register("[Channel2]", "colorfx_select", CO_TYPE_INT,
              &audioEngine->Decks[1].ColorFX.activeFX, 0, 6);
  CO_Register("[Channel2]", "colorfx_parameter", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].ColorFX.parameter, 0, 1.0f);
  CO_Register("[Channel2]", "colorfx_value", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].ColorFX.colorValue, -1.0f, 1.0f);

  // --- Beat FX ---
  CO_Register("[Master]", "beatfx_select", CO_TYPE_INT,
              &audioEngine->BeatFX.activeFX, 0, 13);
  CO_Register("[Master]", "beatfx_drywet", CO_TYPE_FLOAT,
              &audioEngine->BeatFX.levelDepth, 0, 1.0f);
  CO_Register("[Master]", "beatfx_time", CO_TYPE_FLOAT,
              &audioEngine->BeatFX.beatMs, 0, 2000.0f);
  CO_Register("[Master]", "beatfx_on", CO_TYPE_BOOL,
              &audioEngine->BeatFX.isFxOn, 0, 1);
  CO_Register("[Master]", "beatfx_channel", CO_TYPE_INT,
              &audioEngine->BeatFX.targetChannel, 0, 2);

  // --- Library / Browser ---
  CO_Register("[Library]", "browse", CO_TYPE_FLOAT,
              &app->browserState.MidiBrowseDelta, -10.0f, 10.0f);
  CO_Register("[Library]", "enter", CO_TYPE_BOOL,
              &app->browserState.MidiRequestEnter, 0, 1);
  CO_Register("[Library]", "back", CO_TYPE_BOOL,
              &app->browserState.MidiRequestBack, 0, 1);
  CO_Register("[Library]", "up", CO_TYPE_BOOL, &app->browserState.MidiRequestUp,
              0, 1);
  CO_Register("[Library]", "down", CO_TYPE_BOOL,
              &app->browserState.MidiRequestDown, 0, 1);
  CO_Register("[Library]", "loadA", CO_TYPE_BOOL,
              &app->browserState.MidiRequestLoadA, 0, 1);
  CO_Register("[Library]", "loadB", CO_TYPE_BOOL,
              &app->browserState.MidiRequestLoadB, 0, 1);

  // --- Settings Navigation ---
  CO_Register("[Settings]", "browse", CO_TYPE_FLOAT,
              &app->settingsState.MidiBrowseDelta, -10.0f, 10.0f);
  CO_Register("[Settings]", "enter", CO_TYPE_BOOL,
              &app->settingsState.MidiRequestEnter, 0, 1);

  // --- App / UI Navigation ---
  CO_Register("[App]", "settings_toggle", CO_TYPE_BOOL,
              &app->MidiRequestSettings, 0, 1);
  CO_Register("[App]", "info_toggle", CO_TYPE_BOOL, &app->MidiRequestInfo, 0,
              1);
  CO_Register("[App]", "mixer_toggle", CO_TYPE_BOOL, &app->MidiRequestMixer, 0,
              1);
  CO_Register("[App]", "browser_toggle", CO_TYPE_BOOL, &app->MidiRequestBrowser,
              0, 1);

  globalAudioEngine = audioEngine;

#if !defined(PLATFORM_IOS)
  UNX_LOG_INFO("[MAIN] Starting audio backend...");
  if (!AudioBackend_Start(initialAudioCfg, AudioProcessCallback)) {
    UNX_LOG_ERR("[MAIN] Failed to start audio backend on first attempt! "
                "Retrying in 500ms...");
    WaitTime(0.5);
    if (!AudioBackend_Start(initialAudioCfg, AudioProcessCallback)) {
      UNX_LOG_ERR("[MAIN] Audio backend start failed again. Sound might not be "
                  "available.");
    }
  }

  // Final sync of actual hardware sample rate
  int actualSR = 0;
  AudioBackend_GetActiveInfo(NULL, &actualSR, NULL, NULL);
  AudioEngine_SetOutputSampleRate(audioEngine, actualSR);
#endif

  UNX_LOG_INFO("[MAIN] Setting main loop (FPS: 60)...");

#if defined(PLATFORM_WEB) || defined(PLATFORM_IOS)
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
  emscripten_set_main_loop_arg((void (*)(void *))UpdateDrawFrame, app, 0, 1);
#elif defined(PLATFORM_IOS)
  // On iOS, we return here and let the platform layer call
  // ios_ready/ios_update.
  return 0;
#endif
#else
  while (!WindowShouldClose()) {
    UpdateDrawFrame(app);
  }

  UNX_LOG_INFO("[MAIN] Shutting down...");
  UIFonts_Unload();

  // Browser Cleanup (Inline to reduce external functions)
  if (app->browserState.DB)
    RB_FreeDatabase(app->browserState.DB);
  if (app->browserState.SeratoDB)
    Serato_FreeDatabase(app->browserState.SeratoDB);
  if (app->browserState.TrackPointers)
    free(app->browserState.TrackPointers);
  if (app->browserState.SeratoTrackPointers)
    free(app->browserState.SeratoTrackPointers);

  MIDI_Close(&app->midiCtx);
  AudioBackend_Terminate();
  CloseWindow();

  if (audioEngine) {
    for (int i = 0; i < MAX_DECKS; i++) {
      if (audioEngine->Decks[i].PCMBuffer)
        free(audioEngine->Decks[i].PCMBuffer);
    }
    free(audioEngine);
  }
  if (app)
    free(app);

  UNX_LOG_INFO("[MAIN] Exit.");
  Log_Close();
#endif

  return 0;
}

// Helper to manage artwork texture loading/unloading
void ManageArtwork(DeckState *ds) {
  if (strcmp(ds->ArtworkPath, ds->LastLoadedArtPath) != 0) {
    printf("[ARTWORK] Path changed: '%s'\n", ds->ArtworkPath);
    if (ds->ArtworkTexture.id != 0)
      UnloadTexture(ds->ArtworkTexture);
    ds->ArtworkTexture = (Texture2D){0};

    if (ds->ArtworkPath[0] != '\0') {
      char fixedPath[512];
      strncpy(fixedPath, ds->ArtworkPath, 511);
      fixedPath[511] = '\0';

      // Remove trailing ']' if present
      size_t len = strlen(fixedPath);
      if (len > 0 && fixedPath[len - 1] == ']')
        fixedPath[len - 1] = '\0';

      // Normalize slashes for Windows
      for (int p = 0; fixedPath[p]; p++)
        if (fixedPath[p] == '\\')
          fixedPath[p] = '/';

      if (FileExists(fixedPath)) {
        printf("[ARTWORK] File found: '%s'\n", fixedPath);
        Image img = LoadImageManual(fixedPath);
        if (img.data != NULL) {
          printf("[ARTWORK] Image data loaded: %dx%d\n", img.width, img.height);
          // Resize to a standard size for UI consistency
          ImageResize(&img, 128, 128);
          ds->ArtworkTexture = LoadTextureFromImage(img);
          UnloadImage(img);
          if (ds->ArtworkTexture.id != 0)
            SetTextureFilter(ds->ArtworkTexture, TEXTURE_FILTER_BILINEAR);
        }
      } else {
        printf("[ARTWORK] File NOT FOUND: '%s'\n", fixedPath);
      }
    }
    strncpy(ds->LastLoadedArtPath, ds->ArtworkPath, 511);
  }
}

void UpdateDrawFrame(App *app) {
  static bool firstCall = true;
  if (firstCall) {
    UNX_LOG_INFO("[MAIN] UpdateDrawFrame: First call (Window: %dx%d, Ready: "
                 "%d, Hidden: %d, Min: %d)",
                 GetScreenWidth(), GetScreenHeight(), IsWindowReady(),
                 IsWindowHidden(), IsWindowMinimized());
    firstCall = false;
  }

#if defined(PLATFORM_IOS)
#ifndef DEBUG_IOS_GUI
  // Safety: If window dimensions are not yet available, skip this frame.
  if (GetScreenWidth() <= 0 || GetScreenHeight() <= 0) {
    static double lastWarn = 0;
    if (GetTime() - lastWarn > 5.0) {
      UNX_LOG_WARN("[MAIN] UpdateDrawFrame: Waiting for valid screen "
                   "dimensions (%dx%d)...",
                   GetScreenWidth(), GetScreenHeight());
      lastWarn = GetTime();
    }
    return;
  }
  // Grace period for non-debug mode
  if (GetTime() > 2.0 && (IsWindowHidden() || IsWindowMinimized()))
    return;
#endif
#endif

  AudioEngine *audioEngine = globalAudioEngine;
  if (!audioEngine)
    return;

  static int lastScreen = -1;

  if (lastScreen != (int)app->screen) {
    UNX_LOG_INFO("[MAIN] Screen changed to: %d", (int)app->screen);
    lastScreen = (int)app->screen;
  }

  // Manage Artwork Textures
  ManageArtwork(&app->deckA);
  ManageArtwork(&app->deckB);

  // Keep Info screen in sync if active
  if (app->screen == ScreenInfo) {
    for (int i = 0; i < 2; i++) {
      DeckState *ds = (i == 0) ? &app->deckA : &app->deckB;
      InfoTrack *it = &app->infoState.Tracks[i];
      strcpy(it->Title, ds->TrackTitle);
      strcpy(it->Artist, ds->ArtistName);
      strcpy(it->Album, ds->AlbumName);
      strcpy(it->Genre, ds->GenreName);
      strcpy(it->Label, ds->LabelName);
      strcpy(it->Comment, ds->Comment);
      it->Year = ds->Year;
      it->Rating = ds->Rating;
      it->BPM = ds->OriginalBPM;
      strcpy(it->Key, ds->TrackKey);
      it->Duration = ds->TrackLengthMs / 1000;
      strcpy(it->Source, ds->SourceName);
      strcpy(it->ArtworkPath, ds->ArtworkPath);
      it->ArtworkTexture = &ds->ArtworkTexture;
    }
  }

  // --- Sync Audio Engine State to UI State ---
  if (audioEngine->Decks[0].PCMBuffer) {
    // Position is already frame-based (L+R pair = 1 frame)
    double playheadFrames = audioEngine->Decks[0].Position;
    double srA = (double)audioEngine->Decks[0].SampleRate;
    if (srA < 8000)
      srA = 44100.0;

    app->deckA.Position = (playheadFrames * 150.0) / srA;
    app->deckA.IsPlaying = audioEngine->Decks[0].IsPlaying;

    double posSec = playheadFrames / srA;
    app->deckA.PositionMs = (long long)(posSec * 1000.0);

    double lenSec =
        ((double)audioEngine->Decks[0].TotalSamples / (double)CHANNELS) / srA;
    app->deckA.TrackLengthMs = (long long)(lenSec * 1000.0);
  }
  if (audioEngine->Decks[1].PCMBuffer) {
    double playheadFrames = audioEngine->Decks[1].Position;
    double srB = (double)audioEngine->Decks[1].SampleRate;
    if (srB < 8000)
      srB = 44100.0;

    app->deckB.Position = (playheadFrames * 150.0) / srB;
    app->deckB.IsPlaying = audioEngine->Decks[1].IsPlaying;

    double posSec = playheadFrames / srB;
    app->deckB.PositionMs = (long long)(posSec * 1000.0);

    double lenSec =
        ((double)audioEngine->Decks[1].TotalSamples / (double)CHANNELS) / srB;
    app->deckB.TrackLengthMs = (long long)(lenSec * 1000.0);
  }

  // --- Auto Stop at End of Track / Beatgrid ---
  for (int i = 0; i < 2; i++) {
    DeckState *ds = (i == 0) ? &app->deckA : &app->deckB;
    if (ds->IsPlaying && ds->LoadedTrack) {
      long long endMs = ds->TrackLengthMs;
      if (ds->LoadedTrack->BeatGridCount > 0) {
        endMs = (long long)ds->LoadedTrack
                    ->BeatGrid[ds->LoadedTrack->BeatGridCount - 1]
                    .Time;
      }

      // Stop if we passed the end marker
      if (ds->PositionMs >= endMs) {
        DeckAudio_Stop(&audioEngine->Decks[i]);
        // Also ensure position doesn't drift too far past
        DeckAudio_JumpToMs(&audioEngine->Decks[i], endMs);
        ds->IsPlaying = false;
        ds->PositionMs = endMs;
      }
    }
  }

  // Cache scale for this frame based on current window size
  UI_UpdateScale();

  app->topbar.ActiveScreen = app->screen;

  // Navigation Logic (Time-based Splash)
  if (app->screen == ScreenSplash) {
    static float splashTime = 0;
    splashTime += GetFrameTime();
    if (splashTime >= 2.0f) { // 2 Seconds splash
      SplashRenderer_Unload(&app->splash);
      app->screen = ScreenPlayer;
    }
  }

  // Exclusive Master Logic & Auto Takeover
  static bool lastMasterA = false;
  static bool lastMasterB = false;
  static int lastSyncA = 0;
  static int lastSyncB = 0;

  // Auto Assign Master: If Sync is turned ON and No deck is master
  bool noMaster = !app->deckA.IsMaster && !app->deckB.IsMaster;
  if (noMaster) {
    if (app->deckA.SyncMode > 0 && lastSyncA == 0)
      app->deckB.IsMaster = true;
    else if (app->deckB.SyncMode > 0 && lastSyncB == 0)
      app->deckA.IsMaster = true;
  }
  lastSyncA = app->deckA.SyncMode;
  lastSyncB = app->deckB.SyncMode;

  // Auto Takeover: If Master stops, other deck becomes Master if playing
  if (app->deckA.IsMaster && !app->deckA.IsPlaying && app->deckB.IsPlaying) {
    app->deckA.IsMaster = false;
    app->deckB.IsMaster = true;
  } else if (app->deckB.IsMaster && !app->deckB.IsPlaying &&
             app->deckA.IsPlaying) {
    app->deckB.IsMaster = false;
    app->deckA.IsMaster = true;
  }

  if (app->deckA.IsMaster && !lastMasterA)
    app->deckB.IsMaster = false;
  if (app->deckB.IsMaster && !lastMasterB)
    app->deckA.IsMaster = false;
  lastMasterA = app->deckA.IsMaster;
  lastMasterB = app->deckB.IsMaster;

  HandleKeyboardInputs(&app->keyMap, &app->deckA, &app->deckB, audioEngine,
                       &app->fxState);
  MIDI_Update(&app->midiCtx, &app->deckA, &app->deckB, audioEngine);

  // --- Global Beat FX Sync ---
  float masterBpm = 120.0f;
  if (app->deckA.IsMaster)
    masterBpm = app->deckA.CurrentBPM;
  else if (app->deckB.IsMaster)
    masterBpm = app->deckB.CurrentBPM;
  else
    masterBpm = app->deckA.CurrentBPM;
  if (masterBpm < 1.0f)
    masterBpm = 120.0f;

  static const float XPadRatios[] = {0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f};
  int padIdx = app->fxState.SelectedPad;
  if (padIdx < 0)
    padIdx = 4;
  if (padIdx >= 6)
    padIdx = 5;
  float ratio = XPadRatios[padIdx];
  float fxMs = (60000.0f / masterBpm) * ratio;

  audioEngine->BeatFX.activeFX = app->fxState.SelectedFX;
  audioEngine->BeatFX.targetChannel = app->fxState.SelectedChannel;
  audioEngine->BeatFX.isFxOn = app->fxState.IsFXOn;
  audioEngine->BeatFX.beatMs = fxMs;
  audioEngine->BeatFX.levelDepth = app->fxState.LevelDepth;
  audioEngine->BeatFX.scrubVal = app->fxState.XPadScrubValue;
  audioEngine->BeatFX.isScrubbing = app->fxState.IsXPadScrubbing;

  // --- Release FX Sync & Cleanup ---
  for (int i = 0; i < 2; i++) {
    if (audioEngine->Decks[i].ReleaseFXType == 5 &&
        audioEngine->Decks[i].ReleaseFXTimer <= 0.05f) {
      if (app->fxState.IsFXOn && app->fxState.SelectedChannel == i + 1) {
        app->fxState.IsFXOn = false;
      }
    }
  }

  // --- MIDI UI Navigation ---
  if (app->MidiRequestBrowser || IsKeyPressed(app->keyMap.toggleBrowser)) {
    TopBar_OnBrowse(app);
    app->MidiRequestBrowser = false;
  }
  if (app->MidiRequestSettings) {
    TopBar_OnSettings(app);
    app->MidiRequestSettings = false;
  }
  if (app->MidiRequestInfo) {
    TopBar_OnInfo(app);
    app->MidiRequestInfo = false;
  }
  if (app->MidiRequestMixer) {
    TopBar_OnMixer(app);
    app->MidiRequestMixer = false;
  }

  // Handle Library Up/Down requests
  if (app->browserState.MidiRequestUp) {
    app->browserState.MidiBrowseDelta = -1;
    app->browserState.MidiRequestUp = false;
  }
  if (app->browserState.MidiRequestDown) {
    app->browserState.MidiBrowseDelta = 1;
    app->browserState.MidiRequestDown = false;
  }

  // Handle Deck MIDI Requests
  for (int i = 0; i < 2; i++) {
    DeckState *ds = (i == 0) ? &app->deckA : &app->deckB;
    DeckAudioState *audio = &audioEngine->Decks[i];

    // Hot Cues
    for (int j = 0; j < 8; j++) {
      if (ds->MidiRequestHotCue[j]) {
        if (ds->LoadedTrack && j < ds->LoadedTrack->HotCuesCount) {
          uint32_t targetMs = ds->LoadedTrack->HotCues[j].Start;
          
          if (ds->QuantizeEnabled && ds->IsPlaying) {
             int32_t waitMs = Quantize_GetWaitMs(ds->LoadedTrack, ds->PositionMs);
             if (waitMs > 5) { // Only queue if more than 5ms wait (avoid jitter)
                 DeckAudio_QueueJumpMs(audio, targetMs, (uint32_t)waitMs);
                 ds->IsPlaying = true;
                 audio->IsPlaying = true;
                 audio->IsMotorOn = true;
             } else {
                 ds->SeekMs = targetMs;
                 ds->HasSeekRequest = true;
                 ds->IsPlaying = true;
                 audio->IsPlaying = true;
                 audio->IsMotorOn = true;
             }
          } else {
             ds->SeekMs = targetMs;
             ds->HasSeekRequest = true;
             ds->IsPlaying = true;
             audio->IsPlaying = true;
             audio->IsMotorOn = true;
          }
        }
        ds->MidiRequestHotCue[j] = false;
      }
    }

    // Looping
    if (ds->MidiRequestLoopIn) {
      DeckAudio_SetLoop(audio, true, audio->Position,
                        audio->Position + 44100 * 4); // Default 4 beat loop
      ds->MidiRequestLoopIn = false;
    }
    if (ds->MidiRequestLoopOut) {
      if (audio->IsLooping)
        DeckAudio_ExitLoop(audio);
      ds->MidiRequestLoopOut = false;
    }
    if (ds->MidiRequestLoopExit) {
      DeckAudio_ExitLoop(audio);
      ds->MidiRequestLoopExit = false;
    }

    // Pitch Bend
    if (ds->MidiRequestPitchBendPlus) {
      CO_AddValue(i == 0 ? "[Channel1]" : "[Channel2]", "jog", 5.0f);
      ds->MidiRequestPitchBendPlus = false;
    }
    if (ds->MidiRequestPitchBendMinus) {
      CO_AddValue(i == 0 ? "[Channel1]" : "[Channel2]", "jog", -5.0f);
      ds->MidiRequestPitchBendMinus = false;
    }

    // Sync & Master
    if (ds->MidiRequestSync) {
      ds->SyncMode = (ds->SyncMode == 0) ? 1 : 0;
      ds->MidiRequestSync = false;
    }
    if (ds->MidiRequestMaster) {
      ds->IsMaster = !ds->IsMaster;
      ds->MidiRequestMaster = false;
    }

    // Beat Jump
    if (ds->MidiRequestBeatJumpForward) {
      ds->SeekMs += 2000; // Simplified 2s jump
      ds->HasSeekRequest = true;
      ds->MidiRequestBeatJumpForward = false;
    }
    if (ds->MidiRequestBeatJumpBackward) {
      ds->SeekMs -= 2000;
      ds->HasSeekRequest = true;
      ds->MidiRequestBeatJumpBackward = false;
    }

    // Auto Loop
    int loopSizes[] = {1, 2, 4, 8, 16};
    for (int k = 0; k < 5; k++) {
      if (ds->MidiRequestAutoLoop[k]) {
        double beatMs = (60000.0f / ds->CurrentBPM);
        double loopLenFrames =
            (beatMs * loopSizes[k] * audio->SampleRate) / 1000.0f;
        DeckAudio_SetLoop(audio, true, audio->Position,
                          audio->Position + loopLenFrames);
        ds->MidiRequestAutoLoop[k] = false;
      }
    }
  }

  // Tempo Calculation (10000 = 100%)
  float realPitchA = 1.0f + (app->deckA.TempoPercent / 100.0f);
  audioEngine->Decks[0].Pitch = (uint16_t)(realPitchA * 10000.0f);
  app->deckA.CurrentBPM = app->deckA.OriginalBPM * realPitchA;

  float realPitchB = 1.0f + (app->deckB.TempoPercent / 100.0f);
  audioEngine->Decks[1].Pitch = (uint16_t)(realPitchB * 10000.0f);
  app->deckB.CurrentBPM = app->deckB.OriginalBPM * realPitchB;

  // --- Sync Control Logic ---
  Sync_Update(&app->deckA, &app->deckB, audioEngine);

  // --- Sync UI Jog/Modes back to Audio Engine ---
  // Deck A
  if (app->deckA.IsTouching != audioEngine->Decks[0].IsTouching) {
    bool released = !app->deckA.IsTouching && audioEngine->Decks[0].IsTouching;
    DeckAudio_SetJogTouch(&audioEngine->Decks[0], app->deckA.IsTouching);

    // Phase Snap on release if Beat Sync is ON
    if (released && app->deckA.SyncMode == 2 && !app->deckA.IsMaster) {
      Sync_RequestPhaseSnap(&app->deckA, &app->deckB, audioEngine);
    }

    // Immediately switch to BPM sync on touch if was in BEAT sync
    if (app->deckA.IsTouching && app->deckA.SyncMode == 2) {
      app->deckA.SyncMode = 1;
    }
  }
  if (app->deckA.IsTouching) {
    double dt = GetFrameTime();
    if (dt < 0.001)
      dt = 0.016;
    double srA = (double)audioEngine->Decks[0].SampleRate;
    if (srA < 8000)
      srA = 44100.0;

    double framesInFrame = srA * dt;
    // JogDelta is in 150Hz frames. Convert to PCM frames for comparison.
    double instantaneousRate =
        (app->deckA.JogDelta * (srA / 150.0)) / (framesInFrame);

    audioEngine->Decks[0].JogRate = instantaneousRate;
    app->deckA.JogDelta = 0;
  }
  audioEngine->Decks[0].VinylModeEnabled = app->deckA.VinylModeEnabled;
  audioEngine->Decks[0].MasterTempoActive = app->deckA.MasterTempo;
  audioEngine->Decks[0].BPM = app->deckA.CurrentBPM;

  // Deck B
  if (app->deckB.IsTouching != audioEngine->Decks[1].IsTouching) {
    bool released = !app->deckB.IsTouching && audioEngine->Decks[1].IsTouching;
    DeckAudio_SetJogTouch(&audioEngine->Decks[1], app->deckB.IsTouching);

    // Phase Snap on release if Beat Sync is ON
    if (released && app->deckB.SyncMode == 2 && !app->deckB.IsMaster) {
      Sync_RequestPhaseSnap(&app->deckB, &app->deckA, audioEngine);
    }

    // Immediately switch to BPM sync on touch if was in BEAT sync
    if (app->deckB.IsTouching && app->deckB.SyncMode == 2) {
      app->deckB.SyncMode = 1;
    }
  }
  if (app->deckB.IsTouching) {
    double dt = GetFrameTime();
    if (dt < 0.001)
      dt = 0.016;
    double srB = (double)audioEngine->Decks[1].SampleRate;
    if (srB < 8000)
      srB = 44100.0;

    double framesInFrame = srB * dt;
    double instantaneousRate =
        (app->deckB.JogDelta * (srB / 150.0)) / framesInFrame;

    audioEngine->Decks[1].JogRate = instantaneousRate;
    app->deckB.JogDelta = 0;
  }
  audioEngine->Decks[1].VinylModeEnabled = app->deckB.VinylModeEnabled;
  audioEngine->Decks[1].MasterTempoActive = app->deckB.MasterTempo;
  audioEngine->Decks[1].BPM = app->deckB.CurrentBPM;

  // Apply Vinyl Start/Stop Physics
  for (int i = 0; i < 2; i++) {
    DeckState *ds = (i == 0) ? &app->deckA : &app->deckB;
    double sr = (double)audioEngine->Decks[i].SampleRate;
    if (sr < 8000)
      sr = 44100.0;

    // Assuming 1024 frames per block as set in InitAudio
    float blockSize = 1024.0f;
    float blocksPerSec = (float)sr / blockSize;

    // Accel is the increment per block to reach pitch 1.0 in N seconds
    // Since we want linear ramp: delta = (TargetRate - 0) / TotalBlocks
    // TotalBlocks = DurationSeconds * blocksPerSec
    audioEngine->Decks[i].VinylStartAccel =
        1.0f / ((ds->Waveform.VinylStartMs / 1000.0f) * blocksPerSec + 1.0f);
    audioEngine->Decks[i].VinylStopAccel =
        1.0f / ((ds->Waveform.VinylStopMs / 1000.0f) * blocksPerSec + 1.0f);
  }

  if (IsKeyPressed(app->keyMap.toggleInfo)) {
    if (app->screen == ScreenInfo) {
      app->screen = ScreenPlayer;
      app->infoState.IsActive = false;
    } else {
      app->screen = ScreenInfo;
      app->infoState.IsActive = true;

      // Sync Info State from Decks
      for (int i = 0; i < 2; i++) {
        DeckState *ds = (i == 0) ? &app->deckA : &app->deckB;
        InfoTrack *it = &app->infoState.Tracks[i];
        strcpy(it->Title, ds->TrackTitle);
        strcpy(it->Artist, ds->ArtistName);
        strcpy(it->Album, ds->AlbumName);
        strcpy(it->Genre, ds->GenreName);
        strcpy(it->Label, ds->LabelName);
        strcpy(it->Comment, ds->Comment);
        it->Year = ds->Year;
        it->Rating = ds->Rating;
        it->BPM = ds->OriginalBPM;
        strcpy(it->Key, ds->TrackKey);
        it->Duration = ds->TrackLengthMs / 1000;
        strcpy(it->Source, ds->SourceName);
        strcpy(it->ArtworkPath, ds->ArtworkPath);
        it->ArtworkTexture = &ds->ArtworkTexture;
      }
    }
  }

  if (IsKeyPressed(app->keyMap.toggleSettings)) {
    if (app->screen == ScreenSettings) {
      app->screen = ScreenPlayer;
      app->settingsState.IsActive = false;
    } else {
      app->screen = ScreenSettings;
      app->settingsState.IsActive = true;
    }
  }

  if (IsKeyPressed(app->keyMap.toggleMixer)) {
    if (app->screen == ScreenMixer) {
      app->screen = ScreenPlayer;
      app->mixerState.IsActive = false;
    } else {
      app->screen = ScreenMixer;
      app->mixerState.IsActive = true;
      // Hook audio engine up right before drawing if not earlier
      app->mixerState.AudioPlugin = audioEngine;
    }
  }

  // ESC / Back logic
  if (IsKeyPressed(app->keyMap.back)) {
    if (app->screen == ScreenBrowser) {
      if (app->browserState.BrowseLevel == 3 && !app->browserState.IsTagList) {
        app->screen = ScreenPlayer;
        app->browserState.IsActive = false;
      } else {
        Browser_Back(&app->browserState);
      }
    } else if (app->screen != ScreenPlayer && app->screen != ScreenSplash) {
      app->screen = ScreenPlayer;
      app->browserState.IsActive = false;
      app->infoState.IsActive = false;
      app->settingsState.IsActive = false;
      app->aboutState.IsActive = false;
      app->creditsState.IsActive = false;
      app->mixerState.IsActive = false;
    }
  }

  // Update active components
  if (app->screen == ScreenSplash)
    app->splash.base.Update((Component *)&app->splash);
  if (app->screen == ScreenPlayer)
    app->player.base.Update((Component *)&app->player);
  if (app->screen == ScreenBrowser)
    app->browser.base.Update((Component *)&app->browser);
  if (app->screen == ScreenInfo)
    app->info.base.Update((Component *)&app->info);
  if (app->screen == ScreenSettings)
    app->settings.base.Update((Component *)&app->settings);
  if (app->screen == ScreenAbout)
    app->about.base.Update((Component *)&app->about);
  if (app->screen == ScreenMixer)
    app->mixer.base.Update((Component *)&app->mixer);
  if (app->screen == ScreenPad)
    app->pad.base.Update((Component *)&app->pad);
  if (app->screen == ScreenDebug)
    app->debugView.base.Update((Component *)&app->debugView);
  if (app->screen == ScreenCredits)
    app->credits.base.Update((Component *)&app->credits);
  if (app->screen == ScreenSplash) {
    app->splash.base.Update((Component *)&app->splash);
    if (app->splashCounter > 0)
      app->splashCounter--;
  }

  if (app->screen != ScreenSplash && app->screen != ScreenDebug) {
    app->stripA.base.Update((Component *)&app->stripA);
    app->stripB.base.Update((Component *)&app->stripB);
    app->topbar.base.Update((Component *)&app->topbar);

    // --- Update System Stats (CPU/RAM) every 1s ---
    static float statsTimer = 0;
    statsTimer += GetFrameTime();
    if (statsTimer >= 1.0f) {
      SystemStats stats = GetSystemStats();
      app->topbar.CPUUsage = stats.cpuUsage;
      app->topbar.RAMUsage = stats.ramUsageMB;
      app->topbar.BatteryLevel = stats.batteryLevel;
      app->topbar.IsCharging = stats.isCharging;
      statsTimer = 0;
    }

    // --- Handle Seek Requests from UI (Hot Cues / Scrubbing) ---
    if (app->deckA.HasSeekRequest) {
      DeckAudio_JumpToMs(&audioEngine->Decks[0], app->deckA.SeekMs);
      if (app->deckA.IsPlaying) DeckAudio_InstantPlay(&audioEngine->Decks[0]);
      app->deckA.HasSeekRequest = false;
      app->padState.ActiveLoopIdx[0] = -1;
    }
    if (app->deckB.HasSeekRequest) {
      DeckAudio_JumpToMs(&audioEngine->Decks[1], app->deckB.SeekMs);
      if (app->deckB.IsPlaying) DeckAudio_InstantPlay(&audioEngine->Decks[1]);
      app->deckB.HasSeekRequest = false;
      app->padState.ActiveLoopIdx[1] = -1;
    }
  }

  if (!IsWindowReady())
    return;

  BeginDrawing();
  ClearBackground(BLACK);

  // Apply global offset for all UI drawing
  rlPushMatrix();
  rlTranslatef(UI_OffsetX, UI_OffsetY, 0);

  // High-level Screen Router
  switch (app->screen) {
  case ScreenPlayer:
    app->player.base.Draw((Component *)&app->player);
    break;
  case ScreenBrowser:
    app->browser.base.Draw((Component *)&app->browser);
    break;
  case ScreenInfo:
    app->info.base.Draw((Component *)&app->info);
    break;
  case ScreenSettings:
    app->settings.base.Draw((Component *)&app->settings);
    break;
  case ScreenAbout:
    app->about.base.Draw((Component *)&app->about);
    break;
  case ScreenMixer:
    app->mixer.base.Draw((Component *)&app->mixer);
    break;
  case ScreenSplash:
    app->splash.base.Draw((Component *)&app->splash);
    break;
  case ScreenDebug:
    app->debugView.base.Draw((Component *)&app->debugView);
    break;
  case ScreenPad:
    app->pad.base.Draw((Component *)&app->pad);
    break;
  case ScreenCredits:
    app->credits.base.Draw((Component *)&app->credits);
    break;
  default:
    ClearBackground(MAGENTA); // Fail-safe color
    DrawText("UNKNOWN SCREEN", 10, 10, 20, WHITE);
    break;
  }

  // Draw Global Overlays
  if (app->showExitConfirm) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.5f));
    DrawRectangle(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 - 50, 300, 100,
                  ColorDark2);
    DrawText("EXIT APP?", SCREEN_WIDTH / 2 - 50, SCREEN_HEIGHT / 2 - 30, 20,
             WHITE);
    DrawText("PRESS PLAY TO CONFIRM", SCREEN_WIDTH / 2 - 100,
             SCREEN_HEIGHT / 2 + 10, 15, GRAY);
  }

  if (app->screen != ScreenSplash) {
    app->stripA.base.Draw((Component *)&app->stripA);
    app->stripB.base.Draw((Component *)&app->stripB);
    app->topbar.base.Draw((Component *)&app->topbar);
  }

  if (app->showExitConfirm) {
    float pw = S(200);
    float ph = S(100);
    float px = (SCREEN_WIDTH - pw) / 2.0f;
    float py = (SCREEN_HEIGHT - ph) / 2.0f;

    // Overlay
    DrawRectangle(-UI_OffsetX, -UI_OffsetY, GetScreenWidth(), GetScreenHeight(),
                  Fade(BLACK, 0.8f));

    // Box
    DrawRectangle(px, py, pw, ph, ColorBGUtil);
    DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 1.0f, ColorGray);

    Font fMd = UIFonts_GetFace(S(12));
    DrawCentredText("EXIT APPLICATION?", fMd, px, pw, py + S(20), S(12),
                    ColorWhite);
    DrawCentredText("Are you sure?", UIFonts_GetFace(S(9)), px, pw, py + S(40),
                    S(9), ColorShadow);

    // Options
    float btnW = S(60);
    float btnH = S(20);

    // NO Button
    bool noHover = CheckCollisionPointRec(
        UIGetMousePosition(), (Rectangle){px + S(30), py + S(65), btnW, btnH});
    DrawRectangle(px + S(30), py + S(65), btnW, btnH,
                  noHover ? ColorGray : ColorDark1);
    DrawRectangleLines(px + S(30), py + S(65), btnW, btnH, ColorShadow);
    DrawCentredText("NO", fMd, px + S(30), btnW, py + S(68), S(11), ColorWhite);

    // YES Button
    bool yesHover = CheckCollisionPointRec(
        UIGetMousePosition(),
        (Rectangle){px + pw - S(90), py + S(65), btnW, btnH});
    DrawRectangle(px + pw - S(90), py + S(65), btnW, btnH,
                  yesHover ? ColorRed : ColorDark1);
    DrawRectangleLines(px + pw - S(90), py + S(65), btnW, btnH, ColorRed);
    DrawCentredText("YES", fMd, px + pw - S(90), btnW, py + S(68), S(11),
                    ColorWhite);

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      if (noHover)
        app->showExitConfirm = false;
      if (yesHover) {
        // In a callback-based loop, we might need a flag to exit
        // For now, we'll just keep it as is, but it won't exit cleanly on iOS
        // unless the platform handles it.
      }
    }
    if (IsKeyPressed(KEY_ENTER)) { /* handle */
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))
      app->showExitConfirm = false;
  }

  rlPopMatrix();

  EndDrawing();
}
