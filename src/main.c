#include "raylib.h"
#include "rlgl.h"
#include <string.h>
#include <stdio.h>
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/views/topbar.h"
#include "ui/views/splash.h"
#include "ui/views/info.h"
#include "ui/views/settings.h"
#include "ui/views/about.h"
#include "ui/views/mixer.h"
#include "ui/player/player.h"
#include "ui/browser/browser.h"
#include "input/keyboard.h"
#include "logic/sync.h"
#include "logic/settings_io.h"
#include "ui/components/helpers.h"
#include "midi/midi_handler.h"
#include "logic/control_object.h"

typedef enum {
    ScreenPlayer,
    ScreenBrowser,
    ScreenInfo,
    ScreenSettings,
    ScreenAbout,
    ScreenMixer,
    ScreenSplash
} CurrentScreen;

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
} App;

AudioEngine *globalAudioEngine = NULL;

void AudioProcessCallback(void *buffer, unsigned int frames) {
    if (globalAudioEngine) {
#if defined(PLATFORM_DRM)
        // On DRM/ALSA target, we assume 4 channel output is ready
        AudioEngine_Process(globalAudioEngine, (float*)buffer, frames);
#else
        // On Windows/Others (Stereo), we process 4 channels internally and downmix
        static float temp4Ch[4096 * 4];
        AudioEngine_Process(globalAudioEngine, temp4Ch, frames);
        
        float *out = (float*)buffer;
        for (unsigned int s = 0; s < frames; s++) {
            // Mix Master (1-2) and Cue (3-4) for stereo monitoring
            out[s*2] = temp4Ch[s*4] + temp4Ch[s*4 + 2] * 0.5f;
            out[s*2+1] = temp4Ch[s*4 + 1] + temp4Ch[s*4 + 3] * 0.5f;
            
            // Hard clip for safety
            if (out[s*2] > 1.0f) out[s*2] = 1.0f;
            if (out[s*2] < -1.0f) out[s*2] = -1.0f;
            if (out[s*2+1] > 1.0f) out[s*2+1] = 1.0f;
            if (out[s*2+1] < -1.0f) out[s*2+1] = -1.0f;
        }
#endif
    }
}

void OnSettingsApply(void *ctx) {
    App *a = (App*)ctx;
    
    // Sync UI items back to deck states
    int styleIdx = a->settingsState.Items[2].Current;
    a->deckA.Waveform.Style = (WaveformStyle)styleIdx;
    a->deckB.Waveform.Style = (WaveformStyle)styleIdx;
    
    float gainMap[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    a->deckA.Waveform.GainLow = gainMap[a->settingsState.Items[3].Current];
    a->deckA.Waveform.GainMid = gainMap[a->settingsState.Items[4].Current];
    a->deckA.Waveform.GainHigh = gainMap[a->settingsState.Items[5].Current];
    
    a->deckB.Waveform = a->deckA.Waveform;
    
    a->deckA.Waveform.LoadLock = (a->settingsState.Items[1].Current == 1);
    a->deckA.Waveform.VinylStartMs = a->settingsState.Items[6].Value;
    a->deckA.Waveform.VinylStopMs = a->settingsState.Items[7].Value;
    a->deckB.Waveform = a->deckA.Waveform;

    printf("[SETTINGS] Applied Style: %d, Gains: L%.2f M%.2f H%.2f, Start: %.0f, Stop: %.0f, Lock: %d\n", 
           a->deckA.Waveform.Style, a->deckA.Waveform.GainLow, 
           a->deckA.Waveform.GainMid, a->deckA.Waveform.GainHigh,
           a->deckA.Waveform.VinylStartMs, a->deckA.Waveform.VinylStopMs,
           a->deckA.Waveform.LoadLock);

    Settings_Save(a->deckA.Waveform, a->deckB.Waveform);
    a->screen = ScreenPlayer;
    a->settingsState.IsActive = false;
}

void OnSettingsAction(void *ctx, int idx) {
    App *a = (App*)ctx;
    if (idx == 8) { // ABOUT
        a->screen = ScreenAbout;
        a->aboutState.IsActive = true;
    } else if (idx == 9) { // EXIT APPLICATION
        a->showExitConfirm = true;
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
    for (int i = 0; i < 3; i++) a->browserState.PlaylistBankIdx[i] = -1;
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
    Settings_Load(&a->deckA.Waveform, &a->deckB.Waveform);

    strcpy(a->settingsState.Items[2].Label, "WFM STYLE");
    strcpy(a->settingsState.Items[2].Options[0], "BLUE");
    strcpy(a->settingsState.Items[2].Options[1], "RGB");
    strcpy(a->settingsState.Items[2].Options[2], "3-BAND");
    a->settingsState.Items[2].OptionsCount = 3;
    a->settingsState.Items[2].Current = a->deckA.Waveform.Style;

    const char* gLabels[] = { "0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x" };
    float gValues[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    
    strcpy(a->settingsState.Items[3].Label, "WFM LOW GAIN");
    for (int i=0; i<6; i++) {
        strcpy(a->settingsState.Items[3].Options[i], gLabels[i]);
        if (a->deckA.Waveform.GainLow == gValues[i]) a->settingsState.Items[3].Current = i;
    }
    a->settingsState.Items[3].OptionsCount = 6;

    strcpy(a->settingsState.Items[4].Label, "WFM MID GAIN");
    for (int i=0; i<6; i++) {
        strcpy(a->settingsState.Items[4].Options[i], gLabels[i]);
        if (a->deckA.Waveform.GainMid == gValues[i]) a->settingsState.Items[4].Current = i;
    }
    a->settingsState.Items[4].OptionsCount = 6;

    strcpy(a->settingsState.Items[5].Label, "WFM HIGH GAIN");
    for (int i=0; i<6; i++) {
        strcpy(a->settingsState.Items[5].Options[i], gLabels[i]);
        if (a->deckA.Waveform.GainHigh == gValues[i]) a->settingsState.Items[5].Current = i;
    }
    a->settingsState.Items[5].OptionsCount = 6;
    
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
    
    strcpy(a->settingsState.Items[8].Label, "ABOUT");
    a->settingsState.Items[8].Type = SETTING_TYPE_ACTION;

    strcpy(a->settingsState.Items[9].Label, "EXIT APPLICATION");
    a->settingsState.Items[9].Type = SETTING_TYPE_ACTION;
    a->settingsState.ItemsCount = 10;

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
    strcpy(a->aboutState.Version, "v1.2.0-Alpha");
    strcpy(a->aboutState.Developer, "Hanif Bagus Saputra Hadi");
    strcpy(a->aboutState.Instagram, "@unxchr");

    // Init Components
    TopBar_Init(&a->topbar);
    DeckStrip_Init(&a->stripA, 0, &a->deckA);
    DeckStrip_Init(&a->stripB, 1, &a->deckB);
    PlayerRenderer_Init(&a->player, &a->deckA, &a->deckB, &a->fxState, NULL);
    BrowserRenderer_Init(&a->browser, &a->browserState);
    InfoRenderer_Init(&a->info, &a->infoState);
    SettingsRenderer_Init(&a->settings, &a->settingsState);
    a->settings.OnApply = OnSettingsApply;
    a->settings.OnClose = OnSettingsApply;
    a->settings.OnAction = OnSettingsAction;
    a->settings.callbackCtx = a;
    AboutRenderer_Init(&a->about, &a->aboutState);
    MixerRenderer_Init(&a->mixer, &a->mixerState);
    SplashRenderer_Init(&a->splash, &a->splashCounter);
    a->keyMap = GetDefaultMapping();
    memset(&a->midiCtx, 0, sizeof(MidiContext));
}

int main(void) {
    // We start with the reference size or common 16:10 resolution
    // Standard XDJ-XZ screen is actually 7.0-inch 60Hz 1280x800 or similar
    // The user wants max width 1080, which implies 1080x675 (16:10)
    int startWidth = 1080;
    int startHeight = 675;

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(startWidth, startHeight, "XDJ UNX - C Port Test");
    SetWindowMinSize(REF_WIDTH, REF_HEIGHT);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // ESC is for 'back'

    UIFonts_Init();

    App app;
    App_Init(&app);

    MIDI_Init(&app.midiCtx);

    // Initialize Audio
    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(1024);
    
    AudioEngine audioEngine;
    AudioEngine_Init(&audioEngine);
    app.browserState.AudioPlugin = (struct AudioEngine*)&audioEngine;
    app.browserState.DeckA = (struct DeckState*)&app.deckA;
    app.browserState.DeckB = (struct DeckState*)&app.deckB;
    app.player.AudioPlugin = &audioEngine;

    // Register Controls after Init
    CO_Init();
    CO_Register("[Channel1]", "play", CO_TYPE_BOOL, &audioEngine.Decks[0].IsMotorOn, 0, 1);
    CO_Register("[Channel1]", "volume", CO_TYPE_FLOAT, &audioEngine.Decks[0].Trim, 0, 2.0f);
    CO_Register("[Channel1]", "filterHigh", CO_TYPE_FLOAT, &audioEngine.Decks[0].EqHigh, 0, 1.0f);
    CO_Register("[Channel1]", "filterMid", CO_TYPE_FLOAT, &audioEngine.Decks[0].EqMid, 0, 1.0f);
    CO_Register("[Channel1]", "filterLow", CO_TYPE_FLOAT, &audioEngine.Decks[0].EqLow, 0, 1.0f);
    CO_Register("[Channel1]", "cue_default", CO_TYPE_BOOL, &audioEngine.Decks[0].IsCueActive, 0, 1);

    CO_Register("[Channel2]", "play", CO_TYPE_BOOL, &audioEngine.Decks[1].IsMotorOn, 0, 1);
    CO_Register("[Channel2]", "volume", CO_TYPE_FLOAT, &audioEngine.Decks[1].Trim, 0, 2.0f);
    CO_Register("[Channel2]", "filterHigh", CO_TYPE_FLOAT, &audioEngine.Decks[1].EqHigh, 0, 1.0f);
    CO_Register("[Channel2]", "filterMid", CO_TYPE_FLOAT, &audioEngine.Decks[1].EqMid, 0, 1.0f);
    CO_Register("[Channel2]", "filterLow", CO_TYPE_FLOAT, &audioEngine.Decks[1].EqLow, 0, 1.0f);
    CO_Register("[Channel2]", "cue_default", CO_TYPE_BOOL, &audioEngine.Decks[1].IsCueActive, 0, 1);

    CO_Register("[Master]", "crossfader", CO_TYPE_FLOAT, &audioEngine.Crossfader, -1.0f, 1.0f);

    // Attach custom audio processor via a global wrapper pointer since raylib callback takes no context
    globalAudioEngine = &audioEngine;
    SetAudioStreamBufferSizeDefault(4096);
    AttachAudioMixedProcessor(AudioProcessCallback);

    while (!WindowShouldClose()) {
        // Cache scale for this frame based on current window size
        UI_UpdateScale();

        // Navigation Logic (Mock)
        if (app.screen == ScreenSplash) {
            app.splashCounter--;
            if (app.splashCounter <= 0) app.screen = ScreenPlayer;
        }

        // Exclusive Master Logic & Auto Takeover
        static bool lastMasterA = false;
        static bool lastMasterB = false;
        static int lastSyncA = 0;
        static int lastSyncB = 0;

        // Auto Assign Master: If Sync is turned ON and No deck is master
        bool noMaster = !app.deckA.IsMaster && !app.deckB.IsMaster;
        if (noMaster) {
            if (app.deckA.SyncMode > 0 && lastSyncA == 0) app.deckB.IsMaster = true;
            else if (app.deckB.SyncMode > 0 && lastSyncB == 0) app.deckA.IsMaster = true;
        }
        lastSyncA = app.deckA.SyncMode;
        lastSyncB = app.deckB.SyncMode;

        // Auto Takeover: If Master stops, other deck becomes Master if playing
        if (app.deckA.IsMaster && !app.deckA.IsPlaying && app.deckB.IsPlaying) {
            app.deckA.IsMaster = false;
            app.deckB.IsMaster = true;
        } else if (app.deckB.IsMaster && !app.deckB.IsPlaying && app.deckA.IsPlaying) {
            app.deckB.IsMaster = false;
            app.deckA.IsMaster = true;
        }

        if (app.deckA.IsMaster && !lastMasterA) app.deckB.IsMaster = false;
        if (app.deckB.IsMaster && !lastMasterB) app.deckA.IsMaster = false;
        lastMasterA = app.deckA.IsMaster;
        lastMasterB = app.deckB.IsMaster;

        HandleKeyboardInputs(&app.keyMap, &app.deckA, &app.deckB, &audioEngine);
        MIDI_Update(&app.midiCtx, &app.deckA, &app.deckB, &audioEngine);

        // Global UI navigation using keyMap
        if (IsKeyPressed(app.keyMap.toggleBrowser)) {
            if (app.screen == ScreenPlayer) {
                app.screen = ScreenBrowser;
                app.browserState.IsActive = true;
            } else if (app.screen == ScreenBrowser) {
                app.screen = ScreenPlayer;
                app.browserState.IsActive = !app.browserState.IsActive;
            }
        }

        // Tempo Calculation (10000 = 100%)
        float realPitchA = 1.0f + (app.deckA.TempoPercent / 100.0f);
        audioEngine.Decks[0].Pitch = (uint16_t)(realPitchA * 10000.0f);
        app.deckA.CurrentBPM = app.deckA.OriginalBPM * realPitchA;

        float realPitchB = 1.0f + (app.deckB.TempoPercent / 100.0f);
        audioEngine.Decks[1].Pitch = (uint16_t)(realPitchB * 10000.0f);
        app.deckB.CurrentBPM = app.deckB.OriginalBPM * realPitchB;
        
        // --- Sync Control Logic ---
        Sync_Update(&app.deckA, &app.deckB, &audioEngine);
        
        
        // --- Sync UI Jog/Modes back to Audio Engine ---
        // Deck A
        if (app.deckA.IsTouching != audioEngine.Decks[0].IsTouching) {
            bool released = !app.deckA.IsTouching && audioEngine.Decks[0].IsTouching;
            DeckAudio_SetJogTouch(&audioEngine.Decks[0], app.deckA.IsTouching);
            
            // Phase Snap on release if Beat Sync is ON
            if (released && app.deckA.SyncMode == 2 && !app.deckA.IsMaster) {
                Sync_RequestPhaseSnap(&app.deckA, &app.deckB, &audioEngine);
            }

            // Immediately switch to BPM sync on touch if was in BEAT sync
            if (app.deckA.IsTouching && app.deckA.SyncMode == 2) {
                app.deckA.SyncMode = 1;
            }
        }
        if (app.deckA.IsTouching) {
            double dt = GetFrameTime();
            if (dt < 0.001) dt = 0.016; 
            double srA = (double)audioEngine.Decks[0].SampleRate;
            if (srA < 8000) srA = 44100.0;

            double framesInFrame = srA * dt;
            // JogDelta is in 150Hz frames. Convert to PCM frames for comparison.
            double instantaneousRate = (app.deckA.JogDelta * (srA / 150.0)) / (framesInFrame);
            
            audioEngine.Decks[0].JogRate = instantaneousRate;
            app.deckA.JogDelta = 0;
        }
        audioEngine.Decks[0].VinylModeEnabled = app.deckA.VinylModeEnabled;
        audioEngine.Decks[0].MasterTempoActive = app.deckA.MasterTempo;

        // Deck B
        if (app.deckB.IsTouching != audioEngine.Decks[1].IsTouching) {
            bool released = !app.deckB.IsTouching && audioEngine.Decks[1].IsTouching;
            DeckAudio_SetJogTouch(&audioEngine.Decks[1], app.deckB.IsTouching);
            
            // Phase Snap on release if Beat Sync is ON
            if (released && app.deckB.SyncMode == 2 && !app.deckB.IsMaster) {
                Sync_RequestPhaseSnap(&app.deckB, &app.deckA, &audioEngine);
            }

            // Immediately switch to BPM sync on touch if was in BEAT sync
            if (app.deckB.IsTouching && app.deckB.SyncMode == 2) {
                app.deckB.SyncMode = 1;
            }
        }
        if (app.deckB.IsTouching) {
            double dt = GetFrameTime();
            if (dt < 0.001) dt = 0.016; 
            double srB = (double)audioEngine.Decks[1].SampleRate;
            if (srB < 8000) srB = 44100.0;

            double framesInFrame = srB * dt;
            double instantaneousRate = (app.deckB.JogDelta * (srB / 150.0)) / framesInFrame;

            audioEngine.Decks[1].JogRate = instantaneousRate;
            app.deckB.JogDelta = 0;
        }
        audioEngine.Decks[1].VinylModeEnabled = app.deckB.VinylModeEnabled;
        audioEngine.Decks[1].MasterTempoActive = app.deckB.MasterTempo;

        // Apply Vinyl Start/Stop Physics
        for (int i=0; i<2; i++) {
            DeckState *ds = (i == 0) ? &app.deckA : &app.deckB;
            double sr = (double)audioEngine.Decks[i].SampleRate;
            if (sr < 8000) sr = 44100.0;
            
            // Assuming 1024 frames per block as set in InitAudio
            float blockSize = 1024.0f; 
            float blocksPerSec = (float)sr / blockSize;
            
            // Accel is the increment per block to reach pitch 1.0 in N seconds
            // Since we want linear ramp: delta = (TargetRate - 0) / TotalBlocks
            // TotalBlocks = DurationSeconds * blocksPerSec
            audioEngine.Decks[i].VinylStartAccel = 1.0f / ((ds->Waveform.VinylStartMs / 1000.0f) * blocksPerSec + 1.0f);
            audioEngine.Decks[i].VinylStopAccel = 1.0f / ((ds->Waveform.VinylStopMs / 1000.0f) * blocksPerSec + 1.0f);
        }

        // --- Sync Audio Engine State to UI State ---
        if (audioEngine.Decks[0].PCMBuffer) {
            // Position is already frame-based (L+R pair = 1 frame)
            double playheadFrames = audioEngine.Decks[0].Position;
            double srA = (double)audioEngine.Decks[0].SampleRate;
            if (srA < 8000) srA = 44100.0;

            app.deckA.Position = (playheadFrames * 105.0) / srA;
            app.deckA.IsTouching = audioEngine.Decks[0].IsTouching;
            app.deckA.IsPlaying = audioEngine.Decks[0].IsPlaying;
            
            double posSec = playheadFrames / srA;
            app.deckA.PositionMs = (uint32_t)(posSec * 1000.0);
            
            double lenSec = ((double)audioEngine.Decks[0].TotalSamples / (double)CHANNELS) / srA;
            app.deckA.TrackLengthMs = (uint32_t)(lenSec * 1000.0);
        }
        if (audioEngine.Decks[1].PCMBuffer) {
            double playheadFrames = audioEngine.Decks[1].Position;
            double srB = (double)audioEngine.Decks[1].SampleRate;
            if (srB < 8000) srB = 44100.0;

            app.deckB.Position = (playheadFrames * 105.0) / srB;
            app.deckB.IsTouching = audioEngine.Decks[1].IsTouching;
            app.deckB.IsPlaying = audioEngine.Decks[1].IsPlaying;
            
            double posSec = playheadFrames / srB;
            app.deckB.PositionMs = (uint32_t)(posSec * 1000.0);
            
            double lenSec = ((double)audioEngine.Decks[1].TotalSamples / (double)CHANNELS) / srB;
            app.deckB.TrackLengthMs = (uint32_t)(lenSec * 1000.0);
        }
        
        if (IsKeyPressed(app.keyMap.toggleInfo)) {
            if (app.screen == ScreenInfo) {
                app.screen = ScreenPlayer;
                app.infoState.IsActive = false;
            } else {
                app.screen = ScreenInfo;
                app.infoState.IsActive = true;
                
                // Sync Info State from Decks
                for (int i=0; i<2; i++) {
                    DeckState *ds = (i == 0) ? &app.deckA : &app.deckB;
                    InfoTrack *it = &app.infoState.Tracks[i];
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

        if (IsKeyPressed(app.keyMap.toggleSettings)) {
            if (app.screen == ScreenSettings) {
                app.screen = ScreenPlayer;
                app.settingsState.IsActive = false;
            } else {
                app.screen = ScreenSettings;
                app.settingsState.IsActive = true;
            }
        }

        if (IsKeyPressed(app.keyMap.toggleMixer)) {
            if (app.screen == ScreenMixer) {
                app.screen = ScreenPlayer;
                app.mixerState.IsActive = false;
            } else {
                app.screen = ScreenMixer;
                app.mixerState.IsActive = true;
                // Hook audio engine up right before drawing if not earlier
                app.mixerState.AudioPlugin = &audioEngine;
            }
        }

        // ESC / Back logic
        if (IsKeyPressed(app.keyMap.back)) {
            if (app.screen == ScreenBrowser) {
                if (app.browserState.BrowseLevel == 3 && !app.browserState.IsTagList) {
                    app.screen = ScreenPlayer;
                    app.browserState.IsActive = false;
                } else {
                    Browser_Back(&app.browserState);
                }
            } else if (app.screen != ScreenPlayer && app.screen != ScreenSplash) {
                app.screen = ScreenPlayer;
                app.browserState.IsActive = false;
                app.infoState.IsActive = false;
                app.settingsState.IsActive = false;
                app.aboutState.IsActive = false;
                app.mixerState.IsActive = false;
            }
        }

        // Update active components
        if (app.screen == ScreenSplash) app.splash.base.Update((Component*)&app.splash);
        if (app.screen == ScreenPlayer) app.player.base.Update((Component*)&app.player);
        if (app.screen == ScreenBrowser) app.browser.base.Update((Component*)&app.browser);
        if (app.screen == ScreenInfo) app.info.base.Update((Component*)&app.info);
        if (app.screen == ScreenSettings) app.settings.base.Update((Component*)&app.settings);
        if (app.screen == ScreenAbout) app.about.base.Update((Component*)&app.about);
        if (app.screen == ScreenMixer) app.mixer.base.Update((Component*)&app.mixer);
        
        if (app.screen != ScreenSplash) {
            app.stripA.base.Update((Component*)&app.stripA);
            app.stripB.base.Update((Component*)&app.stripB);
            app.topbar.base.Update((Component*)&app.topbar);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        // Apply global offset for all UI drawing
        rlPushMatrix();
        rlTranslatef(UI_OffsetX, UI_OffsetY, 0);

        switch (app.screen) {
            case ScreenSplash: app.splash.base.Draw((Component*)&app.splash); break;
            case ScreenPlayer: app.player.base.Draw((Component*)&app.player); break;
            case ScreenBrowser: app.browser.base.Draw((Component*)&app.browser); break;
            case ScreenInfo: app.info.base.Draw((Component*)&app.info); break;
            case ScreenSettings: app.settings.base.Draw((Component*)&app.settings); break;
            case ScreenAbout: app.about.base.Draw((Component*)&app.about); break;
            case ScreenMixer: app.mixer.base.Draw((Component*)&app.mixer); break;
            default: break;
        }

        if (app.screen != ScreenSplash) {
            app.stripA.base.Draw((Component*)&app.stripA);
            app.stripB.base.Draw((Component*)&app.stripB);
            app.topbar.base.Draw((Component*)&app.topbar);
            DrawText("SPACE: Browser | I: Info | TAB: Settings | M: Mixer", 10, Si(REF_HEIGHT) - 20, 10, GRAY);
        }

        // Exit Confirmation Popup
        if (app.showExitConfirm) {
            float pw = S(200);
            float ph = S(100);
            float px = (SCREEN_WIDTH - pw) / 2.0f;
            float py = (SCREEN_HEIGHT - ph) / 2.0f;

            // Overlay
            DrawRectangle(-UI_OffsetX, -UI_OffsetY, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.8f));
            
            // Box
            DrawRectangle(px, py, pw, ph, ColorBGUtil);
            DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 1.0f, ColorGray);
            
            Font fMd = UIFonts_GetFace(S(12));
            DrawCentredText("EXIT APPLICATION?", fMd, px, pw, py + S(20), S(12), ColorWhite);
            DrawCentredText("Are you sure?", UIFonts_GetFace(S(9)), px, pw, py + S(40), S(9), ColorShadow);

            // Options
            float btnW = S(60);
            float btnH = S(20);
            
            // NO Button
            bool noHover = CheckCollisionPointRec(UIGetMousePosition(), (Rectangle){px + S(30), py + S(65), btnW, btnH});
            DrawRectangle(px + S(30), py + S(65), btnW, btnH, noHover ? ColorGray : ColorDark1);
            DrawRectangleLines(px + S(30), py + S(65), btnW, btnH, ColorShadow);
            DrawCentredText("NO", fMd, px + S(30), btnW, py + S(68), S(11), ColorWhite);
            
            // YES Button
            bool yesHover = CheckCollisionPointRec(UIGetMousePosition(), (Rectangle){px + pw - S(90), py + S(65), btnW, btnH});
            DrawRectangle(px + pw - S(90), py + S(65), btnW, btnH, yesHover ? ColorRed : ColorDark1);
            DrawRectangleLines(px + pw - S(90), py + S(65), btnW, btnH, ColorRed);
            DrawCentredText("YES", fMd, px + pw - S(90), btnW, py + S(68), S(11), ColorWhite);

            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                if (noHover) app.showExitConfirm = false;
                if (yesHover) break; // Exit loop
            }
            if (IsKeyPressed(KEY_ENTER)) break;
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) app.showExitConfirm = false;
        }

        rlPopMatrix();

        EndDrawing();
    }

    UIFonts_Unload();
    MIDI_Close(&app.midiCtx);
    CloseWindow();

    return 0;
}
