#pragma once

#include "raylib.h"
#include <stdbool.h>

// Reference resolution â€” 640Ã—400 (16:10)
#define REF_WIDTH       640
#define REF_HEIGHT      400

// Dynamic resolution (Internal scaled reference)
#define SCREEN_WIDTH    (REF_WIDTH * UI_CurrScale)
#define SCREEN_HEIGHT   (REF_HEIGHT * UI_CurrScale)

// Waveform geometry
#define DYN_WFM_WIDTH   30000
#define DYN_WFM_HEIGHT  41
#define DYN_WFM_CENTRE  20
#define STATIC_WFM_W    200
#define STATIC_WFM_H    30

// Layout zones (tuned for 640x400 to match reference)
#define TOP_BAR_H       S(20.0f)
#define SIDE_PANEL_W    S(110.0f)
#define BEAT_FX_W       SIDE_PANEL_W
#define BEAT_FX_X       (SCREEN_WIDTH - SIDE_PANEL_W)
#define FX_BAR_H        S(44.0f)
#define DECK_STR_H      S(96.0f)
#define WAVE_AREA_H     (SCREEN_HEIGHT - TOP_BAR_H - FX_BAR_H - DECK_STR_H)

// Colors
extern const Color ColorPaper;
extern const Color ColorShadow;
extern const Color ColorDGreen;
extern const Color ColorDark1;
extern const Color ColorDark2;
extern const Color ColorDark3;
extern const Color ColorBGUtil;
extern const Color ColorBlack;
extern const Color ColorWhite;
extern const Color ColorRed;
extern const Color ColorOrange;
extern const Color ColorBlue;
extern const Color ColorCue;
extern const Color ColorGray;

// Screen modes
#define MODE_PLAYER     0
#define MODE_BROWSER    1
#define MODE_INFO       2
#define MODE_SETTINGS   3

// S scales a base (unscaled) pixel value by Scale.
// Use for all hardcoded offsets, paddings, sizes, and font points.
float S(float v);
int Si(int v);

// Call once per frame if resizing is allowed
void UI_UpdateScale(void);
extern float UI_CurrScale;
extern float UI_OffsetX;
extern float UI_OffsetY;
extern bool UI_BoldEnabled;

Vector2 UIGetMousePosition(void);
