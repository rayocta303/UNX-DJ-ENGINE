#include "ui/views/topbar.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>

static int TopBar_Update(Component *base) {
  TopBar *t = (TopBar *)base;
  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    Vector2 mouse = UIGetMousePosition();
    if (mouse.y < TOP_BAR_H) {
      if (mouse.x >= t->btnBrowseX &&
          mouse.x <= t->btnBrowseX + t->btnBrowseW) {
        if (t->OnBrowse)
          t->OnBrowse(t->callbackCtx);
      }
      if (mouse.x >= t->btnMixerX && mouse.x <= t->btnMixerX + t->btnMixerW) {
        if (t->OnMixer)
          t->OnMixer(t->callbackCtx);
      }
      if (mouse.x >= t->btnInfoX && mouse.x <= t->btnInfoX + t->btnInfoW) {
        if (t->OnInfo)
          t->OnInfo(t->callbackCtx);
      }
      if (mouse.x >= t->btnSettingsX &&
          mouse.x <= t->btnSettingsX + t->btnSettingsW) {
        if (t->OnSettings)
          t->OnSettings(t->callbackCtx);
      }
      if (mouse.x >= t->btnPadX && mouse.x <= t->btnPadX + t->btnPadW) {
        if (t->OnPad)
          t->OnPad(t->callbackCtx);
      }
    }
  }
  return 0;
}

static void TopBar_Draw(Component *base) {
  TopBar *t = (TopBar *)base;
  DrawRectangle(0, 0, SCREEN_WIDTH, TOP_BAR_H, ColorDark2);
  DrawLine(0, TOP_BAR_H, SCREEN_WIDTH, TOP_BAR_H, ColorDark1);

  Font faceXS = UIFonts_GetFace(S(8));
  Font faceSm = UIFonts_GetFace(S(9));

  // 1. Left (CPU & RAM Usage)
  char cpuStr[32];
  sprintf(cpuStr, "CPU %d%%", (int)(t->CPUUsage * 100));
  UIDrawText(cpuStr, faceXS, t->MarginX, S(6), S(8), ColorShadow);

  char ramStr[32];
  sprintf(ramStr, "RAM %dMB", (int)t->RAMUsage);
  UIDrawText(ramStr, faceSm, t->MarginX + S(36), S(5), S(9), ColorWhite);

  // 2. Center Group
  float btnH = TOP_BAR_H - S(4);
  float btnY = S(2);
  float btnSpacing = S(8);

  t->btnBrowseW = S(72); // Reduced width to fit 5 buttons
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

  Font faceBold = UIFonts_GetBoldFace(S(9));

  // Draw BROWSE
  DrawRectangle(t->btnBrowseX, btnY, t->btnBrowseW, btnH,
                t->ActiveScreen == ScreenBrowser ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnBrowseX, btnY, t->btnBrowseW, btnH, ColorShadow);
  DrawCentredText("BROWSE", faceBold, t->btnBrowseX, t->btnBrowseW, btnY + S(2.5f), S(9), ColorWhite);

  // Draw MIXER
  DrawRectangle(t->btnMixerX, btnY, t->btnMixerW, btnH,
                t->ActiveScreen == ScreenMixer ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnMixerX, btnY, t->btnMixerW, btnH, ColorShadow);
  DrawCentredText("MIXER", faceBold, t->btnMixerX, t->btnMixerW, btnY + S(2.5f), S(9), ColorWhite);

  // Draw PAD
  DrawRectangle(t->btnPadX, btnY, t->btnPadW, btnH,
                t->ActiveScreen == ScreenPad ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnPadX, btnY, t->btnPadW, btnH, ColorShadow);
  DrawCentredText("PAD", faceBold, t->btnPadX, t->btnPadW, btnY + S(2.5f), S(9), ColorWhite);

  // Draw INFO
  DrawRectangle(t->btnInfoX, btnY, t->btnInfoW, btnH,
                t->ActiveScreen == ScreenInfo ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnInfoX, btnY, t->btnInfoW, btnH, ColorShadow);
  DrawCentredText("INFO", faceBold, t->btnInfoX, t->btnInfoW, btnY + S(2.5f), S(9), ColorWhite);

  // Draw MENU
  DrawRectangle(t->btnSettingsX, btnY, t->btnSettingsW, btnH,
                t->ActiveScreen == ScreenSettings ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnSettingsX, btnY, t->btnSettingsW, btnH, ColorShadow);
  DrawCentredText("MENU", faceBold, t->btnSettingsX, t->btnSettingsW, btnY + S(2.5f), S(9), ColorWhite);

  // 3. Right Status & Battery
  float batW = S(20);
  float batH = S(9);
  float batX = SCREEN_WIDTH - t->MarginX - batW - S(4);
  float batY = (TOP_BAR_H - batH) / 2;

  char batPctStr[16];
  sprintf(batPctStr, "%d%%", (int)(t->BatteryLevel * 100));
  float tw = MeasureTextEx(faceSm, batPctStr, S(9), 1.0f).x;
  float pctX = batX - tw - S(5);
  UIDrawText(batPctStr, faceSm, pctX, S(5), S(9), ColorWhite);

  DrawRectangle(batX, batY, batW, batH, ColorDark3);
  DrawRectangleLines(batX, batY, batW, batH, ColorShadow);
  DrawRectangle(batX + batW, batY + S(2.5f), S(1.5f), S(4), ColorShadow);

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
        UIDrawText("+", faceXS, batX + batW + S(4), batY, S(8), ColorYellow);
    }
  }

  UIDrawText("LINK   MIDI", faceSm, pctX - S(68), S(5), S(9), ColorWhite);
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
