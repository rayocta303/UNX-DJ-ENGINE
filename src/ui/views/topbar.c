#include "ui/views/topbar.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>

static int TopBar_Update(Component *base) {
  TopBar *t = (TopBar *)base;

#if !defined(PLATFORM_DRM)
  if (UICheckClick((Rectangle){ 0, 0, t->btnFullX + t->btnFullW + S(4), TOP_BAR_H })) {
    ToggleFullscreen();
  }
#endif
  if (UICheckClick((Rectangle){ t->btnBrowseX - S(3), 0, t->btnBrowseW + S(6), TOP_BAR_H })) {
    if (t->OnBrowse)
      t->OnBrowse(t->callbackCtx);
  }
  if (UICheckClick((Rectangle){ t->btnMixerX - S(3), 0, t->btnMixerW + S(6), TOP_BAR_H })) {
    if (t->OnMixer)
      t->OnMixer(t->callbackCtx);
  }
  if (UICheckClick((Rectangle){ t->btnPadX - S(3), 0, t->btnPadW + S(6), TOP_BAR_H })) {
    if (t->OnPad)
      t->OnPad(t->callbackCtx);
  }
  if (UICheckClick((Rectangle){ t->btnInfoX - S(3), 0, t->btnInfoW + S(6), TOP_BAR_H })) {
    if (t->OnInfo)
      t->OnInfo(t->callbackCtx);
  }
  if (UICheckClick((Rectangle){ t->btnSettingsX - S(3), 0, t->btnSettingsW + S(6), TOP_BAR_H })) {
    if (t->OnSettings)
      t->OnSettings(t->callbackCtx);
  }
  return 0;
}

static void TopBar_Draw(Component *base) {
  TopBar *t = (TopBar *)base;
  DrawRectangle(0, 0, SCREEN_WIDTH, TOP_BAR_H, ColorDark2);
  DrawLine(0, TOP_BAR_H, SCREEN_WIDTH, TOP_BAR_H, ColorDark1);

  Font faceXS = UIFonts_GetFace(S(8));
  Font faceSm = UIFonts_GetFace(S(9.5f));

  float btnH = TOP_BAR_H - S(4);
  float btnY = S(2);

#if !defined(PLATFORM_DRM)
  // 1. Fullscreen Button (Top-Left corner before CPU/RAM)
  t->btnFullX = t->MarginX;
  t->btnFullW = S(22);

  bool isFull = IsWindowFullscreen();
  DrawRectangle(t->btnFullX, btnY, t->btnFullW, btnH, isFull ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnFullX, btnY, t->btnFullW, btnH, ColorShadow);

  float cx = t->btnFullX + t->btnFullW / 2.0f;
  float cy = btnY + btnH / 2.0f;
  float iconS = S(5.0f);
  Color iconCol = ColorWhite;

  if (isFull) {
    // Inward arrows / shrink icon
    DrawLineEx((Vector2){cx - iconS, cy - iconS}, (Vector2){cx - S(2.0f), cy - S(2.0f)}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx + iconS, cy - iconS}, (Vector2){cx + S(2.0f), cy - S(2.0f)}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx - iconS, cy + iconS}, (Vector2){cx - S(2.0f), cy + S(2.0f)}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx + iconS, cy + iconS}, (Vector2){cx + S(2.0f), cy + S(2.0f)}, 1.5f, iconCol);
  } else {
    // Outward brackets / expand icon
    DrawLineEx((Vector2){cx - iconS, cy - iconS}, (Vector2){cx - iconS + S(3.0f), cy - iconS}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx - iconS, cy - iconS}, (Vector2){cx - iconS, cy - iconS + S(3.0f)}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx + iconS, cy - iconS}, (Vector2){cx + iconS - S(3.0f), cy - iconS}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx + iconS, cy - iconS}, (Vector2){cx + iconS, cy - iconS + S(3.0f)}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx - iconS, cy + iconS}, (Vector2){cx - iconS + S(3.0f), cy + iconS}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx - iconS, cy + iconS}, (Vector2){cx - iconS, cy + iconS - S(3.0f)}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx + iconS, cy + iconS}, (Vector2){cx + iconS - S(3.0f), cy + iconS}, 1.5f, iconCol);
    DrawLineEx((Vector2){cx + iconS, cy + iconS}, (Vector2){cx + iconS, cy + iconS - S(3.0f)}, 1.5f, iconCol);
  }

  // 2. CPU & RAM Usage (Positioned after Fullscreen Button)
  float textStartX = t->btnFullX + t->btnFullW + S(6);
#else
  t->btnFullX = t->MarginX;
  t->btnFullW = 0;
  float textStartX = t->MarginX + S(4);
#endif
  char cpuRamStr[64];
  snprintf(cpuRamStr, sizeof(cpuRamStr), "CPU: %d%%  |  RAM: %dMB", (int)(t->CPUUsage * 100), (int)t->RAMUsage);
  float sysTextY = (TOP_BAR_H - S(9.5f)) / 2.0f;
  UIDrawText(cpuRamStr, faceSm, textStartX, sysTextY, S(9.5f), ColorWhite);

  // 3. Center Group
  float btnSpacing = S(6);

  t->btnBrowseW = S(80); // Enlarged button width for touch accuracy
  t->btnMixerW = t->btnBrowseW;
  t->btnInfoW = t->btnBrowseW;
  t->btnSettingsW = t->btnBrowseW;
  t->btnPadW = t->btnBrowseW;

  float totalCenterW = (t->btnBrowseW * 5) + (btnSpacing * 4);
  float centerX = (SCREEN_WIDTH - totalCenterW) / 2.0f;

  t->btnBrowseX = centerX;
  t->btnMixerX = t->btnBrowseX + t->btnBrowseW + btnSpacing;
  t->btnPadX = t->btnMixerX + t->btnMixerW + btnSpacing;
  t->btnInfoX = t->btnPadX + t->btnPadW + btnSpacing;
  t->btnSettingsX = t->btnInfoX + t->btnInfoW + btnSpacing;

  Font faceBold = UIFonts_GetBoldFace(S(10.5f));
  float textY = btnY + (btnH - S(10.5f)) / 2.0f - S(1.0f);

  // Draw BROWSE
  DrawRectangle(t->btnBrowseX, btnY, t->btnBrowseW, btnH,
                t->ActiveScreen == ScreenBrowser ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnBrowseX, btnY, t->btnBrowseW, btnH, ColorShadow);
  DrawCentredText("BROWSE", faceBold, t->btnBrowseX, t->btnBrowseW, textY, S(10.5f), ColorWhite);

  // Draw MIXER
  DrawRectangle(t->btnMixerX, btnY, t->btnMixerW, btnH,
                t->ActiveScreen == ScreenMixer ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnMixerX, btnY, t->btnMixerW, btnH, ColorShadow);
  DrawCentredText("MIXER", faceBold, t->btnMixerX, t->btnMixerW, textY, S(10.5f), ColorWhite);

  // Draw PAD
  DrawRectangle(t->btnPadX, btnY, t->btnPadW, btnH,
                t->ActiveScreen == ScreenPad ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnPadX, btnY, t->btnPadW, btnH, ColorShadow);
  DrawCentredText("PAD", faceBold, t->btnPadX, t->btnPadW, textY, S(10.5f), ColorWhite);

  // Draw INFO
  DrawRectangle(t->btnInfoX, btnY, t->btnInfoW, btnH,
                t->ActiveScreen == ScreenInfo ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnInfoX, btnY, t->btnInfoW, btnH, ColorShadow);
  DrawCentredText("INFO", faceBold, t->btnInfoX, t->btnInfoW, textY, S(10.5f), ColorWhite);

  // Draw MENU
  DrawRectangle(t->btnSettingsX, btnY, t->btnSettingsW, btnH,
                t->ActiveScreen == ScreenSettings ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnSettingsX, btnY, t->btnSettingsW, btnH, ColorShadow);
  DrawCentredText("MENU", faceBold, t->btnSettingsX, t->btnSettingsW, textY, S(10.5f), ColorWhite);

  // 3. Right Status & Battery
  float batW = S(22);
  float batH = S(11);
  float batX = SCREEN_WIDTH - t->MarginX - batW - S(15);
  float batY = (TOP_BAR_H - batH) / 2.0f;

  char batPctStr[16];
  sprintf(batPctStr, "%d%%", (int)(t->BatteryLevel * 100));
  float tw = MeasureTextEx(faceSm, batPctStr, S(9.5f), 1.0f).x;
  float pctX = batX - tw - S(5);
  UIDrawText(batPctStr, faceSm, pctX, (TOP_BAR_H - S(9.5f)) / 2.0f, S(9.5f), ColorWhite);

  DrawRectangle(batX, batY, batW, batH, ColorDark3);
  DrawRectangleLines(batX, batY, batW, batH, ColorShadow);
  DrawRectangle(batX + batW, batY + S(3.0f), S(2.0f), S(5.0f), ColorShadow);

  if (t->BatteryLevel > 0) {
    float gap = 1.5f;
    int segments = 4;
    float segGap = 1.0f;
    float totalSegW = batW - (gap * 2);
    float segW = (totalSegW - ((segments - 1) * segGap)) / segments;

    Color fillColor = ColorDGreen;
    if (t->BatteryLevel < 0.25f)
      fillColor = ColorRed;
    else if (t->BatteryLevel < 0.5f)
      fillColor = ColorOrange;

    for (int i = 0; i < segments; i++) {
      if (t->BatteryLevel >= (float)(i + 1) / segments - 0.05f) {
        DrawRectangle(batX + gap + i * (segW + segGap), batY + gap, segW,
                      batH - (gap * 2), fillColor);
      }
    }

    // Charging indicator (Bolt icon or "+" sign)
    if (t->IsCharging) {
        UIDrawText("+", faceXS, batX + batW + S(4), batY, S(9), ColorYellow);
    }
  }
}

void TopBar_Init(TopBar *t) {
  t->base.Update = TopBar_Update;
  t->base.Draw = TopBar_Draw;
  t->MarginX = S(4);
  t->BatteryLevel = 0.85f;
  t->CPUUsage = 0;
  t->RAMUsage = 0;
  t->OnBrowse = NULL;
  t->OnMixer = NULL;
  t->OnInfo = NULL;
  t->OnSettings = NULL;
  t->OnPad = NULL;
  t->callbackCtx = NULL;
}
