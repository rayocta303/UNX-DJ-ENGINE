#include "settings_io.h"
#include "jog_config.h"
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

static void LoadFromJSON(const char* json, WaveformSettings *wfmA, WaveformSettings *wfmB, AudioBackendConfig *audio, BeatFXState *fx, ColorFXManager *cfxA, ColorFXManager *cfxB, bool *quantizeA, bool *quantizeB, float *masterVolume, JogConfig *jog) {
    if (!json) return;
    
    const char* p = json;
    const char* sub = NULL;
    float val;
    int ival;

    // Deck A
    if ((p = strstr(json, "\"wfmA\""))) {
        if ((sub = strstr(p, "\"style\"")) && sscanf(sub, "\"style\": %d", &ival) == 1) wfmA->Style = (WaveformStyle)ival;
        if ((sub = strstr(p, "\"low\"")) && sscanf(sub, "\"low\": %f", &val) == 1) wfmA->GainLow = val;
        if ((sub = strstr(p, "\"mid\"")) && sscanf(sub, "\"mid\": %f", &val) == 1) wfmA->GainMid = val;
        if ((sub = strstr(p, "\"high\"")) && sscanf(sub, "\"high\": %f", &val) == 1) wfmA->GainHigh = val;
        if ((sub = strstr(p, "\"start\"")) && sscanf(sub, "\"start\": %f", &val) == 1) {
            if (val > 16.0f) val = 0.5f; // Convert legacy millisecond settings to Bar
            wfmA->VinylStartMs = val;
        }
        if ((sub = strstr(p, "\"stop\"")) && sscanf(sub, "\"stop\": %f", &val) == 1) {
            if (val > 16.0f) val = 1.0f; // Convert legacy millisecond settings to Bar
            wfmA->VinylStopMs = val;
        }
        if ((sub = strstr(p, "\"lock\"")) && sscanf(sub, "\"lock\": %d", &ival) == 1) wfmA->LoadLock = (bool)ival;
        if ((sub = strstr(p, "\"rpm\"")) && sscanf(sub, "\"rpm\": %f", &val) == 1) wfmA->JogCalibRPM = val;
        // WaveformTouch: default enabled (1) if field absent (backwards compat)
        wfmA->WaveformTouchEnabled = true;
        if ((sub = strstr(p, "\"touch\"")) && sscanf(sub, "\"touch\": %d", &ival) == 1) wfmA->WaveformTouchEnabled = (bool)ival;
        if ((sub = strstr(p, "\"qres\"")) && sscanf(sub, "\"qres\": %d", &ival) == 1) wfmA->QuantizeResolution = ival;
    }

    // Deck B
    if ((p = strstr(json, "\"wfmB\""))) {
        if ((sub = strstr(p, "\"style\"")) && sscanf(sub, "\"style\": %d", &ival) == 1) wfmB->Style = (WaveformStyle)ival;
        if ((sub = strstr(p, "\"low\"")) && sscanf(sub, "\"low\": %f", &val) == 1) wfmB->GainLow = val;
        if ((sub = strstr(p, "\"mid\"")) && sscanf(sub, "\"mid\": %f", &val) == 1) wfmB->GainMid = val;
        if ((sub = strstr(p, "\"high\"")) && sscanf(sub, "\"high\": %f", &val) == 1) wfmB->GainHigh = val;
        if ((sub = strstr(p, "\"start\"")) && sscanf(sub, "\"start\": %f", &val) == 1) {
            if (val > 16.0f) val = 0.5f;
            wfmB->VinylStartMs = val;
        }
        if ((sub = strstr(p, "\"stop\"")) && sscanf(sub, "\"stop\": %f", &val) == 1) {
            if (val > 16.0f) val = 1.0f;
            wfmB->VinylStopMs = val;
        }
        if ((sub = strstr(p, "\"lock\"")) && sscanf(sub, "\"lock\": %d", &ival) == 1) wfmB->LoadLock = (bool)ival;
        if ((sub = strstr(p, "\"rpm\"")) && sscanf(sub, "\"rpm\": %f", &val) == 1) wfmB->JogCalibRPM = val;
        wfmB->WaveformTouchEnabled = true;
        if ((sub = strstr(p, "\"touch\"")) && sscanf(sub, "\"touch\": %d", &ival) == 1) wfmB->WaveformTouchEnabled = (bool)ival;
        if ((sub = strstr(p, "\"qres\"")) && sscanf(sub, "\"qres\": %d", &ival) == 1) wfmB->QuantizeResolution = ival;
    }

    // Audio
    if ((p = strstr(json, "\"audio\""))) {
        if ((sub = strstr(p, "\"devIdx\"")) && sscanf(sub, "\"devIdx\": %d", &ival) == 1) audio->DeviceIndex = ival;
        if ((sub = strstr(p, "\"mastL\"")) && sscanf(sub, "\"mastL\": %d", &ival) == 1) audio->MasterOutL = ival;
        if ((sub = strstr(p, "\"mastR\"")) && sscanf(sub, "\"mastR\": %d", &ival) == 1) audio->MasterOutR = ival;
        if ((sub = strstr(p, "\"cueL\"")) && sscanf(sub, "\"cueL\": %d", &ival) == 1) audio->CueOutL = ival;
        if ((sub = strstr(p, "\"cueR\"")) && sscanf(sub, "\"cueR\": %d", &ival) == 1) audio->CueOutR = ival;
        if ((sub = strstr(p, "\"sr\"")) && sscanf(sub, "\"sr\": %d", &ival) == 1) audio->SampleRate = ival;
        if ((sub = strstr(p, "\"buf\"")) && sscanf(sub, "\"buf\": %d", &ival) == 1) audio->BufferSizeFrames = ival;
        if ((sub = strstr(p, "\"bitdepth\"")) && sscanf(sub, "\"bitdepth\": %d", &ival) == 1) audio->PCMBitDepth = ival;
        if ((sub = strstr(p, "\"xfader\"")) && sscanf(sub, "\"xfader\": %d", &ival) == 1) audio->CrossfaderCurve = ival;
    }
    // Beat FX
    if ((p = strstr(json, "\"beatfx\""))) {
        if ((sub = strstr(p, "\"fx\"")) && sscanf(sub, "\"fx\": %d", &ival) == 1) fx->SelectedFX = ival;
        if ((sub = strstr(p, "\"pad\"")) && sscanf(sub, "\"pad\": %d", &ival) == 1) fx->SelectedPad = ival;
        if ((sub = strstr(p, "\"ch\"")) && sscanf(sub, "\"ch\": %d", &ival) == 1) fx->SelectedChannel = ival;
        if ((sub = strstr(p, "\"depth\"")) && sscanf(sub, "\"depth\": %f", &val) == 1) fx->LevelDepth = val;
        if ((sub = strstr(p, "\"q\"")) && sscanf(sub, "\"q\": %d", &ival) == 1) fx->Quantize = (bool)ival;
        if ((sub = strstr(p, "\"tab\"")) && sscanf(sub, "\"tab\": %d", &ival) == 1) fx->ShowBeatFXTab = (bool)ival;
        if ((sub = strstr(p, "\"on\"")) && sscanf(sub, "\"on\": %d", &ival) == 1) fx->IsFXOn = (bool)ival;
    }

    // Color FX
    if ((p = strstr(json, "\"colorfx\""))) {
        if ((sub = strstr(p, "\"active\"")) && sscanf(sub, "\"active\": %d", &ival) == 1) {
            if (cfxA) ColorFXManager_SetFX(cfxA, (ColorFXType)ival);
            if (cfxB) ColorFXManager_SetFX(cfxB, (ColorFXType)ival);
        }
        if ((sub = strstr(p, "\"param\"")) && sscanf(sub, "\"param\": %f", &val) == 1) {
            if (cfxA) cfxA->parameter = val;
            if (cfxB) cfxB->parameter = val;
        }
        if ((sub = strstr(p, "\"valA\"")) && sscanf(sub, "\"valA\": %f", &val) == 1 && cfxA) cfxA->colorValue = val;
        if ((sub = strstr(p, "\"valB\"")) && sscanf(sub, "\"valB\": %f", &val) == 1 && cfxB) cfxB->colorValue = val;
    }

    // Quantize
    if ((p = strstr(json, "\"quantize\""))) {
        if ((sub = strstr(p, "\"qA\"")) && sscanf(sub, "\"qA\": %d", &ival) == 1 && quantizeA) *quantizeA = (bool)ival;
        if ((sub = strstr(p, "\"qB\"")) && sscanf(sub, "\"qB\": %d", &ival) == 1 && quantizeB) *quantizeB = (bool)ival;
    }

    // Master Volume
    if ((p = strstr(json, "\"master\""))) {
        if ((sub = strstr(p, "\"vol\"")) && sscanf(sub, "\"vol\": %f", &val) == 1 && masterVolume) {
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            *masterVolume = val;
        }
    }

    // Jogwheel Config
    if (jog && (p = strstr(json, "\"jog\""))) {
        if ((sub = strstr(p, "\"rpm\"")) && sscanf(sub, "\"rpm\": %f", &val) == 1) jog->DefaultRPM = val;
        if ((sub = strstr(p, "\"tpr\"")) && sscanf(sub, "\"tpr\": %f", &val) == 1) jog->TicksPerRev = val;
        if ((sub = strstr(p, "\"eraw\"")) && sscanf(sub, "\"eraw\": %f", &val) == 1) jog->EmaRawWeight = val;
        if ((sub = strstr(p, "\"eprev\"")) && sscanf(sub, "\"eprev\": %f", &val) == 1) jog->EmaPrevWeight = val;
        if ((sub = strstr(p, "\"vfric\"")) && sscanf(sub, "\"vfric\": %f", &val) == 1) jog->VinylReleaseFriction = val;
        if ((sub = strstr(p, "\"vcutoff\"")) && sscanf(sub, "\"vcutoff\": %f", &val) == 1) jog->VinylReleaseCutoff = val;
        if ((sub = strstr(p, "\"pbfric\"")) && sscanf(sub, "\"pbfric\": %f", &val) == 1) jog->PitchBendFriction = val;
        if ((sub = strstr(p, "\"pbscale\"")) && sscanf(sub, "\"pbscale\": %f", &val) == 1) jog->PitchBendScale = val;
        if ((sub = strstr(p, "\"nudge\"")) && sscanf(sub, "\"nudge\": %f", &val) == 1) jog->WaveformNudgeScale = val;
    }
}

void Settings_Load(WaveformSettings *wfmA, WaveformSettings *wfmB, AudioBackendConfig *audio, BeatFXState *fx, ColorFXManager *cfxA, ColorFXManager *cfxB, char *controllerPath, bool *quantizeA, bool *quantizeB, float *masterVolume, JogConfig *jog) {
    // Defaults
    controllerPath[0] = '\0';
    wfmA->Style = WAVEFORM_STYLE_RGB;
    wfmA->GainLow = 0.4f; wfmA->GainMid = 0.4f; wfmA->GainHigh = 0.4f;
    wfmA->VinylStartMs = 0.5f; wfmA->VinylStopMs = 1.0f; wfmA->LoadLock = true;
    wfmA->JogCalibRPM = 33.3f;
    wfmA->WaveformTouchEnabled = true;
    wfmA->QuantizeResolution = 3; // Default 1 BEAT
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
        Settings_Save(*wfmA, *wfmB, *audio, *fx, *cfxA, *cfxB, controllerPath, quantizeA ? *quantizeA : true, quantizeB ? *quantizeB : true, masterVolume ? *masterVolume : 1.0f, jog);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';
        LoadFromJSON(buf, wfmA, wfmB, audio, fx, cfxA, cfxB, quantizeA, quantizeB, masterVolume, jog);
        
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

void Settings_Save(WaveformSettings wfmA, WaveformSettings wfmB, AudioBackendConfig audio, BeatFXState fx, ColorFXManager cfxA, ColorFXManager cfxB, const char *controllerPath, bool quantizeA, bool quantizeB, float masterVolume, const JogConfig *jog) {
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
    fprintf(f, "  \"wfmA\": { \"style\": %d, \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"start\": %.1f, \"stop\": %.1f, \"lock\": %d, \"rpm\": %.1f, \"touch\": %d, \"qres\": %d },\n", 
            wfmA.Style, wfmA.GainLow, wfmA.GainMid, wfmA.GainHigh, wfmA.VinylStartMs, wfmA.VinylStopMs, wfmA.LoadLock ? 1 : 0, wfmA.JogCalibRPM, wfmA.WaveformTouchEnabled ? 1 : 0, wfmA.QuantizeResolution);
    fprintf(f, "  \"wfmB\": { \"style\": %d, \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"start\": %.1f, \"stop\": %.1f, \"lock\": %d, \"rpm\": %.1f, \"touch\": %d, \"qres\": %d },\n", 
            wfmB.Style, wfmB.GainLow, wfmB.GainMid, wfmB.GainHigh, wfmB.VinylStartMs, wfmB.VinylStopMs, wfmB.LoadLock ? 1 : 0, wfmB.JogCalibRPM, wfmB.WaveformTouchEnabled ? 1 : 0, wfmB.QuantizeResolution);
    fprintf(f, "  \"audio\": { \"devIdx\": %d, \"mastL\": %d, \"mastR\": %d, \"cueL\": %d, \"cueR\": %d, \"sr\": %d, \"buf\": %d, \"bitdepth\": %d, \"xfader\": %d },\n",
            audio.DeviceIndex, audio.MasterOutL, audio.MasterOutR, audio.CueOutL, audio.CueOutR, audio.SampleRate, audio.BufferSizeFrames, audio.PCMBitDepth, audio.CrossfaderCurve);
    fprintf(f, "  \"beatfx\": { \"fx\": %d, \"pad\": %d, \"ch\": %d, \"depth\": %.2f, \"q\": %d, \"tab\": %d, \"on\": %d },\n",
            fx.SelectedFX, fx.SelectedPad, fx.SelectedChannel, fx.LevelDepth, fx.Quantize ? 1 : 0, fx.ShowBeatFXTab ? 1 : 0, fx.IsFXOn ? 1 : 0);
    fprintf(f, "  \"colorfx\": { \"active\": %d, \"param\": %.2f, \"valA\": %.2f, \"valB\": %.2f },\n",
            (int)cfxA.activeFX, cfxA.parameter, cfxA.colorValue, cfxB.colorValue);
    fprintf(f, "  \"quantize\": { \"qA\": %d, \"qB\": %d },\n", quantizeA ? 1 : 0, quantizeB ? 1 : 0);
    fprintf(f, "  \"master\": { \"vol\": %.3f },\n", masterVolume);
    if (jog) {
        fprintf(f, "  \"jog\": { \"rpm\": %.6f, \"tpr\": %.2f, \"eraw\": %.4f, \"eprev\": %.4f, \"vfric\": %.4f, \"vcutoff\": %.6f, \"pbfric\": %.4f, \"pbscale\": %.4f, \"nudge\": %.4f },\n",
                jog->DefaultRPM, jog->TicksPerRev, jog->EmaRawWeight, jog->EmaPrevWeight,
                jog->VinylReleaseFriction, jog->VinylReleaseCutoff,
                jog->PitchBendFriction, jog->PitchBendScale, jog->WaveformNudgeScale);
    }
    fprintf(f, "  \"controllers\": { \"path\": \"%s\" }\n", controllerPath ? controllerPath : "");
    fprintf(f, "}\n");

    fclose(f);
}
