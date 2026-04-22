#include "settings_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"

static void LoadFromJSON(const char* json, WaveformSettings *wfmA, WaveformSettings *wfmB, AudioBackendConfig *audio) {
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
        if (sscanf(strstr(p, "\"start\""), "\"start\": %f", &val) == 1) wfmA->VinylStartMs = val;
        if (sscanf(strstr(p, "\"stop\""), "\"stop\": %f", &val) == 1) wfmA->VinylStopMs = val;
        if (sscanf(strstr(p, "\"lock\""), "\"lock\": %d", &ival) == 1) wfmA->LoadLock = (bool)ival;
    }

    // Deck B
    if ((p = strstr(json, "\"wfmB\""))) {
        if (sscanf(strstr(p, "\"style\""), "\"style\": %d", &ival) == 1) wfmB->Style = (WaveformStyle)ival;
        if (sscanf(strstr(p, "\"low\""), "\"low\": %f", &val) == 1) wfmB->GainLow = val;
        if (sscanf(strstr(p, "\"mid\""), "\"mid\": %f", &val) == 1) wfmB->GainMid = val;
        if (sscanf(strstr(p, "\"high\""), "\"high\": %f", &val) == 1) wfmB->GainHigh = val;
        if (sscanf(strstr(p, "\"start\""), "\"start\": %f", &val) == 1) wfmB->VinylStartMs = val;
        if (sscanf(strstr(p, "\"stop\""), "\"stop\": %f", &val) == 1) wfmB->VinylStopMs = val;
        if (sscanf(strstr(p, "\"lock\""), "\"lock\": %d", &ival) == 1) wfmB->LoadLock = (bool)ival;
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
    }
}

void Settings_Load(WaveformSettings *wfmA, WaveformSettings *wfmB, AudioBackendConfig *audio) {
    // Defaults
    wfmA->Style = WAVEFORM_STYLE_3BAND;
    wfmA->GainLow = 1.0f; wfmA->GainMid = 1.0f; wfmA->GainHigh = 1.0f;
    wfmA->VinylStartMs = 500.0f; wfmA->VinylStopMs = 1200.0f; wfmA->LoadLock = true;
    *wfmB = *wfmA;
    
    audio->DeviceIndex = -1;
    audio->MasterOutL = 0; audio->MasterOutR = 1;
    audio->CueOutL = 2; audio->CueOutR = 3;
    audio->SampleRate = 48000; audio->BufferSizeFrames = 256;

    char path[512];
    const char *basePath = "";

#if defined(__ANDROID__)
    basePath = GetApplicationDirectory();
    snprintf(path, sizeof(path), "%s/settings.json", basePath);
#elif defined(PLATFORM_IOS)
    extern const char* ios_get_documents_path(const char* filename);
    strncpy(path, ios_get_documents_path("settings.json"), sizeof(path)-1);
#else
    strncpy(path, "settings.json", sizeof(path)-1);
#endif

    FILE *f = fopen(path, "r");
    if (!f && strcmp(path, "settings.json") != 0) {
        f = fopen("settings.json", "r");
    }

    if (!f) {
        Settings_Save(*wfmA, *wfmB, *audio);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';
        LoadFromJSON(buf, wfmA, wfmB, audio);
        free(buf);
    }
    fclose(f);
}

void Settings_Save(WaveformSettings wfmA, WaveformSettings wfmB, AudioBackendConfig audio) {
    char path[512];
    const char *basePath = "";

#if defined(__ANDROID__)
    basePath = GetApplicationDirectory();
    snprintf(path, sizeof(path), "%s/settings.json", basePath);
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
    fprintf(f, "  \"wfmA\": { \"style\": %d, \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"start\": %.1f, \"stop\": %.1f, \"lock\": %d },\n", 
            wfmA.Style, wfmA.GainLow, wfmA.GainMid, wfmA.GainHigh, wfmA.VinylStartMs, wfmA.VinylStopMs, wfmA.LoadLock ? 1 : 0);
    fprintf(f, "  \"wfmB\": { \"style\": %d, \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"start\": %.1f, \"stop\": %.1f, \"lock\": %d },\n", 
            wfmB.Style, wfmB.GainLow, wfmB.GainMid, wfmB.GainHigh, wfmB.VinylStartMs, wfmB.VinylStopMs, wfmB.LoadLock ? 1 : 0);
    fprintf(f, "  \"audio\": { \"devIdx\": %d, \"mastL\": %d, \"mastR\": %d, \"cueL\": %d, \"cueR\": %d, \"sr\": %d, \"buf\": %d }\n",
            audio.DeviceIndex, audio.MasterOutL, audio.MasterOutR, audio.CueOutL, audio.CueOutR, audio.SampleRate, audio.BufferSizeFrames);
    fprintf(f, "}\n");

    fclose(f);
}
