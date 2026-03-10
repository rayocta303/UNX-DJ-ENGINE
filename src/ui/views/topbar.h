#pragma once

#include "ui/components/component.h"

typedef struct TopBar TopBar;

struct TopBar {
  Component base;

  // Callbacks
  void (*OnBrowse)(void *);
  void (*OnTagList)(void *);
  void (*OnInfo)(void *);
  void (*OnSettings)(void *);
  void *callbackCtx;

  float BatteryLevel; // 0.0 to 1.0

  // Internal layout state
  float MarginX;
  float btnBrowseX, btnBrowseW;
  float btnTagListX, btnTagListW;
  float btnInfoX, btnInfoW;
  float btnSettingsX, btnSettingsW;
};

// Initializes a TopBar.
void TopBar_Init(TopBar *t);
