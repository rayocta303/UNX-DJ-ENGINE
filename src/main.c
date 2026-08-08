#include "core/audio_backend.h"
#include "core/logger.h"
#include "core/logic/control_object.h"
#include "core/logic/settings_io.h"
#include "core/logic/jog_config.h"
#include "core/logic/sync.h"
#include "core/midi/midi_handler.h"
#include "core/system_info.h"
#include "core/memory_guard.h"
#include "input/keyboard.h"
#include "raylib.h"
#include "rlgl.h"
#if defined(GRAPHICS_API_OPENGL_ES2) || defined(PLATFORM_DESKTOP)
#include <GLES2/gl2.h>
#endif
#include "ui/browser/browser.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/player.h"
#include "ui/player/waveform.h"
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
#include "library/hotcue_db.h"
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
#include <GL/gl.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#elif defined(__ANDROID__)
#include <GLES2/gl2.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <unistd.h>
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
    UNX_LOG_WARN("[STBI] Failed to load '%s': %s", path, stbi_failure_reason());
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
  ColorFXManager colorFxDeckA;
  ColorFXManager colorFxDeckB;
  BrowserState browserState;
  InfoState infoState;
  SettingsState settingsState;
  AboutState aboutState;
  CreditsState creditsState;
  MixerState mixerState;
  PadState padState;
  float libraryScrollDelta;

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
  float masterVolume; // Persisted master output level (0.0 - 1.0)
  AudioBackendConfig activeAudioConfig;

  bool MidiRequestSettings;
  bool MidiRequestInfo;
  bool MidiRequestMixer;
  bool MidiRequestBrowser;
  int MidiWaveformZoomStep;
  bool MidiWaveformZoomIn;
  bool MidiWaveformZoomOut;

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

void App_SaveSettings(App *a) {
  if (!a)
    return;
  if (globalAudioEngine) {
    a->colorFxDeckA = globalAudioEngine->Decks[0].ColorFX;
    a->colorFxDeckB = globalAudioEngine->Decks[1].ColorFX;
    a->fxState.SelectedFX = globalAudioEngine->BeatFX.activeFX;
    a->fxState.SelectedChannel = globalAudioEngine->BeatFX.targetChannel;
    a->fxState.LevelDepth = globalAudioEngine->BeatFX.levelDepth;
    a->fxState.IsFXOn = globalAudioEngine->BeatFX.isFxOn;
    a->masterVolume = globalAudioEngine->MasterVolume;
  }
  Settings_Save(a->deckA.Waveform, a->deckB.Waveform, a->activeAudioConfig,
                a->fxState, a->colorFxDeckA, a->colorFxDeckB,
                a->activeControllerPath, a->deckA.QuantizeEnabled,
                a->deckB.QuantizeEnabled, a->masterVolume);
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
  a->deckA.Waveform.JogCalibRPM = (a->settingsState.Items[8].Current == 1) ? 45.0f : 33.3f;
  a->deckB.Waveform = a->deckA.Waveform;

  UNX_LOG_INFO(
      "[SETTINGS] Applied Style: %d, Gains: L%.2f M%.2f H%.2f, Start: %.0f, "
      "Stop: %.0f, Lock: %d, JogRPM: %.1f",
      a->deckA.Waveform.Style, a->deckA.Waveform.GainLow,
      a->deckA.Waveform.GainMid, a->deckA.Waveform.GainHigh,
      a->deckA.Waveform.VinylStartMs, a->deckA.Waveform.VinylStopMs,
      a->deckA.Waveform.LoadLock, a->deckA.Waveform.JogCalibRPM);

  // Apply Audio backend settings
  AudioBackendConfig aconf = {
      .DeviceIndex =
          a->settingsState.Items[9].Current - 1, // 0 is System Default
      .MasterOutL = a->settingsState.Items[10].Current,
      .MasterOutR = a->settingsState.Items[11].Current,
      // Cue items have "Blank" at index 0, so subtract 1 for physical channel
      .CueOutL = a->settingsState.Items[12].Current - 1,
      .CueOutR = a->settingsState.Items[13].Current - 1,
      .SampleRate = (a->settingsState.Items[15].Current == 0) ? 44100 : 48000,
      .PCMBitDepth = (a->settingsState.Items[16].Current == 0) ? 16 : 24,
      .CrossfaderCurve = a->settingsState.Items[17].Current,
  };
  int bufMap[] = {128, 256, 512, 1024};
  aconf.BufferSizeFrames = bufMap[a->settingsState.Items[14].Current];

  if (globalAudioEngine) {
    globalAudioEngine->CrossfaderCurve = aconf.CrossfaderCurve;
  }

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
    a->activeAudioConfig.CrossfaderCurve = aconf.CrossfaderCurve;
  }

  App_SaveSettings(a);
}

void UpdateChannelOptions(App *a, int deviceIdx) {
  AudioDeviceInfo devs[MAX_AUDIO_DEVICES];
  int devCount = AudioBackend_GetDevices(devs, MAX_AUDIO_DEVICES);

  int channels = 2; // Fallback for System Default
  if (deviceIdx >= 0 && deviceIdx < devCount) {
    channels = devs[deviceIdx].NativeChannels;
  }

  // Common init for channel items (Items 10..13)
  strcpy(a->settingsState.Items[10].Label, "MASTER LEFT");
  a->settingsState.Items[10].Type = SETTING_TYPE_LIST;
  strcpy(a->settingsState.Items[11].Label, "MASTER RIGHT");
  a->settingsState.Items[11].Type = SETTING_TYPE_LIST;
  strcpy(a->settingsState.Items[12].Label, "CUE LEFT");
  a->settingsState.Items[12].Type = SETTING_TYPE_LIST;
  strcpy(a->settingsState.Items[13].Label, "CUE RIGHT");
  a->settingsState.Items[13].Type = SETTING_TYPE_LIST;

  a->settingsState.Items[10].Category = SETTING_CAT_AUDIO;
  a->settingsState.Items[11].Category = SETTING_CAT_AUDIO;
  a->settingsState.Items[12].Category = SETTING_CAT_AUDIO;
  a->settingsState.Items[13].Category = SETTING_CAT_AUDIO;

  // Master L/R: CH 1..N
  a->settingsState.Items[10].OptionsCount = channels;
  a->settingsState.Items[11].OptionsCount = channels;
  for (int i = 0; i < channels && i < MAX_SETTING_OPTIONS; i++) {
    sprintf(a->settingsState.Items[10].Options[i], "CH %d", i + 1);
    sprintf(a->settingsState.Items[11].Options[i], "CH %d", i + 1);
  }

  // Cue L/R: Blank, CH 1..N
  a->settingsState.Items[12].OptionsCount = channels + 1;
  a->settingsState.Items[13].OptionsCount = channels + 1;
  strcpy(a->settingsState.Items[12].Options[0], "Blank");
  strcpy(a->settingsState.Items[13].Options[0], "Blank");
  for (int i = 0; i < channels && (i + 1) < MAX_SETTING_OPTIONS; i++) {
    sprintf(a->settingsState.Items[12].Options[i + 1], "CH %d", i + 1);
    sprintf(a->settingsState.Items[13].Options[i + 1], "CH %d", i + 1);
  }

  // Auto-Select defaults
  a->settingsState.Items[10].Current = 0; // Master L -> CH1
  a->settingsState.Items[11].Current = (channels > 1) ? 1 : 0; // Master R -> CH2
  a->settingsState.Items[12].Current = 1; // Cue L -> CH 1 (Index 0 is Blank)
  a->settingsState.Items[13].Current = (channels > 1) ? 2 : 1; // Cue R -> CH 2
}

void OnSettingsValueChanged(void *ctx, int idx) {
  App *a = (App *)ctx;
  SettingItem *item = &a->settingsState.Items[idx];
  if (strcmp(item->Label, "AUDIO DEVICE") == 0) {
    UpdateChannelOptions(a, item->Current - 1);
  } else if (strcmp(item->Label, "CONNECTED DEVICE") == 0) {
    int devChoice = item->Current;
    char devName[128] = "";
    char autoPresetPath[256] = "";
    if (devChoice == 0) {
      MIDI_SelectDevice(&a->midiCtx, 0, devName, autoPresetPath);
    } else {
      MIDI_SelectDevice(&a->midiCtx, devChoice - 1, devName, autoPresetPath);
    }
    if (autoPresetPath[0] != '\0') {
      strncpy(a->activeControllerPath, autoPresetPath, 255);
    }
    PopulateMidiSettings(a);
    App_SaveSettings(a);
  } else if (strcmp(item->Label, "MAPPING PRESET") == 0) {
    int presetIdx = item->Current;
    if (presetIdx < a->midiPresetCount) {
      strncpy(a->activeControllerPath, a->midiPresetPaths[presetIdx], 255);
      MIDI_RefreshMapping(a->activeControllerPath);
      // Refresh the MIDI mapping tab items
      PopulateMidiSettings(a);
      // Save immediately when preset changes
      App_SaveSettings(a);
    }
  }
}

void OnSettingsAction(void *ctx, int idx) {
  App *a = (App *)ctx;
  SettingItem *item = &a->settingsState.Items[idx];
  if (strcmp(item->Label, "ABOUT") == 0) {
    a->screen = ScreenAbout;
    a->aboutState.IsActive = true;
  } else if (strcmp(item->Label, "CREDITS") == 0) {
    a->screen = ScreenCredits;
    a->creditsState.IsActive = true;
  } else if (strcmp(item->Label, "EXIT APPLICATION") == 0) {
    a->showExitConfirm = true;
  }
}

void PopulateMidiSettings(App *a) {
  // --- CONNECTED DEVICE SELECTION ITEM (Index 21) ---
  char devNames[16][64];
  int devCount = MIDI_GetDeviceList(devNames);

  strcpy(a->settingsState.Items[21].Label, "CONNECTED DEVICE");
  a->settingsState.Items[21].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[21].Category = SETTING_CAT_CONTROLLERS;
  
  strcpy(a->settingsState.Items[21].Options[0], "AUTO DETECT");
  for (int i = 0; i < devCount && (i + 1) < MAX_SETTING_OPTIONS; i++) {
    strncpy(a->settingsState.Items[21].Options[i + 1], devNames[i], 31);
  }
  a->settingsState.Items[21].OptionsCount = devCount + 1;
  
  a->settingsState.Items[21].Current = 0;
  if (a->midiCtx.activeDeviceName[0] != '\0') {
    for (int i = 0; i < devCount; i++) {
      if (strstr(devNames[i], a->midiCtx.activeDeviceName) || strstr(a->midiCtx.activeDeviceName, devNames[i])) {
        a->settingsState.Items[21].Current = i + 1;
        break;
      }
    }
  }

  // --- PRESET SELECTION ITEM (Index 22) ---
  char names[32][64];
  a->midiPresetCount =
      MIDI_ListControllers("controllers", names, a->midiPresetPaths);

  strcpy(a->settingsState.Items[22].Label, "MAPPING PRESET");
  a->settingsState.Items[22].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[22].Category = SETTING_CAT_CONTROLLERS;
  a->settingsState.Items[22].OptionsCount = a->midiPresetCount;
  a->settingsState.Items[22].Current = 0;

  for (int i = 0; i < a->midiPresetCount; i++) {
    strncpy(a->settingsState.Items[22].Options[i], names[i], 31);
    if (strcmp(a->activeControllerPath, a->midiPresetPaths[i]) == 0) {
      a->settingsState.Items[22].Current = i;
    }
  }

  a->settingsState.ItemsCount = 23;
}

static void App_DeactivateAllViews(App *a) {
  a->browserState.IsActive = false;
  a->infoState.IsActive = false;
  a->mixerState.IsActive = false;
  a->settingsState.IsActive = false;
  a->padState.IsActive = false;
  a->aboutState.IsActive = false;
  a->creditsState.IsActive = false;
  // Crucial: reset input focus flags
  a->browserState.IsSearching = false;
}

static double g_lastTopBarSwitchTime = 0.0;

void TopBar_OnBrowse(void *ctx) {
  double now = GetTime();
  if (now - g_lastTopBarSwitchTime < 0.30) return;
  g_lastTopBarSwitchTime = now;

  App *a = (App *)ctx;
  if (a->screen == ScreenBrowser) {
    a->screen = ScreenPlayer;
    App_DeactivateAllViews(a);
  } else {
    App_DeactivateAllViews(a);
    a->screen = ScreenBrowser;
    a->browserState.IsActive = true;
  }
}

void TopBar_OnMixer(void *ctx) {
  double now = GetTime();
  if (now - g_lastTopBarSwitchTime < 0.30) return;
  g_lastTopBarSwitchTime = now;

  App *a = (App *)ctx;
  if (a->screen == ScreenMixer) {
    a->screen = ScreenPlayer;
    App_DeactivateAllViews(a);
  } else {
    App_DeactivateAllViews(a);
    a->screen = ScreenMixer;
    a->mixerState.IsActive = true;
    a->mixerState.AudioPlugin = globalAudioEngine;
  }
}

void TopBar_OnInfo(void *ctx) {
  double now = GetTime();
  if (now - g_lastTopBarSwitchTime < 0.30) return;
  g_lastTopBarSwitchTime = now;

  App *a = (App *)ctx;
  if (a->screen == ScreenInfo) {
    a->screen = ScreenPlayer;
    App_DeactivateAllViews(a);
  } else {
    App_DeactivateAllViews(a);
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
      strcpy(it->Album, ds->AlbumName);
      strcpy(it->Genre, ds->GenreName);
      strcpy(it->Label, ds->LabelName);
      strcpy(it->MixName, ds->MixName);
      strcpy(it->Remixer, ds->Remixer);
      strcpy(it->Comment, ds->Comment);
      it->Year = ds->Year;
      it->Rating = ds->Rating;
    }
  }
}

void TopBar_OnPad(void *ctx) {
  double now = GetTime();
  if (now - g_lastTopBarSwitchTime < 0.30) return;
  g_lastTopBarSwitchTime = now;

  App *a = (App *)ctx;
  if (a->screen == ScreenPad) {
    a->screen = ScreenPlayer;
    App_DeactivateAllViews(a);
  } else {
    App_DeactivateAllViews(a);
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
    bool isShift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || a->padState.ShiftActive[deckIdx];
    int foundIdx = -1;
    for (int i = 0; i < ds->LoadedTrack->HotCuesCount; i++) {
      if (ds->LoadedTrack->HotCues[i].ID == (unsigned int)(padIdx + 1)) {
        foundIdx = i;
        break;
      }
    }

    if (isShift) {
      // DELETE HOT CUE
      if (foundIdx >= 0) {
        for (int j = foundIdx; j < ds->LoadedTrack->HotCuesCount - 1; j++) {
          ds->LoadedTrack->HotCues[j] = ds->LoadedTrack->HotCues[j + 1];
        }
        ds->LoadedTrack->HotCuesCount--;

        // Also remove from Analysis.Cues for real-time waveform updates
        for (uint32_t c = 0; c < ds->LoadedTrack->Analysis.CueCount; c++) {
          if (ds->LoadedTrack->Analysis.Cues[c].ID == (uint16_t)(padIdx + 1)) {
            for (uint32_t j = c; j < ds->LoadedTrack->Analysis.CueCount - 1; j++) {
              ds->LoadedTrack->Analysis.Cues[j] = ds->LoadedTrack->Analysis.Cues[j + 1];
            }
            ds->LoadedTrack->Analysis.CueCount--;
            break;
          }
        }
        UNX_LOG_INFO("[HOT CUE] Deleted Hot Cue %c (ID %d) on Deck %d", 'A' + padIdx, padIdx + 1, deckIdx + 1);
        const char *storagePath = a->browserState.SelectedStorage ? a->browserState.SelectedStorage->Path : NULL;
        HotCueDB_SaveTrack(storagePath, ds->LoadedTrack->FilePath, ds->TrackTitle, ds->ArtistName, ds->LoadedTrack->HotCues, ds->LoadedTrack->HotCuesCount);
      }
    } else {
      if (foundIdx >= 0) {
        // JUMP TO HOT CUE
        HotCue hc = ds->LoadedTrack->HotCues[foundIdx];
        ds->SeekMs = hc.Start;
        ds->HasSeekRequest = true;

        // Start playback immediately without motor ramp
        ds->IsPlaying = true;
        DeckAudio_ExitLoop(audio);
        DeckAudio_InstantPlay(audio);
      } else {
        // SET HOT CUE AT CURRENT POSITION
        uint32_t currentMs = (uint32_t)ds->PositionMs;

        // Quantize Snap: If Quantize is Enabled, snap marker to nearest beatgrid point
        if (ds->QuantizeEnabled && ds->LoadedTrack && ds->LoadedTrack->Analysis.BeatGridCount > 0) {
          int nearestIdx = 0;
          double minDist = 100000.0;
          for (int i = 0; i < ds->LoadedTrack->Analysis.BeatGridCount; i++) {
            double diff = fabs((double)ds->LoadedTrack->Analysis.BeatGrid[i].Time - (double)currentMs);
            if (diff < minDist) {
              minDist = diff;
              nearestIdx = i;
            }
          }
          currentMs = ds->LoadedTrack->Analysis.BeatGrid[nearestIdx].Time;
        }

        if (ds->LoadedTrack->HotCuesCount < 8) {
          static const unsigned char defaultColors[8][3] = {
              {255, 255, 0}, {255, 128, 0}, {255, 0, 255}, {255, 0, 0},
              {0, 255, 0},   {0, 204, 0},   {0, 120, 255}, {0, 120, 255}
          };

          HotCue newHc;
          memset(&newHc, 0, sizeof(HotCue));
          newHc.ID = padIdx + 1;
          newHc.Start = currentMs;
          newHc.Status = 1; // Enabled
          newHc.Color[0] = defaultColors[padIdx % 8][0];
          newHc.Color[1] = defaultColors[padIdx % 8][1];
          newHc.Color[2] = defaultColors[padIdx % 8][2];

          ds->LoadedTrack->HotCues[ds->LoadedTrack->HotCuesCount++] = newHc;

          // Sync into Analysis.Cues for real-time waveform markers
          RBCue newRc;
          memset(&newRc, 0, sizeof(RBCue));
          newRc.Time = currentMs;
          newRc.ID = padIdx + 1;
          newRc.Type = 1; // Memory / HotCue
          newRc.Status = 1;
          newRc.Color[0] = newHc.Color[0];
          newRc.Color[1] = newHc.Color[1];
          newRc.Color[2] = newHc.Color[2];

          RBCue *nextCues = (RBCue *)realloc(ds->LoadedTrack->Analysis.Cues, sizeof(RBCue) * (ds->LoadedTrack->Analysis.CueCount + 1));
          if (nextCues) {
            ds->LoadedTrack->Analysis.Cues = nextCues;
            ds->LoadedTrack->Analysis.Cues[ds->LoadedTrack->Analysis.CueCount++] = newRc;
          }
          UNX_LOG_INFO("[HOT CUE] Created Hot Cue %c (ID %d) at %u ms on Deck %d", 'A' + padIdx, padIdx + 1, currentMs, deckIdx + 1);

          const char *storagePath = a->browserState.SelectedStorage ? a->browserState.SelectedStorage->Path : NULL;
          HotCueDB_SaveTrack(storagePath, ds->LoadedTrack->FilePath, ds->TrackTitle, ds->ArtistName, ds->LoadedTrack->HotCues, ds->LoadedTrack->HotCuesCount);
        }
      }
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
        ds->LoadedTrack->Analysis.BeatGridCount > 0) {
      // Find nearest beat index
      double currentMs = (startPos / trackSR) * 1000.0;
      int nearestIdx = 0;
      double minDist = 10000.0;
      for (int i = 0; i < ds->LoadedTrack->Analysis.BeatGridCount; i++) {
        double d = fabs(ds->LoadedTrack->Analysis.BeatGrid[i].Time - currentMs);
        if (d < minDist) {
          minDist = d;
          nearestIdx = i;
        }
      }

      startPos =
          (ds->LoadedTrack->Analysis.BeatGrid[nearestIdx].Time / 1000.0) * trackSR;

      // Calculate end pos using grid if possible (assuming 1 grid entry per
      // beat)
      int beatsCount = (int)beats;
      if (beats < 1.0f) {
        // Fractional beat loop (e.g. 1/4, 1/2)
        double nextBeatMs = (nearestIdx + 1 < ds->LoadedTrack->Analysis.BeatGridCount)
                                ? ds->LoadedTrack->Analysis.BeatGrid[nearestIdx + 1].Time
                                : (ds->LoadedTrack->Analysis.BeatGrid[nearestIdx].Time +
                                   (60000.0 / ds->CurrentBPM));
        double beatDurationMs =
            nextBeatMs - ds->LoadedTrack->Analysis.BeatGrid[nearestIdx].Time;
        loopLengthSamples = (beats * beatDurationMs / 1000.0) * trackSR;
      } else {
        int endIdx = nearestIdx + beatsCount;
        if (endIdx < ds->LoadedTrack->Analysis.BeatGridCount) {
          double endMs = ds->LoadedTrack->Analysis.BeatGrid[endIdx].Time;
          loopLengthSamples =
              ((endMs - ds->LoadedTrack->Analysis.BeatGrid[nearestIdx].Time) / 1000.0) *
              trackSR;
        } else {
          // Fallback to BPM
          Log_Heartbeat((float)GetFPS(), GetFrameTime());
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
    DeckAudio_ExitLoop(audio);
    DeckAudio_JumpToMs(audio, (int64_t)(currentMs + (beats * beatDurationMs)));

    a->padState.ActiveLoopIdx[deckIdx] = -1;
  } else if (mode == PAD_MODE_GATE_CUE) {
    for (int i = 0; i < ds->LoadedTrack->HotCuesCount; i++) {
      if (ds->LoadedTrack->HotCues[i].ID == (unsigned int)(padIdx + 1)) {
        HotCue hc = ds->LoadedTrack->HotCues[i];
        ds->SeekMs = hc.Start;
        ds->HasSeekRequest = true;
        ds->IsPlaying = true;
        DeckAudio_ExitLoop(audio);
        DeckAudio_InstantPlay(audio);
        break;
      }
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
      // Echo Out (1/2, 1, 2 beats)
      static float eRatios[] = {0.5f, 1.0f, 2.0f};
      float bpm = (audio->BPM > 10.0f) ? audio->BPM : 120.0f;
      float beatMs = (60000.0f / bpm) * eRatios[padIdx - 4];
      
      if (globalAudioEngine) {
        globalAudioEngine->BeatFX.targetChannel = deckIdx + 1;
        BeatFXManager_SetFX(&globalAudioEngine->BeatFX, 1); // 1 = BEATFX_ECHO
        globalAudioEngine->BeatFX.beatMs = beatMs;
        globalAudioEngine->BeatFX.levelDepth = 0.85f; // Prominent Echo tail
        globalAudioEngine->BeatFX.isFxOn = true;
      }
      
      a->fxState.SelectedChannel = deckIdx + 1;
      a->fxState.IsFXOn = true;
      a->fxState.SelectedFX = 1; // ECHO
      a->fxState.LevelDepth = 0.85f;

      audio->ReleaseFXEchoActive = true;
    } else if (padIdx == 7) {
      // Mute / Instant Stop
      DeckAudio_Stop(audio);
      audio->VinylStopAccel = 1.0f;
    }
  }
}

void OnPadRelease(void *ctx, int deckIdx, int padIdx) {
  App *a = (App *)ctx;
  DeckAudioState *audio = &globalAudioEngine->Decks[deckIdx];
  PadMode mode = a->padState.Mode[deckIdx];

  if (mode == PAD_MODE_SLIP_LOOP) {
    DeckAudio_ExitLoop(audio);
    DeckAudio_SetSlip(audio, false);
    a->padState.ActiveLoopIdx[deckIdx] = -1;
  } else if (mode == PAD_MODE_GATE_CUE) {
    DeckAudio_InstantStop(audio);
  } else if (mode == PAD_MODE_RELEASE_FX) {
    if (padIdx >= 4 && padIdx <= 6) {
      audio->ReleaseFXEchoActive = false;
    }
  }
}

void TopBar_OnSettings(void *ctx) {
  double now = GetTime();
  if (now - g_lastTopBarSwitchTime < 0.30) return;
  g_lastTopBarSwitchTime = now;

  App *a = (App *)ctx;
  if (a->screen == ScreenSettings) {
    a->screen = ScreenPlayer;
    App_DeactivateAllViews(a);
  } else {
    App_DeactivateAllViews(a);
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

  // Init FX State (Defaults before loading)
  memset(&a->fxState, 0, sizeof(BeatFXState));
  a->fxState.LevelDepth = 0.5f;
  a->fxState.SelectedPad = 4; // 1 Beat
  a->fxState.Quantize = true;

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
  a->masterVolume = 1.0f; // default before load
  Settings_Load(&a->deckA.Waveform, &a->deckB.Waveform, &a->activeAudioConfig,
                &a->fxState, &a->colorFxDeckA, &a->colorFxDeckB,
                a->activeControllerPath, &a->deckA.QuantizeEnabled,
                &a->deckB.QuantizeEnabled, &a->masterVolume);
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

  strcpy(a->settingsState.Items[6].Label, "JOG RELEASE TIME");
  a->settingsState.Items[6].Type = SETTING_TYPE_KNOB;
  a->settingsState.Items[6].Min = 0.0f;
  a->settingsState.Items[6].Max = 16.0f;
  a->settingsState.Items[6].Step = 0.25f;
  a->settingsState.Items[6].Value = a->deckA.Waveform.VinylStartMs;
  strcpy(a->settingsState.Items[6].Unit, "Bar");
  a->settingsState.Items[6].Category = SETTING_CAT_DECK;

  strcpy(a->settingsState.Items[7].Label, "TOUCH BRAKE");
  a->settingsState.Items[7].Type = SETTING_TYPE_KNOB;
  a->settingsState.Items[7].Min = 0.0f;
  a->settingsState.Items[7].Max = 16.0f;
  a->settingsState.Items[7].Step = 0.25f;
  a->settingsState.Items[7].Value = a->deckA.Waveform.VinylStopMs;
  strcpy(a->settingsState.Items[7].Unit, "Bar");
  a->settingsState.Items[7].Category = SETTING_CAT_DECK;

  strcpy(a->settingsState.Items[8].Label, "JOG RPM");
  a->settingsState.Items[8].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[8].OptionsCount = 2;
  strcpy(a->settingsState.Items[8].Options[0], "33.3 RPM");
  strcpy(a->settingsState.Items[8].Options[1], "45.0 RPM");
  a->settingsState.Items[8].Current = (a->deckA.Waveform.JogCalibRPM > 40.0f) ? 1 : 0;
  a->settingsState.Items[8].Category = SETTING_CAT_DECK;

  // --- Audio Configurations ---
  AudioDeviceInfo devs[MAX_AUDIO_DEVICES];
  int devCount = AudioBackend_GetDevices(devs, MAX_AUDIO_DEVICES);

  strcpy(a->settingsState.Items[9].Label, "AUDIO DEVICE");
  a->settingsState.Items[9].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[9].OptionsCount = devCount + 1;
  strcpy(a->settingsState.Items[9].Options[0], "System Default");
  for (int i = 0; i < devCount && i < 31; i++) {
    // 31 characters limit for options. Format: "2CH Output Name"
    snprintf(a->settingsState.Items[9].Options[i + 1], 32, "%dCH %s",
             devs[i].NativeChannels, devs[i].Name);
  }
  a->settingsState.Items[9].Category = SETTING_CAT_AUDIO;

  // Initial population based on loaded config
  UpdateChannelOptions(a, a->activeAudioConfig.DeviceIndex);

  // Back-sync from loaded config to UI selection items
  a->settingsState.Items[9].Current = a->activeAudioConfig.DeviceIndex + 1;
  a->settingsState.Items[10].Current = a->activeAudioConfig.MasterOutL;
  a->settingsState.Items[11].Current = a->activeAudioConfig.MasterOutR;
  a->settingsState.Items[12].Current = a->activeAudioConfig.CueOutL + 1;
  a->settingsState.Items[13].Current = a->activeAudioConfig.CueOutR + 1;

  int bufMap[] = {128, 256, 512, 1024};
  a->settingsState.Items[14].Current = 1; // Default 256
  for (int i = 0; i < 4; i++) {
    if (a->activeAudioConfig.BufferSizeFrames == bufMap[i])
      a->settingsState.Items[14].Current = i;
  }
  a->settingsState.Items[15].Current =
      (a->activeAudioConfig.SampleRate == 44100) ? 0 : 1;

  strcpy(a->settingsState.Items[14].Label, "BUFFER SIZE");
  a->settingsState.Items[14].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[14].OptionsCount = 4;
  strcpy(a->settingsState.Items[14].Options[0], "128");
  strcpy(a->settingsState.Items[14].Options[1], "256");
  strcpy(a->settingsState.Items[14].Options[2], "512");
  strcpy(a->settingsState.Items[14].Options[3], "1024");
  a->settingsState.Items[14].Category = SETTING_CAT_AUDIO;

  strcpy(a->settingsState.Items[15].Label, "SAMPLE RATE");
  a->settingsState.Items[15].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[15].OptionsCount = 2;
  strcpy(a->settingsState.Items[15].Options[0], "44100 Hz");
  strcpy(a->settingsState.Items[15].Options[1], "48000 Hz");
  a->settingsState.Items[15].Category = SETTING_CAT_AUDIO;

  // Sync again to make sure labels/options are set before setting Current
  a->settingsState.Items[14].Current = 1; // Default 256
  for (int i = 0; i < 4; i++) {
    if (a->activeAudioConfig.BufferSizeFrames == bufMap[i])
      a->settingsState.Items[14].Current = i;
  }
  a->settingsState.Items[15].Current =
      (a->activeAudioConfig.SampleRate == 44100) ? 0 : 1;

  strcpy(a->settingsState.Items[16].Label, "PCM BIT DEPTH");
  a->settingsState.Items[16].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[16].OptionsCount = 2;
  strcpy(a->settingsState.Items[16].Options[0], "16-bit (RAM Save)");
  strcpy(a->settingsState.Items[16].Options[1], "24-bit (Quality)");
  a->settingsState.Items[16].Current =
      (a->activeAudioConfig.PCMBitDepth == 24) ? 1 : 0;
  a->settingsState.Items[16].Category = SETTING_CAT_AUDIO;

  strcpy(a->settingsState.Items[17].Label, "CROSSFADER CURVE");
  a->settingsState.Items[17].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[17].OptionsCount = 3;
  strcpy(a->settingsState.Items[17].Options[0], "SMOOTH (POWER)");
  strcpy(a->settingsState.Items[17].Options[1], "LINEAR");
  strcpy(a->settingsState.Items[17].Options[2], "CUTTING (SCRATCH)");
  a->settingsState.Items[17].Current = a->activeAudioConfig.CrossfaderCurve;
  a->settingsState.Items[17].Category = SETTING_CAT_AUDIO;

  strcpy(a->settingsState.Items[18].Label, "ABOUT");
  a->settingsState.Items[18].Type = SETTING_TYPE_ACTION;
  a->settingsState.Items[18].Category = SETTING_CAT_SYSTEM;

  strcpy(a->settingsState.Items[19].Label, "CREDITS");
  a->settingsState.Items[19].Type = SETTING_TYPE_ACTION;
  a->settingsState.Items[19].Category = SETTING_CAT_SYSTEM;

  strcpy(a->settingsState.Items[20].Label, "EXIT APPLICATION");
  a->settingsState.Items[20].Type = SETTING_TYPE_ACTION;
  a->settingsState.Items[20].Category = SETTING_CAT_SYSTEM;
  a->settingsState.ItemsCount = 21;

  // Set Load Lock current opt
  a->settingsState.Items[1].Current = a->deckA.Waveform.LoadLock ? 1 : 0;

  // Init Info State
  memset(&a->infoState, 0, sizeof(InfoState));
  a->infoState.IsActive = false;

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

#if defined(PLATFORM_DRM) || (defined(__linux__) && !defined(__ANDROID__))
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <errno.h>

#define MAX_EVDEV_DEVS 16

typedef struct {
    int fd;
    char path[64];
    char name[128];
    bool isTouch;
    int maxX;
    int maxY;
} EvdevDev;

static EvdevDev g_evDevs[MAX_EVDEV_DEVS];
static int g_evDevCount = 0;

static int g_rawTouchX = -1;
static int g_rawTouchY = -1;
static bool g_rawTouchDown = false;
static bool g_touchPressedEvent = false;
static bool g_touchReleasedEvent = false;

static bool g_evdevKeys[512] = { false };
static bool g_evdevKeysPressed[512] = { false };
static bool g_evdevKeysReleased[512] = { false };

static int EvdevToRaylibKey(int code) {
    switch (code) {
        case 1:   return KEY_ESCAPE;
        case 2:   return KEY_ONE;
        case 3:   return KEY_TWO;
        case 4:   return KEY_THREE;
        case 5:   return KEY_FOUR;
        case 6:   return KEY_FIVE;
        case 7:   return KEY_SIX;
        case 8:   return KEY_SEVEN;
        case 9:   return KEY_EIGHT;
        case 10:  return KEY_NINE;
        case 11:  return KEY_ZERO;
        case 14:  return KEY_BACKSPACE;
        case 15:  return KEY_TAB;
        case 28:  return KEY_ENTER;
        case 29:  return KEY_LEFT_CONTROL;
        case 42:  return KEY_LEFT_SHIFT;
        case 56:  return KEY_LEFT_ALT;
        case 57:  return KEY_SPACE;
        case 103: return KEY_UP;
        case 105: return KEY_LEFT;
        case 106: return KEY_RIGHT;
        case 108: return KEY_DOWN;
        case 30: return KEY_A; case 48: return KEY_B; case 46: return KEY_C;
        case 32: return KEY_D; case 18: return KEY_E; case 33: return KEY_F;
        case 34: return KEY_G; case 35: return KEY_H; case 23: return KEY_I;
        case 36: return KEY_J; case 37: return KEY_K; case 38: return KEY_L;
        case 50: return KEY_M; case 49: return KEY_N; case 24: return KEY_O;
        case 25: return KEY_P; case 16: return KEY_Q; case 19: return KEY_R;
        case 31: return KEY_S; case 20: return KEY_T; case 22: return KEY_U;
        case 47: return KEY_V; case 17: return KEY_W; case 45: return KEY_X;
        case 21: return KEY_Y; case 44: return KEY_Z;
        default: return 0;
    }
}

static void EvdevScanDevices(void) {
    for (int i = 0; i < 16; i++) {
        if (g_evDevCount >= MAX_EVDEV_DEVS) break;
        char devPath[64];
        snprintf(devPath, sizeof(devPath), "/dev/input/event%d", i);

        bool alreadyOpen = false;
        for (int d = 0; d < g_evDevCount; d++) {
            if (strcmp(g_evDevs[d].path, devPath) == 0) {
                alreadyOpen = true;
                break;
            }
        }
        if (alreadyOpen) continue;

        int fd = open(devPath, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        char name[128] = "Unknown Device";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        struct input_absinfo absX, absY;
        bool isTouch = false;
        int maxX = 2048, maxY = 2048;

        if (ioctl(fd, EVIOCGABS(ABS_X), &absX) >= 0 && ioctl(fd, EVIOCGABS(ABS_Y), &absY) >= 0) {
            if (absX.maximum > 0 && absY.maximum > 0) {
                isTouch = true;
                maxX = absX.maximum;
                maxY = absY.maximum;
            }
        }

        g_evDevs[g_evDevCount].fd = fd;
        strncpy(g_evDevs[g_evDevCount].path, devPath, 63);
        strncpy(g_evDevs[g_evDevCount].name, name, 127);
        g_evDevs[g_evDevCount].isTouch = isTouch;
        g_evDevs[g_evDevCount].maxX = maxX;
        g_evDevs[g_evDevCount].maxY = maxY;
        printf("[EVDEV] Listening: %s (%s) [Touch: %s]\n", devPath, name, isTouch ? "YES" : "NO");
        UNX_LOG_INFO("[EVDEV] Listening: %s (%s) [Touch: %s]", devPath, name, isTouch ? "YES" : "NO");
        g_evDevCount++;
    }
}

static void* EvdevInput_Thread(void* arg) {
    (void)arg;
    int scanCounter = 500; // Force immediate scan on thread start
    struct input_event ev[32];

    while (1) {
        if (++scanCounter >= 500) { // Every ~2 seconds (500 * 4ms)
            EvdevScanDevices();
            scanCounter = 0;
        }

        bool hadEvent = false;
        for (int d = 0; d < g_evDevCount; d++) {
            ssize_t bytes = read(g_evDevs[d].fd, ev, sizeof(ev));
            if (bytes <= 0) {
                if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    continue;
                }
                // Device read error or disconnect -> close and remove
                close(g_evDevs[d].fd);
                UNX_LOG_INFO("[EVDEV] Device disconnected or closed: %s", g_evDevs[d].path);
                for (int m = d; m < g_evDevCount - 1; m++) {
                    g_evDevs[m] = g_evDevs[m + 1];
                }
                g_evDevCount--;
                d--;
                continue;
            }
            hadEvent = true;

            int count = (int)(bytes / sizeof(struct input_event));
            for (int k = 0; k < count; k++) {
                if (ev[k].type == EV_ABS) {
                    if (ev[k].code == ABS_X || ev[k].code == ABS_MT_POSITION_X) {
                        g_rawTouchX = (ev[k].value * 1024) / g_evDevs[d].maxX;
                    } else if (ev[k].code == ABS_Y || ev[k].code == ABS_MT_POSITION_Y) {
                        g_rawTouchY = (ev[k].value * 600) / g_evDevs[d].maxY;
                    }
                } else if (ev[k].type == EV_REL) {
                    if (g_rawTouchX < 0) g_rawTouchX = 512;
                    if (g_rawTouchY < 0) g_rawTouchY = 300;
                    if (ev[k].code == REL_X) {
                        g_rawTouchX += ev[k].value;
                        if (g_rawTouchX < 0) g_rawTouchX = 0;
                        if (g_rawTouchX > 1023) g_rawTouchX = 1023;
                    } else if (ev[k].code == REL_Y) {
                        g_rawTouchY += ev[k].value;
                        if (g_rawTouchY < 0) g_rawTouchY = 0;
                        if (g_rawTouchY > 599) g_rawTouchY = 599;
                    }
                } else if (ev[k].type == EV_KEY) {
                    if (ev[k].code == BTN_TOUCH || ev[k].code == BTN_LEFT) {
                        bool down = (ev[k].value > 0);
                        if (down && !g_rawTouchDown) {
                            g_touchPressedEvent = true;
                        } else if (!down && g_rawTouchDown) {
                            g_touchReleasedEvent = true;
                        }
                        g_rawTouchDown = down;
                    } else {
                        int rKey = EvdevToRaylibKey(ev[k].code);
                        if (rKey > 0 && rKey < 512) {
                            if (ev[k].value == 1) {
                                g_evdevKeys[rKey] = true;
                                g_evdevKeysPressed[rKey] = true;
                            } else if (ev[k].value == 0) {
                                g_evdevKeys[rKey] = false;
                                g_evdevKeysReleased[rKey] = true;
                            }
                        }
                    }
                }
            }
        }
        if (!hadEvent) {
            usleep(4000);
        }
    }
    return NULL;
}

static void EvdevTouch_Init(void) {
    pthread_t threadId;
    pthread_create(&threadId, NULL, EvdevInput_Thread, NULL);
    pthread_detach(threadId);
}

static bool g_touchPressedThisFrame = false;
static bool g_touchReleasedThisFrame = false;

static void EvdevTouch_Update(void) {
    if (g_rawTouchX >= 0 && g_rawTouchY >= 0) {
        SetMousePosition(g_rawTouchX, g_rawTouchY);
    }
    g_touchPressedThisFrame = g_touchPressedEvent;
    g_touchReleasedThisFrame = g_touchReleasedEvent;
    g_touchPressedEvent = false;
    g_touchReleasedEvent = false;
}

bool Evdev_IsTouchDown(void) {
    return g_rawTouchDown;
}

bool Evdev_IsTouchPressed(void) {
    return g_touchPressedThisFrame;
}

bool Evdev_IsTouchReleased(void) {
    return g_touchReleasedThisFrame;
}

bool Evdev_IsKeyPressed(int key) {
    if (key > 0 && key < 512 && g_evdevKeysPressed[key]) {
        g_evdevKeysPressed[key] = false;
        return true;
    }
    return false;
}

bool Evdev_IsKeyDown(int key) {
    if (key > 0 && key < 512) {
        return g_evdevKeys[key];
    }
    return false;
}
#endif

#if defined(PLATFORM_IOS)
int raylib_main(int argc, char *argv[]) {
#else
int main(void) {
#endif
  SetTraceLogLevel(LOG_WARNING);
  // Log_Init moved after InitWindow for Android path stability
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
  Log_Init();
  SetTraceLogLevel(LOG_WARNING);
  printf("[MAIN] Platform: LINUX DRM/KMS\n");
  UNX_LOG_INFO("[MAIN] Platform: LINUX DRM/KMS Mode");
  InitWindow(1024, 600, APP_NAME);
  int monitor = GetCurrentMonitor();
  int monWidth = GetMonitorWidth(monitor);
  int monHeight = GetMonitorHeight(monitor);
  if (monWidth > 0 && monHeight > 0) {
    SetWindowSize(monWidth, monHeight);
  }
  if (IsWindowReady()) {
    printf("[MAIN] InitWindow SUCCESS (DRM). Size: %dx%d\n", GetScreenWidth(), GetScreenHeight());
    UNX_LOG_INFO("[DRM] InitWindow SUCCESS. Size: %dx%d", GetScreenWidth(), GetScreenHeight());
  } else {
    printf("[MAIN] InitWindow FAILED (DRM)!\n");
    UNX_LOG_ERR("[DRM] InitWindow FAILED!");
  }
  SetTargetFPS(60);
  Log_RegisterCrashHandlers();
  const char* gpuModel = (const char*)glGetString(0x1F01);
  Log_LogDeviceInfo(gpuModel);
  EvdevTouch_Init();
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
  Log_Init(); // Initialize logger after Raylib is ready (required for Android paths)
  Log_RegisterCrashHandlers();
  const char* gpuModel = (const char*)glGetString(0x1F01); // GL_RENDERER
  Log_LogDeviceInfo(gpuModel);
#else
// Desktop (X11, Wayland, Windows)
Log_Init(); 
printf("[MAIN] Platform: DESKTOP (X11/Wayland/Windows)\n");

#if defined(_WIN32) || defined(__CYGWIN__)
// Windows: Standard decorated windowed mode (1280x720 default)
SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
InitWindow(1280, 720, APP_NAME);
#else
// Linux / Armbian: Fullscreen undecorated matching monitor size
SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
InitWindow(1024, 600, APP_NAME);
int monitor = GetCurrentMonitor();
int monWidth = GetMonitorWidth(monitor);
int monHeight = GetMonitorHeight(monitor);

if (monWidth > 0 && monHeight > 0) {
    SetWindowSize(monWidth, monHeight);
    SetWindowPosition(0, 0);
}
#endif

if (IsWindowReady()) {
  printf("[MAIN] InitWindow SUCCESS. Window size: %dx%d\n", GetScreenWidth(),
         GetScreenHeight());
  UNX_LOG_INFO("[DESKTOP] InitWindow SUCCESS. Size: %dx%d", GetScreenWidth(), GetScreenHeight());
} else {
  printf("[MAIN] InitWindow FAILED!\n");
  UNX_LOG_ERR("[DESKTOP] InitWindow FAILED!");
}

SetWindowMinSize(320, 240);
SetTargetFPS(60);
Log_RegisterCrashHandlers();
const char* gpuModel = (const char*)glGetString(0x1F01); // GL_RENDERER
Log_LogDeviceInfo(gpuModel);
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

  App *app = (App *)UNX_MALLOC(sizeof(App));
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

  // MIDI_Init auto-scans connected devices and may overwrite the saved mapping.
  // Re-apply the user's saved controller preset and re-populate the UI.
  if (app->activeControllerPath[0] != '\0') {
    MIDI_RefreshMapping(app->activeControllerPath);
  }
  PopulateMidiSettings(app);

  int bufMap[] = {128, 256, 512, 1024};
  AudioBackendConfig initialAudioCfg = {
      .DeviceIndex = app->settingsState.Items[9].Current - 1,
      .MasterOutL = app->settingsState.Items[10].Current,
      .MasterOutR = app->settingsState.Items[11].Current,
      .CueOutL = app->settingsState.Items[12].Current - 1,
      .CueOutR = app->settingsState.Items[13].Current - 1,
      .SampleRate = (app->settingsState.Items[15].Current == 0) ? 44100 : 48000,
      .BufferSizeFrames = bufMap[app->settingsState.Items[14].Current],
      .PCMBitDepth = (app->settingsState.Items[16].Current == 0) ? 16 : 24,
      .CrossfaderCurve = app->settingsState.Items[17].Current};

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

  AudioEngine *audioEngine = (AudioEngine *)UNX_MALLOC(sizeof(AudioEngine));
  if (!audioEngine) {
    UNX_LOG_ERR("[CRITICAL] Failed to allocate AudioEngine on heap!");
    return -1;
  }
  AudioEngine_Init(audioEngine, initialAudioCfg.SampleRate);
  audioEngine->CrossfaderCurve = initialAudioCfg.CrossfaderCurve;

  // Restore saved Color FX and Beat FX state into Audio Engine
  audioEngine->Decks[0].ColorFX = app->colorFxDeckA;
  audioEngine->Decks[1].ColorFX = app->colorFxDeckB;
  BeatFXManager_SetFX(&audioEngine->BeatFX, app->fxState.SelectedFX);
  audioEngine->BeatFX.targetChannel = app->fxState.SelectedChannel;
  audioEngine->BeatFX.levelDepth = app->fxState.LevelDepth;
  audioEngine->BeatFX.isFxOn = app->fxState.IsFXOn;
  audioEngine->MasterVolume = app->masterVolume; // Restore saved master volume
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
  JogConfig_Load(&g_JogConfig, "jog_config.json");
  JogConfig_RegisterControlObjects();
  CO_Register("[Channel1]", "play", CO_TYPE_BOOL, &app->deckA.MidiRequestPlay, 0,
              1);
  CO_Register("[Channel1]", "cue", CO_TYPE_BOOL, &app->deckA.MidiRequestCue, 0,
              1);
  CO_Register("[Channel1]", "volume", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].Trim, 0, 1.0f);
  CO_Register("[Channel1]", "filterHigh", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqHigh, 0, 1.0f);
  CO_Register("[Channel1]", "filterMid", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqMid, 0, 1.0f);
  CO_Register("[Channel1]", "filterLow", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqLow, 0, 1.0f);
  CO_Register("[Channel1]", "cue_default", CO_TYPE_BOOL,
              &app->deckA.MidiRequestCue, 0, 1);
  CO_Register("[Channel1]", "pfl", CO_TYPE_BOOL,
              &audioEngine->Decks[0].IsCueActive, 0, 1);
  CO_Register("[Channel1]", "fader", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].Fader, 0, 1.0f);
  CO_Register("[Channel1]", "jog", CO_TYPE_DOUBLE, &app->deckA.JogDelta, -100.0f,
              100.0f);
  CO_Register("[Channel1]", "touch", CO_TYPE_BOOL, &app->deckA.IsTouching, 0, 1);
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
  CO_Register("[Channel1]", "sync_enabled", CO_TYPE_BOOL, &app->deckA.MidiRequestSync,
              0, 1);
  CO_Register("[Channel1]", "master", CO_TYPE_BOOL,
              &app->deckA.MidiRequestMaster, 0, 1);
  CO_Register("[Channel1]", "sync_leader", CO_TYPE_BOOL,
              &app->deckA.MidiRequestMaster, 0, 1);
  CO_Register("[Channel1]", "tempo_percent", CO_TYPE_FLOAT,
              &app->deckA.TempoPercent, -100.0f, 100.0f);
  CO_Register("[Channel1]", "tempo_range", CO_TYPE_INT,
              &app->deckA.TempoRange, 0, 3);


  // Loops
  CO_Register("[Channel1]", "loop_in", CO_TYPE_BOOL,
              &app->deckA.MidiRequestLoopIn, 0, 1);
  CO_Register("[Channel1]", "loop_out", CO_TYPE_BOOL,
              &app->deckA.MidiRequestLoopOut, 0, 1);
  CO_Register("[Channel1]", "loop_exit", CO_TYPE_BOOL,
              &app->deckA.MidiRequestLoopExit, 0, 1);
  CO_Register("[Channel1]", "loop_adjust_in", CO_TYPE_BOOL,
              &app->deckA.LoopAdjustIn, 0, 1);
  CO_Register("[Channel1]", "loop_adjust_out", CO_TYPE_BOOL,
              &app->deckA.LoopAdjustOut, 0, 1);
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
  CO_Register("[Channel1]", "shift", CO_TYPE_BOOL,
              &app->padState.ShiftActive[0], 0, 1);
  CO_Register("[Channel1]", "hotcue_1_clear", CO_TYPE_BOOL, &app->deckA.MidiRequestHotCueClear[0], 0, 1);
  CO_Register("[Channel1]", "hotcue_2_clear", CO_TYPE_BOOL, &app->deckA.MidiRequestHotCueClear[1], 0, 1);
  CO_Register("[Channel1]", "hotcue_3_clear", CO_TYPE_BOOL, &app->deckA.MidiRequestHotCueClear[2], 0, 1);
  CO_Register("[Channel1]", "hotcue_4_clear", CO_TYPE_BOOL, &app->deckA.MidiRequestHotCueClear[3], 0, 1);
  CO_Register("[Channel1]", "hotcue_5_clear", CO_TYPE_BOOL, &app->deckA.MidiRequestHotCueClear[4], 0, 1);
  CO_Register("[Channel1]", "hotcue_6_clear", CO_TYPE_BOOL, &app->deckA.MidiRequestHotCueClear[5], 0, 1);
  CO_Register("[Channel1]", "hotcue_7_clear", CO_TYPE_BOOL, &app->deckA.MidiRequestHotCueClear[6], 0, 1);
  CO_Register("[Channel1]", "hotcue_8_clear", CO_TYPE_BOOL, &app->deckA.MidiRequestHotCueClear[7], 0, 1);
  CO_Register("[Channel1]", "memory_set", CO_TYPE_BOOL,
              &app->deckA.MidiRequestMemoryCue, 0, 1);
  CO_Register("[Channel1]", "padmode", CO_TYPE_INT,
              &app->padState.Mode[0], 0, 5);

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

  CO_Register("[Channel2]", "play", CO_TYPE_BOOL, &app->deckB.MidiRequestPlay, 0,
              1);
  CO_Register("[Channel2]", "cue", CO_TYPE_BOOL, &app->deckB.MidiRequestCue, 0,
              1);
  CO_Register("[Channel2]", "volume", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].Trim, 0, 1.0f);
  CO_Register("[Channel2]", "filterHigh", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqHigh, 0, 1.0f);
  CO_Register("[Channel2]", "filterMid", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqMid, 0, 1.0f);
  CO_Register("[Channel2]", "filterLow", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqLow, 0, 1.0f);
  CO_Register("[Channel2]", "cue_default", CO_TYPE_BOOL,
              &app->deckB.MidiRequestCue, 0, 1);
  CO_Register("[Channel2]", "pfl", CO_TYPE_BOOL,
              &audioEngine->Decks[1].IsCueActive, 0, 1);
  CO_Register("[Channel2]", "fader", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].Fader, 0, 1.0f);
  CO_Register("[Channel2]", "jog", CO_TYPE_DOUBLE, &app->deckB.JogDelta, -100.0f,
              100.0f);
  CO_Register("[Channel2]", "touch", CO_TYPE_BOOL, &app->deckB.IsTouching, 0, 1);
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
  CO_Register("[Channel2]", "sync_enabled", CO_TYPE_BOOL, &app->deckB.MidiRequestSync,
              0, 1);
  CO_Register("[Channel2]", "master", CO_TYPE_BOOL,
              &app->deckB.MidiRequestMaster, 0, 1);
  CO_Register("[Channel2]", "sync_leader", CO_TYPE_BOOL,
              &app->deckB.MidiRequestMaster, 0, 1);
  CO_Register("[Channel2]", "tempo_percent", CO_TYPE_FLOAT,
              &app->deckB.TempoPercent, -100.0f, 100.0f);
  CO_Register("[Channel2]", "tempo_range", CO_TYPE_INT,
              &app->deckB.TempoRange, 0, 3);


  // Loops
  CO_Register("[Channel2]", "loop_in", CO_TYPE_BOOL,
              &app->deckB.MidiRequestLoopIn, 0, 1);
  CO_Register("[Channel2]", "loop_out", CO_TYPE_BOOL,
              &app->deckB.MidiRequestLoopOut, 0, 1);
  CO_Register("[Channel2]", "loop_exit", CO_TYPE_BOOL,
              &app->deckB.MidiRequestLoopExit, 0, 1);
  CO_Register("[Channel2]", "loop_adjust_in", CO_TYPE_BOOL,
              &app->deckB.LoopAdjustIn, 0, 1);
  CO_Register("[Channel2]", "loop_adjust_out", CO_TYPE_BOOL,
              &app->deckB.LoopAdjustOut, 0, 1);
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
  CO_Register("[Channel2]", "shift", CO_TYPE_BOOL,
              &app->padState.ShiftActive[1], 0, 1);
  CO_Register("[Channel2]", "hotcue_1_clear", CO_TYPE_BOOL, &app->deckB.MidiRequestHotCueClear[0], 0, 1);
  CO_Register("[Channel2]", "hotcue_2_clear", CO_TYPE_BOOL, &app->deckB.MidiRequestHotCueClear[1], 0, 1);
  CO_Register("[Channel2]", "hotcue_3_clear", CO_TYPE_BOOL, &app->deckB.MidiRequestHotCueClear[2], 0, 1);
  CO_Register("[Channel2]", "hotcue_4_clear", CO_TYPE_BOOL, &app->deckB.MidiRequestHotCueClear[3], 0, 1);
  CO_Register("[Channel2]", "hotcue_5_clear", CO_TYPE_BOOL, &app->deckB.MidiRequestHotCueClear[4], 0, 1);
  CO_Register("[Channel2]", "hotcue_6_clear", CO_TYPE_BOOL, &app->deckB.MidiRequestHotCueClear[5], 0, 1);
  CO_Register("[Channel2]", "hotcue_7_clear", CO_TYPE_BOOL, &app->deckB.MidiRequestHotCueClear[6], 0, 1);
  CO_Register("[Channel2]", "hotcue_8_clear", CO_TYPE_BOOL, &app->deckB.MidiRequestHotCueClear[7], 0, 1);
  CO_Register("[Channel2]", "memory_set", CO_TYPE_BOOL,
              &app->deckB.MidiRequestMemoryCue, 0, 1);
  CO_Register("[Channel2]", "padmode", CO_TYPE_INT,
              &app->padState.Mode[1], 0, 5);

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
              0, 2.0f);
  CO_Register("[Master]", "headphone_volume", CO_TYPE_FLOAT, &audioEngine->HeadphoneVolume,
              0, 2.0f);
  CO_Register("[Master]", "headphone_mix", CO_TYPE_FLOAT, &audioEngine->HeadphoneMix,
              0, 1.0f);
  CO_Register("[Master]", "headMix", CO_TYPE_FLOAT, &audioEngine->HeadphoneMix,
              0, 1.0f);
  CO_Register("[Library]", "scroll", CO_TYPE_FLOAT, &app->libraryScrollDelta,
              -100.0f, 100.0f);
  CO_Register("[Channel1]", "key_shift", CO_TYPE_INT, &audioEngine->Decks[0].ReleaseFXType,
              -12, 12);
  CO_Register("[Channel2]", "key_shift", CO_TYPE_INT, &audioEngine->Decks[1].ReleaseFXType,
              -12, 12);
  CO_Register("[Channel1]", "deck_layer", CO_TYPE_INT, &app->deckA.DeckLayer, 0, 1);
  CO_Register("[Channel2]", "deck_layer", CO_TYPE_INT, &app->deckB.DeckLayer, 0, 1);

  // --- Touch & Jog ---
  CO_Register("[Channel1]", "touch", CO_TYPE_BOOL, &app->deckA.IsTouching, 0, 1);
  CO_Register("[Channel2]", "touch", CO_TYPE_BOOL, &app->deckB.IsTouching, 0, 1);

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

  // --- Channel3 (Deck A Alias) & Channel4 (Deck B Alias) for 4-Deck Controllers ---
  CO_Register("[Channel3]", "play", CO_TYPE_BOOL, &app->deckA.MidiRequestPlay, 0, 1);
  CO_Register("[Channel3]", "cue", CO_TYPE_BOOL, &app->deckA.MidiRequestCue, 0, 1);
  CO_Register("[Channel3]", "cue_default", CO_TYPE_BOOL, &app->deckA.MidiRequestCue, 0, 1);
  CO_Register("[Channel3]", "pfl", CO_TYPE_BOOL, &audioEngine->Decks[0].IsCueActive, 0, 1);
  CO_Register("[Channel3]", "sync", CO_TYPE_BOOL, &app->deckA.MidiRequestSync, 0, 1);
  CO_Register("[Channel3]", "sync_enabled", CO_TYPE_BOOL, &app->deckA.MidiRequestSync, 0, 1);
  CO_Register("[Channel3]", "master", CO_TYPE_BOOL, &app->deckA.MidiRequestMaster, 0, 1);
  CO_Register("[Channel3]", "sync_leader", CO_TYPE_BOOL, &app->deckA.MidiRequestMaster, 0, 1);
  CO_Register("[Channel3]", "rate", CO_TYPE_FLOAT, &app->deckA.TempoPercent, -100.0f, 100.0f);
  CO_Register("[Channel3]", "volume", CO_TYPE_FLOAT, &audioEngine->Decks[0].Fader, 0, 1.0f);
  CO_Register("[Channel3]", "jog", CO_TYPE_DOUBLE, &app->deckA.JogDelta, -1000, 1000);
  CO_Register("[Channel3]", "touch", CO_TYPE_BOOL, &app->deckA.IsTouching, 0, 1);
  CO_Register("[Channel3]", "memory_set", CO_TYPE_BOOL, &app->deckA.MidiRequestMemoryCue, 0, 1);
  CO_Register("[Channel3]", "padmode", CO_TYPE_INT, &app->padState.Mode[0], 0, 5);

  CO_Register("[Channel4]", "play", CO_TYPE_BOOL, &app->deckB.MidiRequestPlay, 0, 1);
  CO_Register("[Channel4]", "cue", CO_TYPE_BOOL, &app->deckB.MidiRequestCue, 0, 1);
  CO_Register("[Channel4]", "cue_default", CO_TYPE_BOOL, &app->deckB.MidiRequestCue, 0, 1);
  CO_Register("[Channel4]", "pfl", CO_TYPE_BOOL, &audioEngine->Decks[1].IsCueActive, 0, 1);
  CO_Register("[Channel4]", "sync", CO_TYPE_BOOL, &app->deckB.MidiRequestSync, 0, 1);
  CO_Register("[Channel4]", "sync_enabled", CO_TYPE_BOOL, &app->deckB.MidiRequestSync, 0, 1);
  CO_Register("[Channel4]", "master", CO_TYPE_BOOL, &app->deckB.MidiRequestMaster, 0, 1);
  CO_Register("[Channel4]", "sync_leader", CO_TYPE_BOOL, &app->deckB.MidiRequestMaster, 0, 1);
  CO_Register("[Channel4]", "rate", CO_TYPE_FLOAT, &app->deckB.TempoPercent, -100.0f, 100.0f);
  CO_Register("[Channel4]", "volume", CO_TYPE_FLOAT, &audioEngine->Decks[1].Fader, 0, 1.0f);
  CO_Register("[Channel4]", "jog", CO_TYPE_DOUBLE, &app->deckB.JogDelta, -1000, 1000);
  CO_Register("[Channel4]", "touch", CO_TYPE_BOOL, &app->deckB.IsTouching, 0, 1);
  CO_Register("[Channel4]", "memory_set", CO_TYPE_BOOL, &app->deckB.MidiRequestMemoryCue, 0, 1);
  CO_Register("[Channel4]", "padmode", CO_TYPE_INT, &app->padState.Mode[1], 0, 5);

  // --- Beat FX ---
  CO_Register("[Master]", "beatfx_select", CO_TYPE_INT,
              &audioEngine->BeatFX.activeFX, 0, 13);
  CO_Register("[Master]", "beatfx_drywet", CO_TYPE_FLOAT,
              &audioEngine->BeatFX.levelDepth, 0, 1.0f);
  CO_Register("[Master]", "beatfx_time", CO_TYPE_FLOAT,
              &audioEngine->BeatFX.beatMs, 0, 2000.0f);
  CO_Register("[Master]", "beatfx_on", CO_TYPE_BOOL,
              &app->fxState.IsFXOn, 0, 1);
  CO_Register("[Master]", "beatfx_channel", CO_TYPE_INT,
              &audioEngine->BeatFX.targetChannel, 0, 4);
  CO_Register("[Master]", "beatfx_prev", CO_TYPE_BOOL,
              &app->fxState.MidiRequestPrevFX, 0, 1);
  CO_Register("[Master]", "beatfx_next", CO_TYPE_BOOL,
              &app->fxState.MidiRequestNextFX, 0, 1);
  CO_Register("[Master]", "beatfx_toggle", CO_TYPE_BOOL,
              &app->fxState.MidiRequestToggleFX, 0, 1);
  CO_Register("[Master]", "beatfx_ch1", CO_TYPE_BOOL,
              &app->fxState.MidiRequestCh1, 0, 1);
  CO_Register("[Master]", "beatfx_ch2", CO_TYPE_BOOL,
              &app->fxState.MidiRequestCh2, 0, 1);
  CO_Register("[Master]", "beatfx_ch3", CO_TYPE_BOOL,
              &app->fxState.MidiRequestCh3, 0, 1);
  CO_Register("[Master]", "beatfx_ch4", CO_TYPE_BOOL,
              &app->fxState.MidiRequestCh4, 0, 1);
  CO_Register("[Master]", "beatfx_chmaster", CO_TYPE_BOOL,
              &app->fxState.MidiRequestChMaster, 0, 1);
  CO_Register("[Master]", "beatfx_beat_left", CO_TYPE_BOOL,
              &app->fxState.MidiRequestBeatLeft, 0, 1);
  CO_Register("[Master]", "beatfx_beat_right", CO_TYPE_BOOL,
              &app->fxState.MidiRequestBeatRight, 0, 1);
  CO_Register("[Master]", "beatfx_tap", CO_TYPE_BOOL,
              &app->fxState.MidiRequestTap, 0, 1);


  // --- Library / Browser ---
  CO_Register("[Library]", "browse", CO_TYPE_INT,
              &app->browserState.MidiBrowseDelta, -10, 10);
  CO_Register("[Library]", "enter", CO_TYPE_BOOL,
              &app->browserState.MidiRequestEnter, 0, 1);
  CO_Register("[Library]", "MoveFocusForward", CO_TYPE_BOOL,
              &app->browserState.MidiRequestMoveFocusForward, 0, 1);
  CO_Register("[Library]", "back", CO_TYPE_BOOL,
              &app->browserState.MidiRequestBack, 0, 1);
  CO_Register("[Library]", "MoveFocusBackward", CO_TYPE_BOOL,
              &app->browserState.MidiRequestMoveFocusBackward, 0, 1);
  CO_Register("[Library]", "up", CO_TYPE_BOOL, &app->browserState.MidiRequestUp,
              0, 1);
  CO_Register("[Library]", "down", CO_TYPE_BOOL,
              &app->browserState.MidiRequestDown, 0, 1);
  CO_Register("[Library]", "loadA", CO_TYPE_BOOL,
              &app->browserState.MidiRequestLoadA, 0, 1);
  CO_Register("[Library]", "loadB", CO_TYPE_BOOL,
              &app->browserState.MidiRequestLoadB, 0, 1);

  // --- Settings Navigation ---
  CO_Register("[Settings]", "browse", CO_TYPE_INT,
              &app->settingsState.MidiBrowseDelta, -10, 10);
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
  CO_Register("[Master]", "waveform_zoom_step", CO_TYPE_INT,
              &app->MidiWaveformZoomStep, -10, 10);
  CO_Register("[Master]", "waveform_zoom_in", CO_TYPE_BOOL,
              &app->MidiWaveformZoomIn, 0, 1);
  CO_Register("[Master]", "waveform_zoom_out", CO_TYPE_BOOL,
              &app->MidiWaveformZoomOut, 0, 1);

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
  App_SaveSettings(app);
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

// ---------------------------------------------------------------------------
// Async Artwork Loader (BUG-02 + BUG-07)
// Disk I/O + decode runs in a background thread via artwork_loader.cpp.
// This file stays pure C; only the .cpp module uses std::thread.
// ---------------------------------------------------------------------------

// BUG-C FIX: ArtworkPending struct and ArtworkLoader_* forward decls moved to artwork_loader.h
#include "artwork_loader.h"

static ArtworkPending g_artPending[2];
static bool           g_artPendingInit = false;

static int ArtworkSlotForDeck(DeckState *ds) {
  static DeckState *reg[2] = {NULL, NULL};
  for (int i = 0; i < 2; i++) {
    if (reg[i] == ds) return i;
    if (reg[i] == NULL) { reg[i] = ds; return i; }
  }
  return 0;
}

void ManageArtwork(DeckState *ds) {
  if (!g_artPendingInit) {
    memset(g_artPending, 0, sizeof(g_artPending));
    g_artPendingInit = true;
  }

  int slot = ArtworkSlotForDeck(ds);
  ArtworkPending *pend = &g_artPending[slot];

  // Upload phase: background thread finished, upload to GPU on render thread
  if (pend->ready && !pend->loading) {
    if (ds->ArtworkTexture.id != 0)
      UnloadTexture(ds->ArtworkTexture);
    ds->ArtworkTexture = (Texture2D){0};

    if (pend->data) {
      Image img = {
        .data    = pend->data,
        .width   = pend->w,
        .height  = pend->h,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
      };
      ds->ArtworkTexture = LoadTextureFromImage(img);
      if (ds->ArtworkTexture.id != 0)
        SetTextureFilter(ds->ArtworkTexture, TEXTURE_FILTER_BILINEAR);
      free(pend->data);
      pend->data = NULL;
    }
    strncpy(ds->LastLoadedArtPath, pend->path, 511);
    pend->ready = false;
    return;
  }

  // Nothing to do if path unchanged
  if (strcmp(ds->ArtworkPath, ds->LastLoadedArtPath) == 0)
    return;
  // Already loading this path — wait for background thread
  if (pend->loading)
    return;

  // Stale data cleanup
  if (pend->data) { free(pend->data); pend->data = NULL; }
  pend->ready = false;

  // Empty path — clear texture immediately
  if (ds->ArtworkPath[0] == '\0') {
    if (ds->ArtworkTexture.id != 0)
      UnloadTexture(ds->ArtworkTexture);
    ds->ArtworkTexture = (Texture2D){0};
    strncpy(ds->LastLoadedArtPath, ds->ArtworkPath, 511);
    return;
  }

  // Normalize path
  char fixedPath[512];
  strncpy(fixedPath, ds->ArtworkPath, 511);
  fixedPath[511] = '\0';
  for (int p = 0; fixedPath[p]; p++)
    if (fixedPath[p] == '\\') fixedPath[p] = '/';

  if (!FileExists(fixedPath)) {
    UNX_LOG_WARN("[ARTWORK] File NOT FOUND: '%s'", fixedPath);
    strncpy(ds->LastLoadedArtPath, ds->ArtworkPath, 511);
    return;
  }

  // Kick background thread (implemented in artwork_loader.cpp)
  pend->loading = true;
  strncpy(pend->path, ds->ArtworkPath, 511);
  ArtworkLoader_Kick(pend, fixedPath);
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

  Log_Heartbeat((float)GetFPS(), GetFrameTime());
  UI_UpdateTouchState();

  // CPU Throttling / Stall Detection
  float dt = GetFrameTime();
  if (dt > 0.1f) { // More than 100ms frame
    UNX_LOG_WARN("[PERF] [GUI] CPU Stall Detected: Frame took %.2f ms (Throttling?)", dt * 1000.0f);
  }

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
  app->deckA.IsLoading = audioEngine->Decks[0].IsLoading;
  app->deckB.IsLoading = audioEngine->Decks[1].IsLoading;
  app->deckA.LoadingProgress = audioEngine->Decks[0].LoadingProgress;
  app->deckB.LoadingProgress = audioEngine->Decks[1].LoadingProgress;
  app->deckA.IsLooping = audioEngine->Decks[0].IsLooping;
  app->deckB.IsLooping = audioEngine->Decks[1].IsLooping;
  if (!app->deckA.IsLooping) {
    app->padState.ActiveLoopIdx[0] = -1;
    app->deckA.LoopAdjustIn = false;
    app->deckA.LoopAdjustOut = false;
  }
  if (!app->deckB.IsLooping) {
    app->padState.ActiveLoopIdx[1] = -1;
    app->deckB.LoopAdjustIn = false;
    app->deckB.LoopAdjustOut = false;
  }

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

  // --- Auto Stop at End of Track / Beatgrid & Empty Deck Guard ---
  for (int i = 0; i < 2; i++) {
    DeckState *ds = (i == 0) ? &app->deckA : &app->deckB;
    if (!ds->LoadedTrack) {
      ds->IsPlaying = false;
      ds->IsCueActive = false;
      audioEngine->Decks[i].IsPlaying = false;
    } else if (ds->IsPlaying) {
      long long endMs = ds->TrackLengthMs;
      if (ds->LoadedTrack->Analysis.BeatGridCount > 0) {
        endMs = (long long)ds->LoadedTrack
                    ->Analysis.BeatGrid[ds->LoadedTrack->Analysis.BeatGridCount - 1]
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

#if defined(PLATFORM_DRM) || (defined(__linux__) && !defined(__ANDROID__))
  EvdevTouch_Update();
#endif

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

  // Force Instant Snap when Sync is turned ON to BEAT mode
  if (app->deckA.SyncMode == 2 && lastSyncA != 2 && app->deckB.IsMaster) {
      Sync_RequestPhaseSnap(&app->deckA, &app->deckB, audioEngine);
  }
  if (app->deckB.SyncMode == 2 && lastSyncB != 2 && app->deckA.IsMaster) {
      Sync_RequestPhaseSnap(&app->deckB, &app->deckA, audioEngine);
  }

  lastSyncA = app->deckA.SyncMode;
  lastSyncB = app->deckB.SyncMode;

  // Auto Takeover: If Master stops or is unloaded, other deck becomes Master if playing
  if (app->deckA.IsMaster && (!app->deckA.IsPlaying || !app->deckA.LoadedTrack) && app->deckB.IsPlaying && app->deckB.LoadedTrack) {
    app->deckA.IsMaster = false;
    app->deckB.IsMaster = true;
  } else if (app->deckB.IsMaster && (!app->deckB.IsPlaying || !app->deckB.LoadedTrack) &&
             app->deckA.IsPlaying && app->deckA.LoadedTrack) {
    app->deckB.IsMaster = false;
    app->deckA.IsMaster = true;
  }

  // Ensure Exclusivity
  if (app->deckA.IsMaster && !lastMasterA)
    app->deckB.IsMaster = false;
  if (app->deckB.IsMaster && !lastMasterB)
    app->deckA.IsMaster = false;
  if (app->deckB.IsMaster && !lastMasterB)
    app->deckA.IsMaster = false;
  lastMasterA = app->deckA.IsMaster;
  lastMasterB = app->deckB.IsMaster;

  // Skip deck/mixer keyboard shortcuts when browser is open or search is focused
  if (!app->browserState.IsActive) {
    // Safety: ensure focus is cleared when browser is not active
    app->browserState.IsSearching = false; 
    
    HandleKeyboardInputs(&app->keyMap, &app->deckA, &app->deckB, audioEngine,
                         &app->fxState);
  }
  MIDI_Update(&app->midiCtx, &app->deckA, &app->deckB, audioEngine);

  // Process MIDI Beat FX requests
  if (app->fxState.MidiRequestPrevFX) {
      app->fxState.SelectedFX = (app->fxState.SelectedFX + 13) % 14;
      BeatFXManager_SetFX(&audioEngine->BeatFX, app->fxState.SelectedFX);
      app->fxState.MidiRequestPrevFX = false;
  }
  if (app->fxState.MidiRequestNextFX) {
      app->fxState.SelectedFX = (app->fxState.SelectedFX + 1) % 14;
      BeatFXManager_SetFX(&audioEngine->BeatFX, app->fxState.SelectedFX);
      app->fxState.MidiRequestNextFX = false;
  }
  if (app->fxState.MidiRequestToggleFX) {
      app->fxState.IsFXOn = !app->fxState.IsFXOn;
      BeatFXManager_SetFXOn(&audioEngine->BeatFX, app->fxState.IsFXOn);
      app->fxState.MidiRequestToggleFX = false;
  }
  if (app->fxState.MidiRequestCh1) {
      audioEngine->BeatFX.targetChannel = 0;
      app->fxState.MidiRequestCh1 = false;
  }
  if (app->fxState.MidiRequestCh2) {
      audioEngine->BeatFX.targetChannel = 1;
      app->fxState.MidiRequestCh2 = false;
  }
  if (app->fxState.MidiRequestCh3) {
      audioEngine->BeatFX.targetChannel = 0;
      app->fxState.MidiRequestCh3 = false;
  }
  if (app->fxState.MidiRequestCh4) {
      audioEngine->BeatFX.targetChannel = 1;
      app->fxState.MidiRequestCh4 = false;
  }
  if (app->fxState.MidiRequestChMaster) {
      audioEngine->BeatFX.targetChannel = 2;
      app->fxState.MidiRequestChMaster = false;
  }
  if (app->fxState.MidiRequestBeatLeft) {
      BeatFXManager_AdjustTimeMultiplier(&audioEngine->BeatFX, 0.5f);
      app->fxState.MidiRequestBeatLeft = false;
  }
  if (app->fxState.MidiRequestBeatRight) {
      BeatFXManager_AdjustTimeMultiplier(&audioEngine->BeatFX, 2.0f);
      app->fxState.MidiRequestBeatRight = false;
  }
  if (app->fxState.MidiRequestTap) {
      BeatFXManager_AdjustTimeMultiplier(&audioEngine->BeatFX, 1.0f);
      app->fxState.MidiRequestTap = false;
  }

  // Send hardware LED and VU Meter updates via MIDI
  MIDI_UpdateLEDs(&app->midiCtx, &app->deckA, &app->deckB, audioEngine, app);


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
  if (app->fxState.IsFXOn != audioEngine->BeatFX.isFxOn) {
      BeatFXManager_SetFXOn(&audioEngine->BeatFX, app->fxState.IsFXOn);
  }
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

  // Handle Library Up/Down requests
  if (app->browserState.MidiRequestUp) {
    app->browserState.MidiBrowseDelta = -1;
    app->browserState.MidiRequestUp = false;
  }
  if (app->browserState.MidiRequestDown) {
    app->browserState.MidiBrowseDelta = 1;
    app->browserState.MidiRequestDown = false;
  }

  // Route Encoder & MIDI signals to Settings FIRST when Settings screen is active
  if (app->screen == ScreenSettings || app->settingsState.IsActive) {
    if (app->browserState.MidiBrowseDelta != 0) {
      app->settingsState.MidiBrowseDelta += app->browserState.MidiBrowseDelta;
      app->browserState.MidiBrowseDelta = 0;
    }
    if (app->browserState.MidiRequestEnter) {
      app->settingsState.MidiRequestEnter = true;
      app->browserState.MidiRequestEnter = false;
    }
    if (app->browserState.MidiRequestBack) {
      if (app->settingsState.IsDropdownOpen) {
        app->settingsState.IsDropdownOpen = false;
        app->settingsState.FocusLevel = 1;
      } else if (app->settingsState.IsEditMappingOpen) {
        app->settingsState.IsEditMappingOpen = false;
      } else if (app->settingsState.IsMappingListOpen) {
        app->settingsState.IsMappingListOpen = false;
      } else if (app->settingsState.FocusLevel == 2) {
        app->settingsState.FocusLevel = 1;
      } else if (app->settingsState.FocusLevel == 1) {
        app->settingsState.FocusLevel = 0;
      } else {
        app->screen = ScreenPlayer;
        app->settingsState.IsActive = false;
      }
      app->browserState.MidiRequestBack = false;
    }
  }

  // --- MIDI UI Navigation (Only processed if not consumed by Settings) ---
  bool _searchFocused = app->browserState.IsActive && app->browserState.IsSearching;
  if (app->MidiRequestBrowser || (!_searchFocused && IsKeyPressed(app->keyMap.toggleBrowser))) {
    TopBar_OnBrowse(app);
    app->MidiRequestBrowser = false;
  }
  if (app->browserState.MidiRequestEnter) {
    if (app->screen != ScreenBrowser) {
      TopBar_OnBrowse(app);
      app->browserState.MidiRequestEnter = false;
    } else if (app->browserState.BrowseLevel == 0 && !app->browserState.ShowLoadPopup) {
      TopBar_OnBrowse(app);
      app->browserState.MidiRequestEnter = false;
    }
  }
  if (app->browserState.MidiRequestBack) {
    if (app->screen == ScreenBrowser) {
      if (app->browserState.BrowseLevel > 0) {
        app->browserState.BrowseLevel--;
      } else {
        TopBar_OnBrowse(app);
      }
    } else {
      TopBar_OnBrowse(app);
    }
    app->browserState.MidiRequestBack = false;
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
  if (app->MidiWaveformZoomStep != 0) {
    int steps = app->MidiWaveformZoomStep;
    app->MidiWaveformZoomStep = 0;
    if (steps > 0) {
      for (int i = 0; i < steps; i++) {
        Waveform_AdjustZoom(&app->deckA, 1);
        Waveform_AdjustZoom(&app->deckB, 1);
      }
    } else {
      for (int i = 0; i < -steps; i++) {
        Waveform_AdjustZoom(&app->deckA, -1);
        Waveform_AdjustZoom(&app->deckB, -1);
      }
    }
  }
  if (app->MidiWaveformZoomIn) {
    Waveform_AdjustZoom(&app->deckA, 1);
    Waveform_AdjustZoom(&app->deckB, 1);
    app->MidiWaveformZoomIn = false;
  }
  if (app->MidiWaveformZoomOut) {
    Waveform_AdjustZoom(&app->deckA, -1);
    Waveform_AdjustZoom(&app->deckB, -1);
    app->MidiWaveformZoomOut = false;
  }

  // Handle Deck MIDI Requests
  for (int i = 0; i < 2; i++) {
    DeckState *ds = (i == 0) ? &app->deckA : &app->deckB;
    DeckAudioState *audio = &audioEngine->Decks[i];

    // Process MIDI Play Trigger
    static bool lastMidiPlay[2] = {false};
    if (ds->MidiRequestPlay && !lastMidiPlay[i]) {
      if (ds->IsCueHeld) {
        ds->IsCueHeld = false;
        ds->IsPlaying = true;
      } else {
        bool target = !audio->IsMotorOn;
        DeckAudio_SetPlaying(audio, target);
        ds->IsPlaying = target;
        if (target && ds->SyncMode == 2 && !ds->IsMaster) {
          DeckState *otherDeck = (i == 0) ? &app->deckB : &app->deckA;
          if (otherDeck->IsPlaying && otherDeck->IsMaster) {
            Sync_RequestPhaseSnap(ds, otherDeck, audioEngine);
          }
        }
      }
    }
    lastMidiPlay[i] = ds->MidiRequestPlay;

    // Process MIDI Cue Trigger
    static bool lastMidiCue[2] = {false};
    if (ds->MidiRequestCue && !lastMidiCue[i]) {
      // Press
      if (audio->IsMotorOn) {
        DeckAudio_InstantStop(audio);
        DeckAudio_ExitLoop(audio);
        ds->SeekMs = ds->MainCueMs;
        ds->HasSeekRequest = true;
        ds->PositionMs = ds->MainCueMs;
        ds->IsPlaying = false;
      } else {
        if (ds->PositionMs != ds->MainCueMs) {
          if (ds->QuantizeEnabled && ds->LoadedTrack) {
            ds->MainCueMs =
                Quantize_GetNearestBeatMs(ds->LoadedTrack, ds->PositionMs);
          } else {
            ds->MainCueMs = ds->PositionMs;
          }
        }
        DeckAudio_InstantPlay(audio);
        ds->IsPlaying = true;
        ds->IsCueHeld = true;
      }
    } else if (!ds->MidiRequestCue && lastMidiCue[i]) {
      // Release
      if (ds->IsCueHeld) {
        DeckAudio_InstantStop(audio);
        ds->SeekMs = ds->MainCueMs;
        ds->HasSeekRequest = true;
        ds->PositionMs = ds->MainCueMs;
        ds->IsPlaying = false;
        ds->IsCueHeld = false;
      }
    }
    lastMidiCue[i] = ds->MidiRequestCue;

    // Hot Cues
    for (int j = 0; j < 8; j++) {
      if (ds->MidiRequestHotCueClear[j]) {
        bool oldShift = app->padState.ShiftActive[i];
        app->padState.ShiftActive[i] = true;
        OnPadPress(app, i, j);
        app->padState.ShiftActive[i] = oldShift;
        ds->MidiRequestHotCueClear[j] = false;
      } else if (ds->MidiRequestHotCue[j]) {
        OnPadPress(app, i, j);
        ds->MidiRequestHotCue[j] = false;
      }
    }

    // Manual Looping (with Quantize Beatgrid Snapping)
    if (ds->MidiRequestLoopIn) {
      double trackSR = (double)audio->SampleRate;
      if (trackSR < 100) trackSR = 44100.0;
      double pos = audio->Position;

      if (ds->QuantizeEnabled && ds->LoadedTrack && ds->LoadedTrack->Analysis.BeatGridCount > 0) {
        double currentMs = (pos / trackSR) * 1000.0;
        int nearestIdx = 0;
        double minDist = 100000.0;
        for (int b = 0; b < ds->LoadedTrack->Analysis.BeatGridCount; b++) {
          double dist = fabs((double)ds->LoadedTrack->Analysis.BeatGrid[b].Time - currentMs);
          if (dist < minDist) {
            minDist = dist;
            nearestIdx = b;
          }
        }
        pos = (ds->LoadedTrack->Analysis.BeatGrid[nearestIdx].Time / 1000.0) * trackSR;
      }

      if (audio->IsLooping) {
        // Toggle Loop In Adjust mode if inside active loop (Mixxx style)
        ds->LoopAdjustIn = !ds->LoopAdjustIn;
        ds->LoopAdjustOut = false;
      } else {
        audio->LoopStartPos = pos;
        if (audio->LoopEndPos <= audio->LoopStartPos) {
          audio->LoopEndPos = audio->LoopStartPos + trackSR * 4.0;
        }
      }
      ds->MidiRequestLoopIn = false;
    }

    if (ds->MidiRequestLoopOut) {
      double trackSR = (double)audio->SampleRate;
      if (trackSR < 100) trackSR = 44100.0;
      double pos = audio->Position;

      if (audio->IsLooping) {
        if (ds->LoopAdjustIn || ds->LoopAdjustOut) {
          // Exit loop adjust mode
          ds->LoopAdjustIn = false;
          ds->LoopAdjustOut = false;
        } else {
          // Toggle Loop Out Adjust mode (Mixxx style)
          ds->LoopAdjustOut = true;
          ds->LoopAdjustIn = false;
        }
      } else {
        if (ds->QuantizeEnabled && ds->LoadedTrack && ds->LoadedTrack->Analysis.BeatGridCount > 0) {
          double currentMs = (pos / trackSR) * 1000.0;
          int nearestIdx = 0;
          double minDist = 100000.0;
          for (int b = 0; b < ds->LoadedTrack->Analysis.BeatGridCount; b++) {
            double dist = fabs((double)ds->LoadedTrack->Analysis.BeatGrid[b].Time - currentMs);
            if (dist < minDist) {
              minDist = dist;
              nearestIdx = b;
            }
          }
          pos = (ds->LoadedTrack->Analysis.BeatGrid[nearestIdx].Time / 1000.0) * trackSR;
        }

        if (pos <= audio->LoopStartPos) {
          double beatMs = 60000.0 / (ds->CurrentBPM > 0 ? ds->CurrentBPM : 120.0);
          pos = audio->LoopStartPos + (beatMs / 1000.0) * trackSR;
        }

        DeckAudio_SetLoop(audio, true, audio->LoopStartPos, pos);
        ds->IsLooping = true;
      }
      ds->MidiRequestLoopOut = false;
    }

    if (ds->MidiRequestLoopExit) {
      ds->LoopAdjustIn = false;
      ds->LoopAdjustOut = false;
      if (audio->IsLooping) {
        DeckAudio_ExitLoop(audio);
        ds->IsLooping = false;
      } else if (audio->LoopEndPos > audio->LoopStartPos) {
        DeckAudio_SetLoop(audio, true, audio->LoopStartPos, audio->LoopEndPos);
        audio->Position = audio->LoopStartPos;
        audio->MT_ReadPos = audio->LoopStartPos;
        DeckAudio_ClearMT(audio);
        ds->IsLooping = true;
      }
      ds->MidiRequestLoopExit = false;
    }

    // Static debouncing timers for Cue Call Left/Right and Memory Cue buttons (200ms threshold)
    static double lastMemoryPress[2] = {0.0, 0.0};
    static double lastCueCallLeftPress[2] = {0.0, 0.0};
    static double lastCueCallRightPress[2] = {0.0, 0.0};
    double nowTime = GetTime();

    // Cue / Loop Call (Left = Halve / Jump Prev Cue, Right = Double / Jump Next Cue)
    if (ds->MidiRequestLoopHalve) {
      ds->MidiRequestLoopHalve = false;
      if (nowTime - lastCueCallLeftPress[i] >= 0.250) {
        lastCueCallLeftPress[i] = nowTime;
        if (audio->IsLooping) {
          double len = audio->LoopEndPos - audio->LoopStartPos;
          if (len > 2.0) {
            DeckAudio_SetLoop(audio, true, audio->LoopStartPos, audio->LoopStartPos + len / 2.0);
          }
        } else if (ds->LoadedTrack && ds->LoadedTrack->Analysis.CueCount > 0) {
          uint32_t currentMs = (uint32_t)ds->PositionMs;
          int prevMs = -1;
          for (uint32_t c = 0; c < ds->LoadedTrack->Analysis.CueCount; c++) {
            if (ds->LoadedTrack->Analysis.Cues[c].Time < currentMs - 100) {
              if ((int)ds->LoadedTrack->Analysis.Cues[c].Time > prevMs) {
                prevMs = ds->LoadedTrack->Analysis.Cues[c].Time;
              }
            }
          }
          if (prevMs >= 0) {
            ds->SeekMs = prevMs;
            ds->HasSeekRequest = true;
          }
        }
      }
    }

    if (ds->MidiRequestLoopDouble) {
      ds->MidiRequestLoopDouble = false;
      if (nowTime - lastCueCallRightPress[i] >= 0.250) {
        lastCueCallRightPress[i] = nowTime;
        if (audio->IsLooping) {
          double len = audio->LoopEndPos - audio->LoopStartPos;
          DeckAudio_SetLoop(audio, true, audio->LoopStartPos, audio->LoopStartPos + len * 2.0);
        } else if (ds->LoadedTrack && ds->LoadedTrack->Analysis.CueCount > 0) {
          uint32_t currentMs = (uint32_t)ds->PositionMs;
          uint32_t nextMs = 0xFFFFFFFF;
          for (uint32_t c = 0; c < ds->LoadedTrack->Analysis.CueCount; c++) {
            if (ds->LoadedTrack->Analysis.Cues[c].Time > currentMs + 100) {
              if (ds->LoadedTrack->Analysis.Cues[c].Time < nextMs) {
                nextMs = ds->LoadedTrack->Analysis.Cues[c].Time;
              }
            }
          }
          if (nextMs != 0xFFFFFFFF) {
            ds->SeekMs = nextMs;
            ds->HasSeekRequest = true;
          }
        }
      }
    }

    // Memory Cue Toggle (Set or Clear Memory Cue marker) with 200ms debounce
    if (ds->MidiRequestMemoryCue) {
      if (nowTime - lastMemoryPress[i] < 0.200) {
        ds->MidiRequestMemoryCue = false;
      } else {
        lastMemoryPress[i] = nowTime;
        if (ds->LoadedTrack) {
          uint32_t currentMs = (uint32_t)ds->PositionMs;
          if (ds->QuantizeEnabled && ds->LoadedTrack->Analysis.BeatGridCount > 0) {
            currentMs = Quantize_GetNearestBeatMs(ds->LoadedTrack, currentMs);
          }

          int existingIdx = -1;
          for (uint32_t c = 0; c < ds->LoadedTrack->Analysis.CueCount; c++) {
            if (abs((int)ds->LoadedTrack->Analysis.Cues[c].Time - (int)currentMs) < 150) {
              existingIdx = (int)c;
              break;
            }
          }

          if (existingIdx >= 0) {
            for (uint32_t c = (uint32_t)existingIdx; c < ds->LoadedTrack->Analysis.CueCount - 1; c++) {
              ds->LoadedTrack->Analysis.Cues[c] = ds->LoadedTrack->Analysis.Cues[c + 1];
            }
            ds->LoadedTrack->Analysis.CueCount--;
            UNX_LOG_INFO("[MEMORY CUE] Cleared Memory Cue at %u ms on Deck %d", currentMs, i + 1);
          } else {
            RBCue newRc;
            memset(&newRc, 0, sizeof(RBCue));
            newRc.Time = currentMs;
            newRc.ID = 0;
            newRc.Type = 0; // Memory Cue
            newRc.Status = 1;
            newRc.Color[0] = 255; newRc.Color[1] = 165; newRc.Color[2] = 0; // Orange

            RBCue *nextCues = (RBCue *)realloc(ds->LoadedTrack->Analysis.Cues, sizeof(RBCue) * (ds->LoadedTrack->Analysis.CueCount + 1));
            if (nextCues) {
              ds->LoadedTrack->Analysis.Cues = nextCues;
              ds->LoadedTrack->Analysis.Cues[ds->LoadedTrack->Analysis.CueCount++] = newRc;
            }
            UNX_LOG_INFO("[MEMORY CUE] Set Memory Cue at %u ms on Deck %d", currentMs, i + 1);
          }
        }
        ds->MidiRequestMemoryCue = false;
      }
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
      ds->SyncMode = (ds->SyncMode + 1) % 3;
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
        double currentBpm = ds->CurrentBPM;
        if (currentBpm < 1.0) currentBpm = 120.0;
        double beatMs = (60000.0f / currentBpm);
        double loopLenFrames =
            (beatMs * loopSizes[k] * audio->SampleRate) / 1000.0f;
        DeckAudio_SetLoop(audio, true, audio->Position,
                          audio->Position + loopLenFrames);
        ds->MidiRequestAutoLoop[k] = false;
      }
    }
  }

  // --- Sync Control Logic ---
  Sync_Update(&app->deckA, &app->deckB, audioEngine);

  // Tempo Calculation (10000 = 100%)
  // Base pitch determines current track BPM (stable display, no flickering)
  float basePitchA = 1.0f + (app->deckA.TempoPercent / 100.0f);
  app->deckA.CurrentBPM = app->deckA.OriginalBPM * basePitchA;
  float realPitchA = basePitchA + app->deckA.LastPhaseAdjustment;
  audioEngine->Decks[0].Pitch = (uint16_t)(realPitchA * 10000.0f);

  float basePitchB = 1.0f + (app->deckB.TempoPercent / 100.0f);
  app->deckB.CurrentBPM = app->deckB.OriginalBPM * basePitchB;
  float realPitchB = basePitchB + app->deckB.LastPhaseAdjustment;
  audioEngine->Decks[1].Pitch = (uint16_t)(realPitchB * 10000.0f);

  // --- Sync UI Jog/Modes back to Audio Engine ---
  // Sync state flags first so physics processing has up-to-date deck flags
  audioEngine->Decks[0].VinylModeEnabled = app->deckA.VinylModeEnabled;
  audioEngine->Decks[0].MasterTempoActive = app->deckA.MasterTempo;
  audioEngine->Decks[0].BPM = app->deckA.CurrentBPM;

  audioEngine->Decks[1].VinylModeEnabled = app->deckB.VinylModeEnabled;
  audioEngine->Decks[1].MasterTempoActive = app->deckB.MasterTempo;
  audioEngine->Decks[1].BPM = app->deckB.CurrentBPM;

  // Deck A
  bool effTouchA = app->deckA.IsTouching && !app->deckA.LoopAdjustIn && !app->deckA.LoopAdjustOut;
  if (effTouchA != audioEngine->Decks[0].IsTouching) {
    bool released = !effTouchA && audioEngine->Decks[0].IsTouching;
    DeckAudio_SetJogTouch(&audioEngine->Decks[0], effTouchA);

    // Phase Snap on release if Beat Sync is ON
    if (released && app->deckA.SyncMode == 2 && !app->deckA.IsMaster) {
      Sync_RequestPhaseSnap(&app->deckA, &app->deckB, audioEngine);
    }
  }

  if (app->deckA.LoopAdjustIn && app->deckA.JogDelta != 0) {
    double trackSR = (double)audioEngine->Decks[0].SampleRate;
    if (trackSR < 100) trackSR = 44100.0;
    double deltaSamples = app->deckA.JogDelta * (trackSR / 400.0);
    double newStart = audioEngine->Decks[0].LoopStartPos + deltaSamples;
    if (newStart < 0) newStart = 0;
    if (newStart > audioEngine->Decks[0].LoopEndPos - 16.0) {
      newStart = audioEngine->Decks[0].LoopEndPos - 16.0;
    }
    audioEngine->Decks[0].LoopStartPos = newStart;
    DeckAudio_SetLoop(&audioEngine->Decks[0], true, audioEngine->Decks[0].LoopStartPos, audioEngine->Decks[0].LoopEndPos);
    app->deckA.JogDelta = 0;
  } else if (app->deckA.LoopAdjustOut && app->deckA.JogDelta != 0) {
    double trackSR = (double)audioEngine->Decks[0].SampleRate;
    if (trackSR < 100) trackSR = 44100.0;
    double deltaSamples = app->deckA.JogDelta * (trackSR / 400.0);
    double newEnd = audioEngine->Decks[0].LoopEndPos + deltaSamples;
    if (newEnd < audioEngine->Decks[0].LoopStartPos + 16.0) {
      newEnd = audioEngine->Decks[0].LoopStartPos + 16.0;
    }
    audioEngine->Decks[0].LoopEndPos = newEnd;
    DeckAudio_SetLoop(&audioEngine->Decks[0], true, audioEngine->Decks[0].LoopStartPos, audioEngine->Decks[0].LoopEndPos);
    app->deckA.JogDelta = 0;
  } else if (audioEngine->Decks[0].ReleaseFXType == 2) {
    // Active Backspin: Let engine physics decay JogRate smoothly
  } else if (effTouchA && app->deckA.VinylModeEnabled) {
    // Active vinyl scratch hold/move under hand
    double dt = GetFrameTime();
    if (dt < 0.001) dt = 0.016;

    if (app->deckA.JogDelta != 0) {
      double calibRPM = (app->deckA.Waveform.JogCalibRPM > 5.0f) ? (double)app->deckA.Waveform.JogCalibRPM : (double)g_JogConfig.DefaultRPM;
      double ticksPerSecAtNormalSpeed = (double)g_JogConfig.TicksPerRev * (calibRPM / 60.0);
      double rawRate = app->deckA.JogDelta / (ticksPerSecAtNormalSpeed * dt);
      app->deckA.JogDelta = 0;
      audioEngine->Decks[0].JogRate = audioEngine->Decks[0].JogRate * (double)g_JogConfig.EmaPrevWeight + rawRate * (double)g_JogConfig.EmaRawWeight;
    } else {
      audioEngine->Decks[0].JogRate = 0.0;
    }
  } else if (audioEngine->Decks[0].VinylReleaseActive) {
    // Vinyl touch release inertia in progress: consume leftover physical wheel ticks
    app->deckA.JogDelta = 0;
    double dt = GetFrameTime();
    if (dt < 0.001) dt = 0.016667;
    float dtFactor = (float)(dt / 0.016667);
    if (dtFactor < 0.1f) dtFactor = 0.1f;
    if (dtFactor > 5.0f) dtFactor = 5.0f;
    audioEngine->Decks[0].JogRate *= powf(g_JogConfig.VinylReleaseFriction, dtFactor);
    if (fabs(audioEngine->Decks[0].JogRate) < (double)g_JogConfig.VinylReleaseCutoff) {
      audioEngine->Decks[0].JogRate = 0.0;
      audioEngine->Decks[0].VinylReleaseActive = false;
    }
  } else if (app->deckA.JogDelta != 0) {
    // CDJ pitch bend nudge (outer wheel turn without touching top plate)
    double dt = GetFrameTime();
    if (dt < 0.001) dt = 0.016;
    double calibRPM = (app->deckA.Waveform.JogCalibRPM > 5.0f) ? (double)app->deckA.Waveform.JogCalibRPM : (double)g_JogConfig.DefaultRPM;
    double ticksPerSecAtNormalSpeed = (double)g_JogConfig.TicksPerRev * (calibRPM / 60.0);
    double rawRate = (app->deckA.JogDelta * (double)g_JogConfig.PitchBendScale) / (ticksPerSecAtNormalSpeed * dt);
    app->deckA.JogDelta = 0;
    audioEngine->Decks[0].JogRate = audioEngine->Decks[0].JogRate * (double)g_JogConfig.EmaPrevWeight + rawRate * (double)g_JogConfig.EmaRawWeight;
  } else {
    // Pitch bend release decay back to zero
    double dt = GetFrameTime();
    if (dt < 0.001) dt = 0.016667;
    float dtFactor = (float)(dt / 0.016667);
    if (dtFactor < 0.1f) dtFactor = 0.1f;
    if (dtFactor > 5.0f) dtFactor = 5.0f;
    audioEngine->Decks[0].JogRate *= powf(g_JogConfig.PitchBendFriction, dtFactor);
    if (fabs(audioEngine->Decks[0].JogRate) < (double)g_JogConfig.PitchBendCutoff) {
      audioEngine->Decks[0].JogRate = 0.0;
    }
  }

  // Deck B
  bool effTouchB = app->deckB.IsTouching && !app->deckB.LoopAdjustIn && !app->deckB.LoopAdjustOut;
  if (effTouchB != audioEngine->Decks[1].IsTouching) {
    bool released = !effTouchB && audioEngine->Decks[1].IsTouching;
    DeckAudio_SetJogTouch(&audioEngine->Decks[1], effTouchB);

    // Phase Snap on release if Beat Sync is ON
    if (released && app->deckB.SyncMode == 2 && !app->deckB.IsMaster) {
      Sync_RequestPhaseSnap(&app->deckB, &app->deckA, audioEngine);
    }
  }

  if (app->deckB.LoopAdjustIn && app->deckB.JogDelta != 0) {
    double trackSR = (double)audioEngine->Decks[1].SampleRate;
    if (trackSR < 100) trackSR = 44100.0;
    double deltaSamples = app->deckB.JogDelta * (trackSR / 400.0);
    double newStart = audioEngine->Decks[1].LoopStartPos + deltaSamples;
    if (newStart < 0) newStart = 0;
    if (newStart > audioEngine->Decks[1].LoopEndPos - 16.0) {
      newStart = audioEngine->Decks[1].LoopEndPos - 16.0;
    }
    audioEngine->Decks[1].LoopStartPos = newStart;
    DeckAudio_SetLoop(&audioEngine->Decks[1], true, audioEngine->Decks[1].LoopStartPos, audioEngine->Decks[1].LoopEndPos);
    app->deckB.JogDelta = 0;
  } else if (app->deckB.LoopAdjustOut && app->deckB.JogDelta != 0) {
    double trackSR = (double)audioEngine->Decks[1].SampleRate;
    if (trackSR < 100) trackSR = 44100.0;
    double deltaSamples = app->deckB.JogDelta * (trackSR / 400.0);
    double newEnd = audioEngine->Decks[1].LoopEndPos + deltaSamples;
    if (newEnd < audioEngine->Decks[1].LoopStartPos + 16.0) {
      newEnd = audioEngine->Decks[1].LoopStartPos + 16.0;
    }
    audioEngine->Decks[1].LoopEndPos = newEnd;
    DeckAudio_SetLoop(&audioEngine->Decks[1], true, audioEngine->Decks[1].LoopStartPos, audioEngine->Decks[1].LoopEndPos);
    app->deckB.JogDelta = 0;
  } else if (audioEngine->Decks[1].ReleaseFXType == 2) {
    // Active Backspin: Let engine physics decay JogRate smoothly
  } else if (effTouchB && app->deckB.VinylModeEnabled) {
    // Active vinyl scratch hold/move under hand
    double dt = GetFrameTime();
    if (dt < 0.001) dt = 0.016;

    if (app->deckB.JogDelta != 0) {
      double calibRPM = (app->deckB.Waveform.JogCalibRPM > 5.0f) ? (double)app->deckB.Waveform.JogCalibRPM : (double)g_JogConfig.DefaultRPM;
      double ticksPerSecAtNormalSpeed = (double)g_JogConfig.TicksPerRev * (calibRPM / 60.0);
      double rawRate = app->deckB.JogDelta / (ticksPerSecAtNormalSpeed * dt);
      app->deckB.JogDelta = 0;
      audioEngine->Decks[1].JogRate = audioEngine->Decks[1].JogRate * (double)g_JogConfig.EmaPrevWeight + rawRate * (double)g_JogConfig.EmaRawWeight;
    } else {
      audioEngine->Decks[1].JogRate = 0.0;
    }
  } else if (audioEngine->Decks[1].VinylReleaseActive) {
    // Vinyl touch release inertia in progress: consume leftover physical wheel ticks
    app->deckB.JogDelta = 0;
    double dt = GetFrameTime();
    if (dt < 0.001) dt = 0.016667;
    float dtFactor = (float)(dt / 0.016667);
    if (dtFactor < 0.1f) dtFactor = 0.1f;
    if (dtFactor > 5.0f) dtFactor = 5.0f;
    audioEngine->Decks[1].JogRate *= powf(g_JogConfig.VinylReleaseFriction, dtFactor);
    if (fabs(audioEngine->Decks[1].JogRate) < (double)g_JogConfig.VinylReleaseCutoff) {
      audioEngine->Decks[1].JogRate = 0.0;
      audioEngine->Decks[1].VinylReleaseActive = false;
    }
  } else if (app->deckB.JogDelta != 0) {
    // CDJ pitch bend nudge (outer wheel turn without touching top plate)
    double dt = GetFrameTime();
    if (dt < 0.001) dt = 0.016;
    double calibRPM = (app->deckB.Waveform.JogCalibRPM > 5.0f) ? (double)app->deckB.Waveform.JogCalibRPM : (double)g_JogConfig.DefaultRPM;
    double ticksPerSecAtNormalSpeed = (double)g_JogConfig.TicksPerRev * (calibRPM / 60.0);
    double rawRate = (app->deckB.JogDelta * (double)g_JogConfig.PitchBendScale) / (ticksPerSecAtNormalSpeed * dt);
    app->deckB.JogDelta = 0;
    audioEngine->Decks[1].JogRate = audioEngine->Decks[1].JogRate * (double)g_JogConfig.EmaPrevWeight + rawRate * (double)g_JogConfig.EmaRawWeight;
  } else {
    // Pitch bend release decay back to zero
    double dt = GetFrameTime();
    if (dt < 0.001) dt = 0.016667;
    float dtFactor = (float)(dt / 0.016667);
    if (dtFactor < 0.1f) dtFactor = 0.1f;
    if (dtFactor > 5.0f) dtFactor = 5.0f;
    audioEngine->Decks[1].JogRate *= powf(g_JogConfig.PitchBendFriction, dtFactor);
    if (fabs(audioEngine->Decks[1].JogRate) < (double)g_JogConfig.PitchBendCutoff) {
      audioEngine->Decks[1].JogRate = 0.0;
    }
  }

  // Update Jog Pointer Angle & Load Track Animation Timers
  float frameDt = GetFrameTime();
  if (frameDt < 0.0001f) frameDt = 0.016667f;

  // Deck A Animation Update
  if (app->deckA.LoadAnimTimer > 0.0f) {
      app->deckA.LoadAnimTimer -= frameDt;
      if (app->deckA.LoadAnimTimer < 0.0f) app->deckA.LoadAnimTimer = 0.0f;
      app->deckA.JogPointerAngle += frameDt * 1440.0f; // Fast spin load animation
  } else if (app->deckA.LoadedTrack && audioEngine->Decks[0].SampleRate > 0) {
      double currentSec = audioEngine->Decks[0].Position / (double)audioEngine->Decks[0].SampleRate;
      float rpmA = (app->deckA.Waveform.JogCalibRPM > 5.0f) ? app->deckA.Waveform.JogCalibRPM : 33.333333f;
      double revPeriod = 60.0 / (double)rpmA; // 1.8 seconds per 360 deg rotation at 33.33 RPM
      app->deckA.JogPointerAngle = (float)fmod((currentSec / revPeriod) * 360.0, 360.0);
  } else if (app->deckA.IsPlaying || fabs(audioEngine->Decks[0].JogRate) > 0.01) {
      float rpmA = (app->deckA.Waveform.JogCalibRPM > 5.0f) ? app->deckA.Waveform.JogCalibRPM : 33.333333f;
      float speedA = (app->deckA.IsPlaying ? (1.0f + app->deckA.TempoPercent / 100.0f) : 0.0f) + (float)audioEngine->Decks[0].JogRate;
      app->deckA.JogPointerAngle += frameDt * (rpmA / 60.0f) * 360.0f * speedA;
  }
  app->deckA.JogPointerAngle = fmodf(app->deckA.JogPointerAngle, 360.0f);
  if (app->deckA.JogPointerAngle < 0.0f) app->deckA.JogPointerAngle += 360.0f;

  // Deck B Animation Update
  if (app->deckB.LoadAnimTimer > 0.0f) {
      app->deckB.LoadAnimTimer -= frameDt;
      if (app->deckB.LoadAnimTimer < 0.0f) app->deckB.LoadAnimTimer = 0.0f;
      app->deckB.JogPointerAngle += frameDt * 1440.0f; // Fast spin load animation
  } else if (app->deckB.LoadedTrack && audioEngine->Decks[1].SampleRate > 0) {
      double currentSec = audioEngine->Decks[1].Position / (double)audioEngine->Decks[1].SampleRate;
      float rpmB = (app->deckB.Waveform.JogCalibRPM > 5.0f) ? app->deckB.Waveform.JogCalibRPM : 33.333333f;
      double revPeriod = 60.0 / (double)rpmB; // 1.8 seconds per 360 deg rotation at 33.33 RPM
      app->deckB.JogPointerAngle = (float)fmod((currentSec / revPeriod) * 360.0, 360.0);
  } else if (app->deckB.IsPlaying || fabs(audioEngine->Decks[1].JogRate) > 0.01) {
      float rpmB = (app->deckB.Waveform.JogCalibRPM > 5.0f) ? app->deckB.Waveform.JogCalibRPM : 33.333333f;
      float speedB = (app->deckB.IsPlaying ? (1.0f + app->deckB.TempoPercent / 100.0f) : 0.0f) + (float)audioEngine->Decks[1].JogRate;
      app->deckB.JogPointerAngle += frameDt * (rpmB / 60.0f) * 360.0f * speedB;
  }
  app->deckB.JogPointerAngle = fmodf(app->deckB.JogPointerAngle, 360.0f);
  if (app->deckB.JogPointerAngle < 0.0f) app->deckB.JogPointerAngle += 360.0f;

  // Apply Vinyl Start/Stop Physics
  for (int i = 0; i < 2; i++) {
    DeckState *ds = (i == 0) ? &app->deckA : &app->deckB;
    double sr = (double)audioEngine->Decks[i].SampleRate;
    if (sr < 8000)
      sr = 44100.0;

    // Assuming 1024 frames per block as set in InitAudio
    float blockSize = 1024.0f;
    float blocksPerSec = (float)sr / blockSize;

    // Convert Bar duration settings to seconds using deck BPM (default 120.0 BPM if unanalyzed)
    float bpm = (audioEngine->Decks[i].BPM > 10.0) ? (float)audioEngine->Decks[i].BPM : 120.0f;
    float barSec = (4.0f * 60.0f) / bpm;
    float startSec = ds->Waveform.VinylStartMs * barSec;
    float stopSec = ds->Waveform.VinylStopMs * barSec;

    audioEngine->Decks[i].VinylStartAccel =
        (startSec > 0.001f) ? (1.0f / (startSec * blocksPerSec + 1.0f)) : 1.0f;
    audioEngine->Decks[i].VinylStopAccel =
        (stopSec > 0.001f) ? (1.0f / (stopSec * blocksPerSec + 1.0f)) : 1.0f;
  }

  bool browserSearchFocused = app->browserState.IsActive && app->browserState.IsSearching;

  if (!browserSearchFocused && IsKeyPressed(app->keyMap.toggleInfo)) {
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

  // Block view-switch and back hotkeys while the browser search bar is focused

  if (!browserSearchFocused && IsKeyPressed(app->keyMap.toggleSettings)) {
    if (app->screen == ScreenSettings) {
      app->screen = ScreenPlayer;
      app->settingsState.IsActive = false;
    } else {
      app->screen = ScreenSettings;
      app->settingsState.IsActive = true;
    }
  }

  if (!browserSearchFocused && IsKeyPressed(app->keyMap.toggleMixer)) {
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
  if (!browserSearchFocused && IsKeyPressed(app->keyMap.back)) {
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

    // --- Update System Stats (CPU/RAM) & Storage check every 1s ---
    static float statsTimer = 0;
    statsTimer += GetFrameTime();
    if (statsTimer >= 1.0f) {
      SystemStats stats = GetSystemStats();
      app->topbar.CPUUsage = stats.cpuUsage;
      app->topbar.RAMUsage = stats.ramUsageMB;
      app->topbar.BatteryLevel = stats.batteryLevel;
      app->topbar.IsCharging = stats.isCharging;
      statsTimer = 0;

      // Periodically monitor USB storage device & MIDI Controller connections
      Browser_CheckStorageConnection(&app->browserState);
      if (app->browserState.BrowseLevel == 3) {
        Browser_RefreshStorages(&app->browserState);
      }
      MIDI_CheckHotplug(&app->midiCtx);
    }

    // --- Handle Seek Requests from UI (Hot Cues / Scrubbing) ---
    if (app->deckA.HasSeekRequest) {
      DeckAudio_JumpToMs(&audioEngine->Decks[0], app->deckA.SeekMs);
      if (app->deckA.IsPlaying) DeckAudio_InstantPlay(&audioEngine->Decks[0]);
      else DeckAudio_InstantStop(&audioEngine->Decks[0]);
      app->deckA.HasSeekRequest = false;
      app->padState.ActiveLoopIdx[0] = -1;
    }
    if (app->deckB.HasSeekRequest) {
      DeckAudio_JumpToMs(&audioEngine->Decks[1], app->deckB.SeekMs);
      if (app->deckB.IsPlaying) DeckAudio_InstantPlay(&audioEngine->Decks[1]);
      else DeckAudio_InstantStop(&audioEngine->Decks[1]);
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

  // Update UI touch state & memory state once per frame
#if defined(PLATFORM_DRM) || (defined(__linux__) && !defined(__ANDROID__))
  EvdevTouch_Update();
#endif
  UI_UpdateTouchState();
  MemoryGuard_Update();

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
    Toast_UpdateAndDraw(GetFrameTime());
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

    if (UI_IsReleased()) {
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

  // GLOBAL MEMORY WARNING OVERLAY
  if (MemoryGuard_GetLevel() >= MEM_MODE_LITE) {
      float warnW = S(100);
      float warnH = S(16);
      float warnX = SCREEN_WIDTH - warnW - S(10);
      float warnY = TOP_BAR_H + S(5);
      
      DrawRectangle(warnX, warnY, warnW, warnH, Fade(ColorRed, 0.7f));
      DrawRectangleLinesEx((Rectangle){warnX, warnY, warnW, warnH}, 1.0f, ColorWhite);
      
      Font fSm = UIFonts_GetFace(S(9));
      DrawCentredText(MemoryGuard_GetStatusString(), fSm, warnX, warnW, warnY + S(3), S(9), ColorWhite);
  }

  rlPopMatrix();

  EndDrawing();
}
