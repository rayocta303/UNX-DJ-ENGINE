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

  // 1. Left
  UIDrawText("REMAIN", faceXS, t->MarginX, S(6), S(8), ColorShadow);
  UIDrawText("00:00:00", faceSm, t->MarginX + S(36), S(5), S(9), ColorWhite);

  // 2. Center Group
  float btnH = TOP_BAR_H - S(4);
  float btnY = S(2);
  float btnSpacing = S(8);

  t->btnBrowseW = S(84);
  t->btnMixerW = t->btnBrowseW;
  t->btnInfoW = t->btnBrowseW;
  t->btnSettingsW = t->btnBrowseW;

  float totalCenterW = (t->btnBrowseW * 4) + (btnSpacing * 3);
  float centerX = (SCREEN_WIDTH - totalCenterW) / 2.0f;

  t->btnBrowseX = centerX;
  t->btnMixerX = t->btnBrowseX + t->btnBrowseW + btnSpacing;
  t->btnInfoX = t->btnMixerX + t->btnMixerW + btnSpacing;
  t->btnSettingsX = t->btnInfoX + t->btnInfoW + btnSpacing;

  Font iconFace = UIFonts_GetIcon(S(8));

  // Draw BROWSE
  DrawRectangle(t->btnBrowseX, btnY, t->btnBrowseW, btnH,
                t->ActiveScreen == ScreenBrowser ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnBrowseX, btnY, t->btnBrowseW, btnH, ColorShadow);
  {
    float iconW = MeasureTextEx(iconFace, "\xef\x80\x82", S(8), 1.0f).x; // f002
    float textW = MeasureTextEx(faceSm, "BROWSE", S(9), 1.0f).x;
    float spacing = S(4);
    float totalW = iconW + spacing + textW;
    float startX = t->btnBrowseX + (t->btnBrowseW - totalW) / 2.0f;
    // UIDrawText("\xef\x80\x82", iconFace, startX, btnY + S(3), S(8),
    // ColorWhite);
    UIDrawText("BROWSE", faceSm, startX + iconW + spacing, btnY + S(2.5f), S(9),
               ColorWhite);
  }

  // Draw MIXER
  DrawRectangle(t->btnMixerX, btnY, t->btnMixerW, btnH,
                t->ActiveScreen == ScreenMixer ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnMixerX, btnY, t->btnMixerW, btnH, ColorShadow);
  {
    float iconW = MeasureTextEx(iconFace, "\xef\x87\xde", S(8), 1.0f).x; // f1de
    float textW = MeasureTextEx(faceSm, "MIXER", S(9), 1.0f).x;
    float spacing = S(4);
    float totalW = iconW + spacing + textW;
    float startX = t->btnMixerX + (t->btnMixerW - totalW) / 2.0f;
    // UIDrawText("\xef\x87\xde", iconFace, startX, btnY + S(3), S(8),
    // ColorWhite);
    UIDrawText("MIXER", faceSm, startX + iconW + spacing, btnY + S(2.5f), S(9),
               ColorWhite);
  }

  // Draw INFO
  DrawRectangle(t->btnInfoX, btnY, t->btnInfoW, btnH,
                t->ActiveScreen == ScreenInfo ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnInfoX, btnY, t->btnInfoW, btnH, ColorShadow);
  {
    float iconW = MeasureTextEx(iconFace, "\xef\x84\xa9", S(8), 1.0f).x; // f129
    float textW = MeasureTextEx(faceSm, "INFO", S(9), 1.0f).x;
    float spacing = S(4);
    float totalW = iconW + spacing + textW;
    float startX = t->btnInfoX + (t->btnInfoW - totalW) / 2.0f;
    // UIDrawText("\xef\x84\xa9", iconFace, startX, btnY + S(3), S(8),
    // ColorWhite);
    UIDrawText("INFO", faceSm, startX + iconW + spacing, btnY + S(2.5f), S(9),
               ColorWhite);
  }

  // Draw MENU
  DrawRectangle(t->btnSettingsX, btnY, t->btnSettingsW, btnH,
                t->ActiveScreen == ScreenSettings ? ColorBlue : ColorDark1);
  DrawRectangleLines(t->btnSettingsX, btnY, t->btnSettingsW, btnH, ColorShadow);
  {
    float iconW = MeasureTextEx(iconFace, "\xef\x80\x93", S(8), 1.0f).x; // f013
    float textW = MeasureTextEx(faceSm, "MENU", S(9), 1.0f).x;
    float spacing = S(4);
    float totalW = iconW + spacing + textW;
    float startX = t->btnSettingsX + (t->btnSettingsW - totalW) / 2.0f;
    // UIDrawText("\xef\x80\x93", iconFace, startX, btnY + S(3), S(8),
    // ColorWhite);
    UIDrawText("MENU", faceSm, startX + iconW + spacing, btnY + S(2.5f), S(9),
               ColorWhite);
  }

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
  }

  UIDrawText("LINK   MIDI", faceSm, pctX - S(68), S(5), S(9), ColorWhite);
}

void TopBar_Init(TopBar *t) {
  t->base.Update = TopBar_Update;
  t->base.Draw = TopBar_Draw;
  t->MarginX = S(4);
  t->BatteryLevel = 0.85f;
  t->OnBrowse = NULL;
  t->OnMixer = NULL;
  t->OnInfo = NULL;
  t->OnSettings = NULL;
  t->callbackCtx = NULL;
}
