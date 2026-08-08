#include "settings_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "defaults.h"

#if defined(_WIN32)
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0777)
#endif

static void EnsureControllersExist(const char* baseDir) {
    if (!baseDir || baseDir[0] == '\0') return;
    
    char controllersDir[512];
    snprintf(controllersDir, sizeof(controllersDir), "%s/controllers", baseDir);
    MKDIR(controllersDir);

    char path[512];
    snprintf(path, sizeof(path), "%s/LoopMIDI.midi.xml", controllersDir);
    if (!FileExists(path)) {
        SaveFileText(path, (char*)DEFAULT_MIDI_LOOPMIDI);
    }
    snprintf(path, sizeof(path), "%s/Template.midi.xml", controllersDir);
    if (!FileExists(path)) {
        SaveFileText(path, (char*)DEFAULT_MIDI_TEMPLATE);
    }
}

static void LoadFromJSON(const char* json, WaveformSettings *wfmA, WaveformSettings *wfmB, AudioBackendConfig *audio, BeatFXState *fx, ColorFXManager *cfxA, ColorFXManager *cfxB, bool *quantizeA, bool *quantizeB, float *masterVolume) {
    if (!json) return;
    
    const char* p = json;
    float val;
    int ival;

    // Deck A
    if ((p = strstr(json, "\"wfmA\""))) {
        if (sscanf(strstr(p, "\"style\""), "\"style\": %d", &ival) == 1) wfmA->Style = (WaveformStyle)ival;
        if (sscanf(strstr(p, "\"low\""), "\"low\": %f", &val) == 1) wfmA->GainLow = val;
        if (sscanf(strstr(p, "\"mid\""), "\"mid\": %f", &val) == 1) wfmA->GainMid = val;
        if (sscanf(strstr(p, "\"high\""), "\"high\": %f", &val) == 1) wfmA->GainHigh = val;
        if (sscanf(strstr(p, "\"start\""), "\"start\": %f", &val) == 1) {
            if (val > 16.0f) val = 0.5f; // Convert legacy millisecond settings to Bar
            wfmA->VinylStartMs = val;
        }
        if (sscanf(strstr(p, "\"stop\""), "\"stop\": %f", &val) == 1) {
            if (val > 16.0f) val = 1.0f; // Convert legacy millisecond settings to Bar
            wfmA->VinylStopMs = val;
        }
        if (sscanf(strstr(p, "\"lock\""), "\"lock\": %d", &ival) == 1) wfmA->LoadLock = (bool)ival;
        if (sscanf(strstr(p, "\"rpm\""), "\"rpm\": %f", &val) == 1) wfmA->JogCalibRPM = val;
    }

    // Deck B
    if ((p = strstr(json, "\"wfmB\""))) {
        if (sscanf(strstr(p, "\"style\""), "\"style\": %d", &ival) == 1) wfmB->Style = (WaveformStyle)ival;
        if (sscanf(strstr(p, "\"low\""), "\"low\": %f", &val) == 1) wfmB->GainLow = val;
        if (sscanf(strstr(p, "\"mid\""), "\"mid\": %f", &val) == 1) wfmB->GainMid = val;
        if (sscanf(strstr(p, "\"high\""), "\"high\": %f", &val) == 1) wfmB->GainHigh = val;
        if (sscanf(strstr(p, "\"start\""), "\"start\": %f", &val) == 1) {
            if (val > 16.0f) val = 0.5f;
            wfmB->VinylStartMs = val;
        }
        if (sscanf(strstr(p, "\"stop\""), "\"stop\": %f", &val) == 1) {
            if (val > 16.0f) val = 1.0f;
            wfmB->VinylStopMs = val;
        }
        if (sscanf(strstr(p, "\"lock\""), "\"lock\": %d", &ival) == 1) wfmB->LoadLock = (bool)ival;
        if (sscanf(strstr(p, "\"rpm\""), "\"rpm\": %f", &val) == 1) wfmB->JogCalibRPM = val;
    }

    // Audio
    if ((p = strstr(json, "\"audio\""))) {
        if (sscanf(strstr(p, "\"devIdx\""), "\"devIdx\": %d", &ival) == 1) audio->DeviceIndex = ival;
        if (sscanf(strstr(p, "\"mastL\""), "\"mastL\": %d", &ival) == 1) audio->MasterOutL = ival;
        if (sscanf(strstr(p, "\"mastR\""), "\"mastR\": %d", &ival) == 1) audio->MasterOutR = ival;
        if (sscanf(strstr(p, "\"cueL\""), "\"cueL\": %d", &ival) == 1) audio->CueOutL = ival;
        if (sscanf(strstr(p, "\"cueR\""), "\"cueR\": %d", &ival) == 1) audio->CueOutR = ival;
        if (sscanf(strstr(p, "\"sr\""), "\"sr\": %d", &ival) == 1) audio->SampleRate = ival;
        if (sscanf(strstr(p, "\"buf\""), "\"buf\": %d", &ival) == 1) audio->BufferSizeFrames = ival;
        if (sscanf(strstr(p, "\"bitdepth\""), "\"bitdepth\": %d", &ival) == 1) audio->PCMBitDepth = ival;
        if (sscanf(strstr(p, "\"xfader\""), "\"xfader\": %d", &ival) == 1) audio->CrossfaderCurve = ival;
    }
    // Beat FX
    if ((p = strstr(json, "\"beatfx\""))) {
        if (sscanf(strstr(p, "\"fx\""), "\"fx\": %d", &ival) == 1) fx->SelectedFX = ival;
        if (sscanf(strstr(p, "\"pad\""), "\"pad\": %d", &ival) == 1) fx->SelectedPad = ival;
        if (sscanf(strstr(p, "\"ch\""), "\"ch\": %d", &ival) == 1) fx->SelectedChannel = ival;
        if (sscanf(strstr(p, "\"depth\""), "\"depth\": %f", &val) == 1) fx->LevelDepth = val;
        if (sscanf(strstr(p, "\"q\""), "\"q\": %d", &ival) == 1) fx->Quantize = (bool)ival;
        if (sscanf(strstr(p, "\"tab\""), "\"tab\": %d", &ival) == 1) fx->ShowBeatFXTab = (bool)ival;
        if (sscanf(strstr(p, "\"on\""), "\"on\": %d", &ival) == 1) fx->IsFXOn = (bool)ival;
    }

    // Color FX
    if ((p = strstr(json, "\"colorfx\""))) {
        if (sscanf(strstr(p, "\"active\""), "\"active\": %d", &ival) == 1) {
            if (cfxA) ColorFXManager_SetFX(cfxA, (ColorFXType)ival);
            if (cfxB) ColorFXManager_SetFX(cfxB, (ColorFXType)ival);
        }
        if (sscanf(strstr(p, "\"param\""), "\"param\": %f", &val) == 1) {
            if (cfxA) cfxA->parameter = val;
            if (cfxB) cfxB->parameter = val;
        }
        if (sscanf(strstr(p, "\"valA\""), "\"valA\": %f", &val) == 1 && cfxA) cfxA->colorValue = val;
        if (sscanf(strstr(p, "\"valB\""), "\"valB\": %f", &val) == 1 && cfxB) cfxB->colorValue = val;
    }

    // Quantize
    if ((p = strstr(json, "\"quantize\""))) {
        if (sscanf(strstr(p, "\"qA\""), "\"qA\": %d", &ival) == 1 && quantizeA) *quantizeA = (bool)ival;
        if (sscanf(strstr(p, "\"qB\""), "\"qB\": %d", &ival) == 1 && quantizeB) *quantizeB = (bool)ival;
    }

    // Master Volume
    if ((p = strstr(json, "\"master\""))) {
        if (sscanf(strstr(p, "\"vol\""), "\"vol\": %f", &val) == 1 && masterVolume) {
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            *masterVolume = val;
        }
    }
}

void Settings_Load(WaveformSettings *wfmA, WaveformSettings *wfmB, AudioBackendConfig *audio, BeatFXState *fx, ColorFXManager *cfxA, ColorFXManager *cfxB, char *controllerPath, bool *quantizeA, bool *quantizeB, float *masterVolume) {
    // Defaults
    controllerPath[0] = '\0';
    wfmA->Style = WAVEFORM_STYLE_RGB;
    wfmA->GainLow = 0.4f; wfmA->GainMid = 0.4f; wfmA->GainHigh = 0.4f;
    wfmA->VinylStartMs = 0.5f; wfmA->VinylStopMs = 1.0f; wfmA->LoadLock = true;
    wfmA->JogCalibRPM = 33.3f;
    *wfmB = *wfmA;
    
    audio->DeviceIndex = -1;
    audio->MasterOutL = 0; audio->MasterOutR = 1;
    audio->CueOutL = 2; audio->CueOutR = 3;
    audio->SampleRate = 48000; audio->BufferSizeFrames = 256; audio->PCMBitDepth = 16;
    audio->CrossfaderCurve = 0; // Smooth (Default)
    
    fx->SelectedFX = 0;
    fx->SelectedPad = 4; // 1 Beat
    fx->SelectedChannel = 0; // Master
    fx->LevelDepth = 0.5f;
    fx->Quantize = true;
    fx->IsFXOn = false;

    if (quantizeA) *quantizeA = true;
    if (quantizeB) *quantizeB = true;
    if (masterVolume) *masterVolume = 1.0f;

    if (cfxA) ColorFXManager_Init(cfxA);
    if (cfxB) ColorFXManager_Init(cfxB);

    char path[512];

#if defined(__ANDROID__)
    // Try SD Card first for easier user access, fallback to internal
    strncpy(path, "/sdcard/unx_settings.json", sizeof(path)-1);
    FILE *test = fopen(path, "r");
    if (test) fclose(test);
    else {
        FILE *testW = fopen(path, "w");
        if (testW) fclose(testW);
        else {
            // Fallback to internal
            snprintf(path, sizeof(path), "%s/settings.json", GetApplicationDirectory());
        }
    }
#elif defined(PLATFORM_IOS)
    extern const char* ios_get_documents_path(const char* filename);
    strncpy(path, ios_get_documents_path("settings.json"), sizeof(path)-1);
#else
    strncpy(path, "settings.json", sizeof(path)-1);
#endif

    // Ensure controllers directory exists based on detected platform path
    char baseDir[512] = ".";
#if defined(__ANDROID__)
    strncpy(baseDir, "/sdcard", sizeof(baseDir)-1);
#elif defined(PLATFORM_IOS)
    extern const char* ios_get_documents_path(const char* filename);
    strncpy(baseDir, ios_get_documents_path(""), sizeof(baseDir)-1);
#endif
    EnsureControllersExist(baseDir);

    FILE *f = fopen(path, "r");
    if (!f && strcmp(path, "settings.json") != 0) {
        f = fopen("settings.json", "r");
    }

    if (!f) {
        EnsureControllersExist("."); // Fallback to current dir if nothing else
        Settings_Save(*wfmA, *wfmB, *audio, *fx, *cfxA, *cfxB, controllerPath, quantizeA ? *quantizeA : true, quantizeB ? *quantizeB : true, masterVolume ? *masterVolume : 1.0f);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';
        LoadFromJSON(buf, wfmA, wfmB, audio, fx, cfxA, cfxB, quantizeA, quantizeB, masterVolume);
        
        // Extract controller path manually to avoid changing too many signatures
        const char *p;
        if ((p = strstr(buf, "\"controllers\""))) {
            char *start = strstr(p, "\"path\": \"");
            if (start) {
                start += 9;
                char *end = strchr(start, '\"');
                if (end) {
                    int len = end - start;
                    if (len > 255) len = 255;
                    strncpy(controllerPath, start, len);
                    controllerPath[len] = '\0';
                }
            }
        }
        free(buf);
    }
    fclose(f);
}

void Settings_Save(WaveformSettings wfmA, WaveformSettings wfmB, AudioBackendConfig audio, BeatFXState fx, ColorFXManager cfxA, ColorFXManager cfxB, const char *controllerPath, bool quantizeA, bool quantizeB, float masterVolume) {
    char path[512];

#if defined(__ANDROID__)
    // Try SD Card first for easier user access, fallback to internal
    strncpy(path, "/sdcard/unx_settings.json", sizeof(path)-1);
    FILE *test = fopen(path, "r");
    if (test) fclose(test);
    else {
        FILE *testW = fopen(path, "w");
        if (testW) fclose(testW);
        else {
            // Fallback to internal
            snprintf(path, sizeof(path), "%s/settings.json", GetApplicationDirectory());
        }
    }
#elif defined(PLATFORM_IOS)
    extern const char* ios_get_documents_path(const char* filename);
    strncpy(path, ios_get_documents_path("settings.json"), sizeof(path)-1);
#else
    strncpy(path, "settings.json", sizeof(path)-1);
#endif

    FILE *f = fopen(path, "w");
    if (!f && strcmp(path, "settings.json") != 0) {
        f = fopen("settings.json", "w");
    }
    
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"wfmA\": { \"style\": %d, \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"start\": %.1f, \"stop\": %.1f, \"lock\": %d, \"rpm\": %.1f },\n", 
            wfmA.Style, wfmA.GainLow, wfmA.GainMid, wfmA.GainHigh, wfmA.VinylStartMs, wfmA.VinylStopMs, wfmA.LoadLock ? 1 : 0, wfmA.JogCalibRPM);
    fprintf(f, "  \"wfmB\": { \"style\": %d, \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"start\": %.1f, \"stop\": %.1f, \"lock\": %d, \"rpm\": %.1f },\n", 
            wfmB.Style, wfmB.GainLow, wfmB.GainMid, wfmB.GainHigh, wfmB.VinylStartMs, wfmB.VinylStopMs, wfmB.LoadLock ? 1 : 0, wfmB.JogCalibRPM);
    fprintf(f, "  \"audio\": { \"devIdx\": %d, \"mastL\": %d, \"mastR\": %d, \"cueL\": %d, \"cueR\": %d, \"sr\": %d, \"buf\": %d, \"bitdepth\": %d, \"xfader\": %d },\n",
            audio.DeviceIndex, audio.MasterOutL, audio.MasterOutR, audio.CueOutL, audio.CueOutR, audio.SampleRate, audio.BufferSizeFrames, audio.PCMBitDepth, audio.CrossfaderCurve);
    fprintf(f, "  \"beatfx\": { \"fx\": %d, \"pad\": %d, \"ch\": %d, \"depth\": %.2f, \"q\": %d, \"tab\": %d, \"on\": %d },\n",
            fx.SelectedFX, fx.SelectedPad, fx.SelectedChannel, fx.LevelDepth, fx.Quantize ? 1 : 0, fx.ShowBeatFXTab ? 1 : 0, fx.IsFXOn ? 1 : 0);
    fprintf(f, "  \"colorfx\": { \"active\": %d, \"param\": %.2f, \"valA\": %.2f, \"valB\": %.2f },\n",
            (int)cfxA.activeFX, cfxA.parameter, cfxA.colorValue, cfxB.colorValue);
    fprintf(f, "  \"quantize\": { \"qA\": %d, \"qB\": %d },\n", quantizeA ? 1 : 0, quantizeB ? 1 : 0);
    fprintf(f, "  \"master\": { \"vol\": %.3f },\n", masterVolume);
    fprintf(f, "  \"controllers\": { \"path\": \"%s\" }\n", controllerPath ? controllerPath : "");
    fprintf(f, "}\n");

    fclose(f);
}
