#pragma once

#include "ui/components/component.h"
#include <stdbool.h>

#define MAX_SETTING_OPTIONS 8
#define MAX_SETTINGS_ITEMS 32

typedef struct {
    char Label[64];
    char Options[MAX_SETTING_OPTIONS][32];
    int OptionsCount;
    int Current;
} SettingItem;

typedef struct {
    bool IsActive;
    SettingItem Items[MAX_SETTINGS_ITEMS];
    int ItemsCount;
    int CursorPos;
    int Scroll;
} SettingsState;

typedef struct SettingsRenderer SettingsRenderer;

struct SettingsRenderer {
    Component base;
    SettingsState *State;
    void (*OnClose)(void*);
    void *callbackCtx;
};

void SettingsRenderer_Init(SettingsRenderer *r, SettingsState *state);
