#pragma once

#include "ui/components/component.h"
#include <stdbool.h>

#define MAX_SETTING_OPTIONS 8
#define MAX_SETTINGS_ITEMS 32

typedef enum {
  SETTING_TYPE_LIST,
  SETTING_TYPE_KNOB,
  SETTING_TYPE_ACTION
} SettingType;

typedef struct {
  char Label[64];
  SettingType Type;

  // List part
  char Options[MAX_SETTING_OPTIONS][32];
  int OptionsCount;
  int Current;

  // Knob part
  float Value;
  float Min;
  float Max;
  char Unit[16];
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
  void (*OnClose)(void *);
  void (*OnApply)(void *);
  void (*OnAction)(void *, int);
  void *callbackCtx;
};

void SettingsRenderer_Init(SettingsRenderer *r, SettingsState *state);
