#pragma once

#include "ui/components/component.h"

typedef enum {
    ScreenPlayer,
    ScreenBrowser,
    ScreenInfo,
    ScreenSettings,
    ScreenAbout,
    ScreenMixer,
    ScreenSplash,
    ScreenDebug
} CurrentScreen;

typedef struct TopBar TopBar;

struct TopBar {
  Component base;

  // Callbacks
  void (*OnBrowse)(void *);
  void (*OnMixer)(void *);
  void (*OnInfo)(void *);
  void (*OnSettings)(void *);
  void *callbackCtx;

  float BatteryLevel; // 0.0 to 1.0
  float CPUUsage;     // 0.0 to 1.0
  float RAMUsage;     // MB

  // Internal layout state
  float MarginX;
  float btnBrowseX, btnBrowseW;
  float btnMixerX, btnMixerW;
  float btnInfoX, btnInfoW;
  float btnSettingsX, btnSettingsW;
  
  CurrentScreen ActiveScreen; // Defines current app screen to highlight
};

// Initializes a TopBar.
void TopBar_Init(TopBar *t);
