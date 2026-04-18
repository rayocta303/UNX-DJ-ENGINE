#include "core/audio_backend.h"
#include "core/logger.h"
#include "core/logic/control_object.h"
#include "core/logic/settings_io.h"
#include "core/logic/sync.h"
#include "core/midi/midi_handler.h"
#include "input/keyboard.h"
#include "raylib.h"
#include "rlgl.h"
#include "ui/browser/browser.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/player.h"
#include "ui/views/about.h"
#include "ui/views/info.h"
#include "ui/views/mixer.h"
#include "ui/views/settings.h"
#include "ui/views/splash.h"
#include "ui/views/topbar.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
  MixerState mixerState;

  TopBar topbar;
  DeckStrip stripA;
  DeckStrip stripB;
  PlayerRenderer player;
  BrowserRenderer browser;
  InfoRenderer info;
  SettingsRenderer settings;
  AboutRenderer about;
  MixerRenderer mixer;
  SplashRenderer splash;
  KeyboardMapping keyMap;
  MidiContext midiCtx;
  bool showExitConfirm;
  AudioBackendConfig activeAudioConfig;
} App;

AudioEngine *globalAudioEngine = NULL;
App *globalApp = NULL; // Needed for iOS callbacks

void AudioProcessCallback(float *buffer, unsigned int frames);
void UpdateDrawFrame(App *app);

#if defined(PLATFORM_IOS)
// ghera/raylib-iOS callbacks
void ios_ready(void) {
    UNX_LOG_INFO("[IOS] ios_ready: Initializing Raylib and Audio...");
    
    // 1. Initialize Window (0,0 gets native screen size)
    InitWindow(0, 0, APP_NAME);
    SetTargetFPS(60);
    
    // 2. Initialize Fonts
    UIFonts_Init();
    
    // 3. Start Audio Backend
    if (globalApp) {
        AudioBackend_Start(globalApp->activeAudioConfig, AudioProcessCallback);
    }
    
    UNX_LOG_INFO("[IOS] ios_ready: Setup complete. Window size: %dx%d", GetScreenWidth(), GetScreenHeight());
}

void ios_update(void) {
    if (globalApp) UpdateDrawFrame(globalApp);
}

void ios_destroy(void) {
    UNX_LOG_INFO("[IOS] ios_destroy called.");
}
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

  a->deckA.Waveform.GainLow = 0.5f + (a->settingsState.Items[3].Current * 0.1f);
  a->deckA.Waveform.GainMid = 0.5f + (a->settingsState.Items[4].Current * 0.1f);
  a->deckA.Waveform.GainHigh = 0.5f + (a->settingsState.Items[5].Current * 0.1f);

  a->deckB.Waveform = a->deckA.Waveform;

  a->deckA.Waveform.LoadLock = (a->settingsState.Items[1].Current == 1);
  a->deckA.Waveform.VinylStartMs = a->settingsState.Items[6].Value;
  a->deckA.Waveform.VinylStopMs = a->settingsState.Items[7].Value;
  a->deckB.Waveform = a->deckA.Waveform;

  printf("[SETTINGS] Applied Style: %d, Gains: L%.2f M%.2f H%.2f, Start: %.0f, "
         "Stop: %.0f, Lock: %d\n",
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
    printf("[SETTINGS] Audio Hardware config changed, restarting backend...\n");
    AudioBackend_Start(aconf, AudioProcessCallback);
    a->activeAudioConfig = aconf;
    
    // Update active driver info in About screen
    AudioBackend_GetActiveInfo(NULL, NULL, a->aboutState.AudioDriver, a->aboutState.AudioDevice);
  }

  Settings_Save(a->deckA.Waveform, a->deckB.Waveform, a->activeAudioConfig);
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
  // Auto-Select defaults: Start with 2-channel mirrored setup (CH 1/2 for Master and Cue)
  // to prevent errors on fresh settings, even if more channels are available.
  a->settingsState.Items[9].Current = 0;  // Master L -> CH1
  a->settingsState.Items[10].Current = (channels > 1) ? 1 : 0; // Master R -> CH2
  
  a->settingsState.Items[11].Current = 1; // Cue L -> CH 1 (Index 0 is Blank)
  a->settingsState.Items[12].Current = (channels > 1) ? 2 : 1; // Cue R -> CH 2
}

void OnSettingsValueChanged(void *ctx, int idx) {
  App *a = (App *)ctx;
  if (idx == 8) { // AUDIO DEVICE changed
    UpdateChannelOptions(a, a->settingsState.Items[8].Current - 1);
  }
}

void OnSettingsAction(void *ctx, int idx) {
  App *a = (App *)ctx;
  if (idx == 15) { // ABOUT
    a->screen = ScreenAbout;
    a->aboutState.IsActive = true;
  } else if (idx == 16) { // EXIT APPLICATION
    a->showExitConfirm = true;
  }
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
  a->screen = ScreenSplash;
  a->splashCounter = 120; // 2 seconds at 60 FPS

  // Init Deck States
  memset(&a->deckA, 0, sizeof(DeckState));
  a->deckA.ID = 0;
  strcpy(a->deckA.SourceName, "USB1");
  a->deckA.PositionMs = 0;
  a->deckA.TrackLengthMs = 0;
  a->deckA.IsMaster = true;
  a->deckA.VinylModeEnabled = true;
  a->deckA.MasterTempo = false;
  a->deckA.ZoomScale = 16;

  memset(&a->deckB, 0, sizeof(DeckState));
  a->deckB.ID = 1;
  strcpy(a->deckB.SourceName, "USB1");
  a->deckB.PositionMs = 0;
  a->deckB.TrackLengthMs = 0;
  a->deckB.VinylModeEnabled = true;
  a->deckB.MasterTempo = false;
  a->deckB.ZoomScale = 16;

  // Init Browser State
  memset(&a->browserState, 0, sizeof(BrowserState));
  a->browserState.IsActive = false;
  a->browserState.BrowseLevel = 3; // Source level
  for (int i = 0; i < 3; i++)
    a->browserState.PlaylistBankIdx[i] = -1;
  Browser_RefreshStorages(&a->browserState);

  // Init Settings State
  memset(&a->settingsState, 0, sizeof(SettingsState));
  a->settingsState.IsActive = false;
  a->settingsState.ItemsCount = 8;

  strcpy(a->settingsState.Items[0].Label, "PLAY MODE");
  strcpy(a->settingsState.Items[0].Options[0], "CONTINUE");
  strcpy(a->settingsState.Items[0].Options[1], "SINGLE");
  a->settingsState.Items[0].OptionsCount = 2;

  strcpy(a->settingsState.Items[1].Label, "LOAD LOCK");
  strcpy(a->settingsState.Items[1].Options[0], "OFF");
  strcpy(a->settingsState.Items[1].Options[1], "ON");
  a->settingsState.Items[1].OptionsCount = 2;

  // Load persisted settings
  Settings_Load(&a->deckA.Waveform, &a->deckB.Waveform, &a->activeAudioConfig);

  strcpy(a->settingsState.Items[2].Label, "WFM STYLE");
  strcpy(a->settingsState.Items[2].Options[0], "BLUE");
  strcpy(a->settingsState.Items[2].Options[1], "RGB");
  strcpy(a->settingsState.Items[2].Options[2], "3-BAND");
  a->settingsState.Items[2].OptionsCount = 3;
  a->settingsState.Items[2].Current = a->deckA.Waveform.Style;

  strcpy(a->settingsState.Items[3].Label, "WFM LOW GAIN");
  a->settingsState.Items[3].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[3].OptionsCount = 25; // 0.1 to 2.5
  for(int i=0; i<25; i++) {
     float v = 0.1f + (i * 0.1f);
     sprintf(a->settingsState.Items[3].Options[i], "%.1fx", v);
     if (fabs(a->deckA.Waveform.GainLow - v) < 0.05f) a->settingsState.Items[3].Current = i;
  }

  strcpy(a->settingsState.Items[4].Label, "WFM MID GAIN");
  a->settingsState.Items[4].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[4].OptionsCount = 25;
  for(int i=0; i<25; i++) {
     float v = 0.1f + (i * 0.1f);
     sprintf(a->settingsState.Items[4].Options[i], "%.1fx", v);
     if (fabs(a->deckA.Waveform.GainMid - v) < 0.05f) a->settingsState.Items[4].Current = i;
  }

  strcpy(a->settingsState.Items[5].Label, "WFM HIGH GAIN");
  a->settingsState.Items[5].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[5].OptionsCount = 25;
  for(int i=0; i<25; i++) {
     float v = 0.1f + (i * 0.1f);
     sprintf(a->settingsState.Items[5].Options[i], "%.1fx", v);
     if (fabs(a->deckA.Waveform.GainHigh - v) < 0.05f) a->settingsState.Items[5].Current = i;
  }

  strcpy(a->settingsState.Items[6].Label, "JOG START TIME");
  a->settingsState.Items[6].Type = SETTING_TYPE_KNOB;
  a->settingsState.Items[6].Min = 0;
  a->settingsState.Items[6].Max = 2000;
  a->settingsState.Items[6].Value = a->deckA.Waveform.VinylStartMs;
  strcpy(a->settingsState.Items[6].Unit, "ms");

  strcpy(a->settingsState.Items[7].Label, "JOG RELEASE TIME");
  a->settingsState.Items[7].Type = SETTING_TYPE_KNOB;
  a->settingsState.Items[7].Min = 0;
  a->settingsState.Items[7].Max = 2000;
  a->settingsState.Items[7].Value = a->deckA.Waveform.VinylStopMs;
  strcpy(a->settingsState.Items[7].Unit, "ms");

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
  a->settingsState.Items[14].Current = (a->activeAudioConfig.SampleRate == 44100) ? 0 : 1;

  strcpy(a->settingsState.Items[13].Label, "BUFFER SIZE");
  a->settingsState.Items[13].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[13].OptionsCount = 4;
  strcpy(a->settingsState.Items[13].Options[0], "128");
  strcpy(a->settingsState.Items[13].Options[1], "256");
  strcpy(a->settingsState.Items[13].Options[2], "512");
  strcpy(a->settingsState.Items[13].Options[3], "1024");
  a->settingsState.Items[13].Current = 1;

  strcpy(a->settingsState.Items[14].Label, "SAMPLE RATE");
  a->settingsState.Items[14].Type = SETTING_TYPE_LIST;
  a->settingsState.Items[14].OptionsCount = 2;
  strcpy(a->settingsState.Items[14].Options[0], "44.1 kHz");
  strcpy(a->settingsState.Items[14].Options[1], "48.0 kHz");
  a->settingsState.Items[14].Current = 1;

  strcpy(a->settingsState.Items[15].Label, "ABOUT");
  a->settingsState.Items[15].Type = SETTING_TYPE_ACTION;

  strcpy(a->settingsState.Items[16].Label, "EXIT APPLICATION");
  a->settingsState.Items[16].Type = SETTING_TYPE_ACTION;
  a->settingsState.ItemsCount = 17;

  // Set Load Lock current opt
  a->settingsState.Items[1].Current = a->deckA.Waveform.LoadLock ? 1 : 0;

  // Init Info State
  memset(&a->infoState, 0, sizeof(InfoState));
  a->infoState.IsActive = false;

  // Init FX State
  memset(&a->fxState, 0, sizeof(BeatFXState));

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

  // Default active audio config (matches main's initialAudioCfg)
  a->activeAudioConfig = (AudioBackendConfig){.DeviceIndex = -1,
                                              .MasterOutL = 0,
                                              .MasterOutR = 1,
                                              .CueOutL = 0,
                                              .CueOutR = 1,
                                              .SampleRate = 48000,
                                              .BufferSizeFrames = 256};
  AboutRenderer_Init(&a->about, &a->aboutState);
  MixerRenderer_Init(&a->mixer, &a->mixerState);
  SplashRenderer_Init(&a->splash, &a->splashCounter);
  a->keyMap = GetDefaultMapping();
  memset(&a->midiCtx, 0, sizeof(MidiContext));
}

#if defined(PLATFORM_IOS)
int raylib_main(int argc, char *argv[]) {
#else
int main(void) {
#endif
  // We start with the reference size or common 16:10 resolution
  // Standard XDJ-XZ screen is actually 7.0-inch 60Hz 1280x800 or similar
  // The user wants max width 1080, which implies 1080x675 (16:10)
  int startWidth = 1080;
  int startHeight = 675;

#if !defined(PLATFORM_IOS)
  #if defined(__ANDROID__)
    InitWindow(GetScreenWidth(), GetScreenHeight(), APP_NAME);
    SetTargetFPS(60); 
  #else
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(startWidth, startHeight, APP_NAME " - C Port Test");
    SetWindowMinSize(REF_WIDTH, REF_HEIGHT);
    SetTargetFPS(60);
  #endif
#endif
  SetExitKey(KEY_NULL); // ESC is for 'back'

  Log_Init();
  UNX_LOG_INFO("Application starting...");
  
#if !defined(PLATFORM_IOS)
  UIFonts_Init();
  UNX_LOG_INFO("Fonts initialized.");
#endif

#if defined(PLATFORM_IOS)
  void ios_init_audio_session();
  ios_init_audio_session();
#endif

  // Initialize Audio Backend FIRST so App_Init can enumerate real devices
  AudioBackend_Init();

  UNX_LOG_INFO("[MAIN] Initializing App (Heap)...");
  UNX_LOG_INFO("[MAIN] Structure Sizes - App: %.2f MB, AudioEngine: %.2f MB", 
               (float)sizeof(App)/(1024.0f*1024.0f), 
               (float)sizeof(AudioEngine)/(1024.0f*1024.0f));

  App *app = (App *)malloc(sizeof(App));
  if (!app) {
      UNX_LOG_ERR("[CRITICAL] Failed to allocate App on heap!");
      return -1;
  }
  memset(app, 0, sizeof(App));
  globalApp = app; // Store globally for iOS callbacks
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
  UNX_LOG_INFO("[MAIN] Audio config prepared. SR: %d, Buf: %d", initialAudioCfg.SampleRate, initialAudioCfg.BufferSizeFrames);
  
  UNX_LOG_INFO("[MAIN] Retrieving active audio info...");
  // Set initial Audio Driver name for the UI
  AudioBackend_GetActiveInfo(NULL, NULL, app->aboutState.AudioDriver, app->aboutState.AudioDevice);

  UNX_LOG_INFO("[MAIN] Initializing Audio Engine (Heap)...");
  AudioEngine *audioEngine = (AudioEngine *)malloc(sizeof(AudioEngine));
  if (!audioEngine) {
      UNX_LOG_ERR("[CRITICAL] Failed to allocate AudioEngine on heap!");
      return -1;
  }
  AudioEngine_Init(audioEngine);
  app->browserState.AudioPlugin = (struct AudioEngine *)audioEngine;
  app->browserState.DeckA = (struct DeckState *)&app->deckA;
  app->browserState.DeckB = (struct DeckState *)&app->deckB;
  app->player.AudioPlugin = audioEngine;
  
  UNX_LOG_INFO("[MAIN] Audio Engine initialized. Total RAM: %.2f MB", Log_GetRAMUsage());

  // Register Controls after Init
  CO_Init();
  CO_Register("[Channel1]", "play", CO_TYPE_BOOL,
              &audioEngine->Decks[0].IsMotorOn, 0, 1);
  CO_Register("[Channel1]", "volume", CO_TYPE_FLOAT, &audioEngine->Decks[0].Trim,
              0, 2.0f);
  CO_Register("[Channel1]", "filterHigh", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqHigh, 0, 1.0f);
  CO_Register("[Channel1]", "filterMid", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqMid, 0, 1.0f);
  CO_Register("[Channel1]", "filterLow", CO_TYPE_FLOAT,
              &audioEngine->Decks[0].EqLow, 0, 1.0f);
  CO_Register("[Channel1]", "cue_default", CO_TYPE_BOOL,
              &audioEngine->Decks[0].IsCueActive, 0, 1);
 
  CO_Register("[Channel2]", "play", CO_TYPE_BOOL,
              &audioEngine->Decks[1].IsMotorOn, 0, 1);
  CO_Register("[Channel2]", "volume", CO_TYPE_FLOAT, &audioEngine->Decks[1].Trim,
              0, 2.0f);
  CO_Register("[Channel2]", "filterHigh", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqHigh, 0, 1.0f);
  CO_Register("[Channel2]", "filterMid", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqMid, 0, 1.0f);
  CO_Register("[Channel2]", "filterLow", CO_TYPE_FLOAT,
              &audioEngine->Decks[1].EqLow, 0, 1.0f);
  CO_Register("[Channel2]", "cue_default", CO_TYPE_BOOL,
              &audioEngine->Decks[1].IsCueActive, 0, 1);
 
  CO_Register("[Master]", "crossfader", CO_TYPE_FLOAT, &audioEngine->Crossfader,
              -1.0f, 1.0f);

  globalAudioEngine = audioEngine;

#if !defined(PLATFORM_IOS)
  UIFonts_Init();
  UNX_LOG_INFO("[MAIN] Fonts initialized.");

  UNX_LOG_INFO("[MAIN] Starting audio backend...");
  AudioBackend_Start(initialAudioCfg, AudioProcessCallback);
  UNX_LOG_INFO("[MAIN] Audio backend started.");
  
  UNX_LOG_INFO("[MAIN] Setting main loop (FPS: 60)...");
#endif

#if defined(PLATFORM_WEB)
  #include <emscripten/emscripten.h>
  emscripten_set_main_loop_arg((void (*)(void*))UpdateDrawFrame, app, 0, 1);
#elif defined(PLATFORM_IOS)
  // On iOS, we return here and let the platform layer call ios_ready/ios_update.
  return 0;
#endif
#else
  while (!WindowShouldClose()) {
    UpdateDrawFrame(app);
  }
  
  UIFonts_Unload();
  MIDI_Close(&app->midiCtx);
  CloseWindow();
  
  if (audioEngine) free(audioEngine);
  if (app) free(app);
#endif

  return 0;
}

void UpdateDrawFrame(App *app) {
  static bool firstCall = true;
  if (firstCall) {
      UNX_LOG_INFO("[MAIN] UpdateDrawFrame: First call (Window: %dx%d, Ready: %d, Hidden: %d, Min: %d)", 
                   GetScreenWidth(), GetScreenHeight(), IsWindowReady(), IsWindowHidden(), IsWindowMinimized());
      firstCall = false;
  }

#if defined(PLATFORM_IOS)
  // Safety: If window dimensions are not yet available, skip this frame.
  if (GetScreenWidth() <= 0 || GetScreenHeight() <= 0) {
      static double lastWarn = 0;
      if (GetTime() - lastWarn > 5.0) {
          UNX_LOG_WARN("[MAIN] UpdateDrawFrame: Waiting for valid screen dimensions (%dx%d)...", GetScreenWidth(), GetScreenHeight());
          lastWarn = GetTime();
      }
      return;
  }

  // Grace period: ignore Hidden/Minimized for the first 2 seconds to allow transition
  if (GetTime() > 2.0) {
      if (IsWindowHidden() || IsWindowMinimized()) return;
  }
#endif

  AudioEngine *audioEngine = globalAudioEngine;
  if (!audioEngine) return;

  static int lastScreen = -1;
  
  if (lastScreen != app->screen) {
      UNX_LOG_INFO("[MAIN] Screen changed to: %d", app->screen);
      lastScreen = app->screen;
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

    // Cache scale for this frame based on current window size
    UI_UpdateScale();

    app->topbar.ActiveScreen = app->screen;

    // Navigation Logic (Time-based Splash)
    if (app->screen == ScreenSplash) {
      static float splashTime = 0;
      splashTime += GetFrameTime();
      if (splashTime >= 2.0f) { // 2 Seconds splash
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

    HandleKeyboardInputs(&app->keyMap, &app->deckA, &app->deckB, audioEngine);
    MIDI_Update(&app->midiCtx, &app->deckA, &app->deckB, audioEngine);

    // Global UI navigation using keyMap
    if (IsKeyPressed(app->keyMap.toggleBrowser)) {
      if (app->screen == ScreenPlayer) {
        app->screen = ScreenBrowser;
        app->browserState.IsActive = true;
      } else if (app->screen == ScreenBrowser) {
        app->screen = ScreenPlayer;
        app->browserState.IsActive = !app->browserState.IsActive;
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
          it->BPM = ds->OriginalBPM;
          strcpy(it->Key, ds->TrackKey);
          it->Duration = ds->TrackLengthMs / 1000;
          strcpy(it->Source, ds->SourceName);
          // Add more if available...
        }
      }
    }

    if (IsKeyPressed(app->keyMap.toggleSettings)) {
      if (app->screen == ScreenSettings) {
        app->screen = ScreenPlayer;
        app->infoState.IsActive = false; // Typo fix
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

    if (app->screen != ScreenSplash) {
      app->stripA.base.Update((Component *)&app->stripA);
      app->stripB.base.Update((Component *)&app->stripB);
      app->topbar.base.Update((Component *)&app->topbar);

      // --- Handle Seek Requests from UI ---
      if (app->deckA.HasSeekRequest) {
        DeckAudio_JumpToMs(&audioEngine->Decks[0], app->deckA.SeekMs);
        app->deckA.HasSeekRequest = false;
      }
      if (app->deckB.HasSeekRequest) {
        DeckAudio_JumpToMs(&audioEngine->Decks[1], app->deckB.SeekMs);
        app->deckB.HasSeekRequest = false;
      }
    }

    if (!IsWindowReady() || !IsWindowFocused()) return;

    BeginDrawing();
    ClearBackground(BLACK);

    // Apply global offset for all UI drawing
    rlPushMatrix();
    rlTranslatef(UI_OffsetX, UI_OffsetY, 0);

    switch (app->screen) {
    case ScreenSplash:
      app->splash.base.Draw((Component *)&app->splash);
      break;
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
    default:
      break;
    }

    if (app->screen != ScreenSplash) {
      app->stripA.base.Draw((Component *)&app->stripA);
      app->stripB.base.Draw((Component *)&app->stripB);
      app->topbar.base.Draw((Component *)&app->topbar);
    }

    // Exit Confirmation Popup
    if (app->showExitConfirm) {
      float pw = S(200);
      float ph = S(100);
      float px = (SCREEN_WIDTH - pw) / 2.0f;
      float py = (SCREEN_HEIGHT - ph) / 2.0f;

      // Overlay
      DrawRectangle(-UI_OffsetX, -UI_OffsetY, GetScreenWidth(),
                    GetScreenHeight(), Fade(BLACK, 0.8f));

      // Box
      DrawRectangle(px, py, pw, ph, ColorBGUtil);
      DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 1.0f, ColorGray);

      Font fMd = UIFonts_GetFace(S(12));
      DrawCentredText("EXIT APPLICATION?", fMd, px, pw, py + S(20), S(12),
                      ColorWhite);
      DrawCentredText("Are you sure?", UIFonts_GetFace(S(9)), px, pw,
                      py + S(40), S(9), ColorShadow);

      // Options
      float btnW = S(60);
      float btnH = S(20);

      // NO Button
      bool noHover = CheckCollisionPointRec(
          UIGetMousePosition(),
          (Rectangle){px + S(30), py + S(65), btnW, btnH});
      DrawRectangle(px + S(30), py + S(65), btnW, btnH,
                    noHover ? ColorGray : ColorDark1);
      DrawRectangleLines(px + S(30), py + S(65), btnW, btnH, ColorShadow);
      DrawCentredText("NO", fMd, px + S(30), btnW, py + S(68), S(11),
                      ColorWhite);

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
      if (IsKeyPressed(KEY_ENTER)) { /* handle */ }
      if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))
        app->showExitConfirm = false;
    }

    rlPopMatrix();

    EndDrawing();
}
