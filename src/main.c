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
#include "ui/player/player.h"
#include "ui/browser/browser.h"
#include "input/keyboard.h"

typedef enum {
    ScreenPlayer,
    ScreenBrowser,
    ScreenInfo,
    ScreenSettings,
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

    TopBar topbar;
    PlayerRenderer player;
    BrowserRenderer browser;
    InfoRenderer info;
    SettingsRenderer settings;
    SplashRenderer splash;
    KeyboardMapping keyMap;
} App;

AudioEngine *globalAudioEngine = NULL;

void AudioProcessCallback(void *buffer, unsigned int frames) {
    if (globalAudioEngine) {
        AudioEngine_Process(globalAudioEngine, (float*)buffer, frames);
    }
}

void App_Init(App *a) {
    a->screen = ScreenSplash;
    a->splashCounter = 120; // 2 seconds at 60 FPS

    // Init Deck States
    memset(&a->deckA, 0, sizeof(DeckState));
    strcpy(a->deckA.SourceName, "USB1");
    a->deckA.PositionMs = 0;
    a->deckA.TrackLengthMs = 0;
    a->deckA.IsMaster = true;
    a->deckA.VinylModeEnabled = true;
    a->deckA.MasterTempo = false;
    a->deckA.ZoomScale = 16;

    memset(&a->deckB, 0, sizeof(DeckState));
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
    Browser_RefreshStorages(&a->browserState);

    // Init Settings State
    memset(&a->settingsState, 0, sizeof(SettingsState));
    a->settingsState.IsActive = false;
    a->settingsState.ItemsCount = 4;
    strcpy(a->settingsState.Items[0].Label, "PLAY MODE");
    strcpy(a->settingsState.Items[0].Options[0], "CONTINUE");
    strcpy(a->settingsState.Items[0].Options[1], "SINGLE");
    a->settingsState.Items[0].OptionsCount = 2;
    
    strcpy(a->settingsState.Items[1].Label, "LOAD LOCK");
    strcpy(a->settingsState.Items[1].Options[0], "OFF");
    strcpy(a->settingsState.Items[1].Options[1], "ON");
    a->settingsState.Items[1].OptionsCount = 2;

    // Init Components
    TopBar_Init(&a->topbar);
    PlayerRenderer_Init(&a->player, &a->deckA, &a->deckB, &a->fxState, NULL);
    BrowserRenderer_Init(&a->browser, &a->browserState);
    InfoRenderer_Init(&a->info, &a->infoState);
    SettingsRenderer_Init(&a->settings, &a->settingsState);
    SplashRenderer_Init(&a->splash);
    a->keyMap = GetDefaultMapping();
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

    // Initialize Audio
    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(1024);
    
    AudioEngine audioEngine;
    AudioEngine_Init(&audioEngine);
    app.browserState.AudioPlugin = (struct AudioEngine*)&audioEngine;
    app.browserState.DeckA = (struct DeckState*)&app.deckA;
    app.browserState.DeckB = (struct DeckState*)&app.deckB;
    app.player.AudioPlugin = &audioEngine;

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

        // Exclusive Master Logic
        static bool lastMasterA = false;
        static bool lastMasterB = false;
        if (app.deckA.IsMaster && !lastMasterA) app.deckB.IsMaster = false;
        if (app.deckB.IsMaster && !lastMasterB) app.deckA.IsMaster = false;
        lastMasterA = app.deckA.IsMaster;
        lastMasterB = app.deckB.IsMaster;

        HandleKeyboardInputs(&app.keyMap, &app.deckA, &app.deckB, &audioEngine);

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
        
        // --- Sync UI Scratching/Modes back to Audio Engine ---
        // Deck A
        if (app.deckA.IsTouching != audioEngine.Decks[0].IsTouching) {
            DeckAudio_SetScratch(&audioEngine.Decks[0], app.deckA.IsTouching);
        }
        if (app.deckA.IsTouching) {
            double dt = GetFrameTime();
            if (dt < 0.001) dt = 0.016; 
            double srA = (double)audioEngine.Decks[0].SampleRate;
            if (srA < 8000) srA = 44100.0;

            double framesInFrame = srA * dt;
            // ScratchDelta is in 150Hz frames. Convert to PCM frames for comparison.
            double instantaneousRate = (app.deckA.ScratchDelta * (srA / 150.0)) / (framesInFrame);
            
            audioEngine.Decks[0].ScratchSpeed = instantaneousRate;
            app.deckA.ScratchDelta = 0;
        }
        audioEngine.Decks[0].VinylModeEnabled = app.deckA.VinylModeEnabled;
        audioEngine.Decks[0].MasterTempoActive = app.deckA.MasterTempo;

        // Deck B
        if (app.deckB.IsTouching != audioEngine.Decks[1].IsTouching) {
            DeckAudio_SetScratch(&audioEngine.Decks[1], app.deckB.IsTouching);
        }
        if (app.deckB.IsTouching) {
            double dt = GetFrameTime();
            if (dt < 0.001) dt = 0.016; 
            double srB = (double)audioEngine.Decks[1].SampleRate;
            if (srB < 8000) srB = 44100.0;

            double framesInFrame = srB * dt;
            double instantaneousRate = (app.deckB.ScratchDelta * (srB / 150.0)) / framesInFrame;

            audioEngine.Decks[1].ScratchSpeed = instantaneousRate;
            app.deckB.ScratchDelta = 0;
        }
        audioEngine.Decks[1].VinylModeEnabled = app.deckB.VinylModeEnabled;
        audioEngine.Decks[1].MasterTempoActive = app.deckB.MasterTempo;

        // --- Sync Audio Engine State to UI State ---
        if (audioEngine.Decks[0].PCMBuffer) {
            // Position is already frame-based (L+R pair = 1 frame)
            double playheadFrames = audioEngine.Decks[0].Position;
            double srA = (double)audioEngine.Decks[0].SampleRate;
            if (srA < 8000) srA = 44100.0;

            app.deckA.Position = (playheadFrames * 150.0) / srA;
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

            app.deckB.Position = (playheadFrames * 150.0) / srB;
            app.deckB.IsTouching = audioEngine.Decks[1].IsTouching;
            app.deckB.IsPlaying = audioEngine.Decks[1].IsPlaying;
            
            double posSec = playheadFrames / srB;
            app.deckB.PositionMs = (uint32_t)(posSec * 1000.0);
            
            double lenSec = ((double)audioEngine.Decks[1].TotalSamples / (double)CHANNELS) / srB;
            app.deckB.TrackLengthMs = (uint32_t)(lenSec * 1000.0);
        }
        
        if (IsKeyPressed(app.keyMap.toggleInfo)) {
            if (app.screen == ScreenInfo) app.screen = ScreenPlayer;
            else app.screen = ScreenInfo;
        }

        if (IsKeyPressed(app.keyMap.toggleSettings)) {
            if (app.screen == ScreenSettings) app.screen = ScreenPlayer;
            else app.screen = ScreenSettings;
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
            }
        }

        // Update active components
        if (app.screen == ScreenSplash) app.splash.base.Update((Component*)&app.splash);
        if (app.screen == ScreenPlayer) app.player.base.Update((Component*)&app.player);
        if (app.screen == ScreenBrowser) app.browser.base.Update((Component*)&app.browser);
        if (app.screen == ScreenInfo) app.info.base.Update((Component*)&app.info);
        if (app.screen == ScreenSettings) app.settings.base.Update((Component*)&app.settings);
        
        app.topbar.base.Update((Component*)&app.topbar);

        BeginDrawing();
        ClearBackground(BLACK);

        // Calculate centering offset
        float drawW = REF_WIDTH * UI_CurrScale;
        float drawH = REF_HEIGHT * UI_CurrScale;
        float offsetX = (GetScreenWidth() - drawW) / 2.0f;
        float offsetY = (GetScreenHeight() - drawH) / 2.0f;

        // Apply global offset for all UI drawing
        rlPushMatrix();
        rlTranslatef(offsetX, offsetY, 0);

        switch (app.screen) {
            case ScreenSplash: app.splash.base.Draw((Component*)&app.splash); break;
            case ScreenPlayer: app.player.base.Draw((Component*)&app.player); break;
            case ScreenBrowser: app.browser.base.Draw((Component*)&app.browser); break;
            case ScreenInfo: app.info.base.Draw((Component*)&app.info); break;
            case ScreenSettings: app.settings.base.Draw((Component*)&app.settings); break;
            default: break;
        }

        if (app.screen != ScreenSplash) {
            app.topbar.base.Draw((Component*)&app.topbar);
            DrawText("SPACE: Browser | I: Info | TAB: Settings", 10, REF_HEIGHT * UI_CurrScale - 20, 10, GRAY);
        }

        rlPopMatrix();

        EndDrawing();
    }

    UIFonts_Unload();
    CloseWindow();

    return 0;
}
