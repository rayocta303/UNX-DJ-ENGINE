#pragma once

#include "ui/components/component.h"
#include <stdbool.h>

#define MAX_SETTING_OPTIONS 32
#define MAX_SETTINGS_ITEMS 512

typedef enum {
  SETTING_TYPE_LIST,
  SETTING_TYPE_KNOB,
  SETTING_TYPE_ACTION
} SettingType;

typedef enum {
  SETTING_CAT_DECK,
  SETTING_CAT_AUDIO,
  SETTING_CAT_VIEW,
  SETTING_CAT_SYSTEM,
  SETTING_CAT_CONTROLLERS,
  SETTING_CAT_COUNT
} SettingCategory;

typedef struct {
  char Label[64];
  SettingType Type;
  SettingCategory Category;

  // List part
  char Options[MAX_SETTING_OPTIONS][32];
  int OptionsCount;
  int Current;

  // Knob part
  float Value;
  float Min;
  float Max;
  char Unit[16];
  float Step; // Custom increment step (0 = auto 5%)
} SettingItem;

typedef struct {
  bool IsActive;
  SettingItem Items[MAX_SETTINGS_ITEMS];
  int ItemsCount;
  int CursorPos;
  int Scroll;
  int SelectedTab;
  float TouchDragAccumulator;
  bool IsDropdownOpen;
  int DropdownItemIdx;
  float DropdownScroll;
  
  bool IsLearningMidi;
  int LearningItemIdx;
} SettingsState;

typedef struct SettingsRenderer SettingsRenderer;

struct SettingsRenderer {
  Component base;
  SettingsState *State;
  void (*OnClose)(void *);
  void (*OnApply)(void *);
  void (*OnAction)(void *, int);
  void (*OnValueChanged)(void *, int);
  void *callbackCtx;
};

void SettingsRenderer_Init(SettingsRenderer *r, SettingsState *state);
