#include "settings_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void LoadFromJSON(const char* json, WaveformSettings *wfmA, WaveformSettings *wfmB) {
    if (!json) return;
    
    // Very simple "parser" looking for specific keys
    // Schema: {"wfmA": {"style": 0, "low": 1.0, "mid": 1.0, "high": 1.0}, "wfmB": ...}
    
    const char* p = json;
    float val;
    int style;

    // Deck A
    if ((p = strstr(json, "\"wfmA\""))) {
        if (sscanf(strstr(p, "\"style\""), "\"style\": %d", &style) == 1) wfmA->Style = (WaveformStyle)style;
        if (sscanf(strstr(p, "\"low\""), "\"low\": %f", &val) == 1) wfmA->GainLow = val;
        if (sscanf(strstr(p, "\"mid\""), "\"mid\": %f", &val) == 1) wfmA->GainMid = val;
        if (sscanf(strstr(p, "\"high\""), "\"high\": %f", &val) == 1) wfmA->GainHigh = val;
        if (sscanf(strstr(p, "\"start\""), "\"start\": %f", &val) == 1) wfmA->VinylStartMs = val;
        if (sscanf(strstr(p, "\"stop\""), "\"stop\": %f", &val) == 1) wfmA->VinylStopMs = val;
        if (sscanf(strstr(p, "\"lock\""), "\"lock\": %d", &style) == 1) wfmA->LoadLock = (bool)style;
    }

    // Deck B
    if ((p = strstr(json, "\"wfmB\""))) {
        if (sscanf(strstr(p, "\"style\""), "\"style\": %d", &style) == 1) wfmB->Style = (WaveformStyle)style;
        if (sscanf(strstr(p, "\"low\""), "\"low\": %f", &val) == 1) wfmB->GainLow = val;
        if (sscanf(strstr(p, "\"mid\""), "\"mid\": %f", &val) == 1) wfmB->GainMid = val;
        if (sscanf(strstr(p, "\"high\""), "\"high\": %f", &val) == 1) wfmB->GainHigh = val;
        if (sscanf(strstr(p, "\"start\""), "\"start\": %f", &val) == 1) wfmB->VinylStartMs = val;
        if (sscanf(strstr(p, "\"stop\""), "\"stop\": %f", &val) == 1) wfmB->VinylStopMs = val;
        if (sscanf(strstr(p, "\"lock\""), "\"lock\": %d", &style) == 1) wfmB->LoadLock = (bool)style;
    }
}

void Settings_Load(WaveformSettings *wfmA, WaveformSettings *wfmB) {
    // Defaults
    wfmA->Style = WAVEFORM_STYLE_3BAND;
    wfmA->GainLow = 1.0f;
    wfmA->GainMid = 1.0f;
    wfmA->GainHigh = 1.0f;
    wfmA->VinylStartMs = 500.0f;
    wfmA->VinylStopMs = 1200.0f;
    wfmA->LoadLock = true;
    
    *wfmB = *wfmA;

    FILE *f = fopen("settings.json", "r");
    if (!f) {
        // If file doesn't exist, create it with defaults immediately
        Settings_Save(*wfmA, *wfmB);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';
        LoadFromJSON(buf, wfmA, wfmB);
        free(buf);
    }
    fclose(f);
}

void Settings_Save(WaveformSettings wfmA, WaveformSettings wfmB) {
    FILE *f = fopen("settings.json", "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"wfmA\": { \"style\": %d, \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"start\": %.1f, \"stop\": %.1f, \"lock\": %d },\n", 
            wfmA.Style, wfmA.GainLow, wfmA.GainMid, wfmA.GainHigh, wfmA.VinylStartMs, wfmA.VinylStopMs, wfmA.LoadLock ? 1 : 0);
    fprintf(f, "  \"wfmB\": { \"style\": %d, \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"start\": %.1f, \"stop\": %.1f, \"lock\": %d }\n", 
            wfmB.Style, wfmB.GainLow, wfmB.GainMid, wfmB.GainHigh, wfmB.VinylStartMs, wfmB.VinylStopMs, wfmB.LoadLock ? 1 : 0);
    fprintf(f, "}\n");

    fclose(f);
}
