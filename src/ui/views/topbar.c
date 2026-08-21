#include "ui/views/topbar.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "input/input.h"
#include <stdio.h>

static int TopBar_Update(Component *base) {
  TopBar *t = (TopBar *)base;

#if !defined(PLATFORM_DRM)
  if (Touch_CheckClick((Rectangle){ 0, 0, t->btnFullX + t->btnFullW, TOP_BAR_H }, S(2.0f))) {
    ToggleFullscreen();
  }
#endif
  if (Touch_CheckClick((Rectangle){ t->btnBrowseX, 0, t->btnBrowseW, TOP_BAR_H }, S(2.0f))) {
    if (t->OnBrowse)
      t->OnBrowse(t->callbackCtx);
  }
  if (Touch_CheckClick((Rectangle){ t->btnMixerX, 0, t->btnMixerW, TOP_BAR_H }, S(2.0f))) {
    if (t->OnMixer)
      t->OnMixer(t->callbackCtx);
  }
  if (Touch_CheckClick((Rectangle){ t->btnPadX, 0, t->btnPadW, TOP_BAR_H }, S(2.0f))) {
    if (t->OnPad)
      t->OnPad(t->callbackCtx);
  }
  if (Touch_CheckClick((Rectangle){ t->btnInfoX, 0, t->btnInfoW, TOP_BAR_H }, S(2.0f))) {
    if (t->OnInfo)
      t->OnInfo(t->callbackCtx);
  }
  if (Touch_CheckClick((Rectangle){ t->btnSettingsX, 0, t->btnSettingsW, TOP_BAR_H }, S(2.0f))) {
    if (t->OnSettings)
      t->OnSettings(t->callbackCtx);
  }
  return 0;
}

static void TopBar_Draw(Component *base) {
  TopBar *t = (TopBar *)base;
  DrawRectangle(0, 0, SCREEN_WIDTH, TOP_BAR_H, Theme.BgPanel);
  DrawLine(0, TOP_BAR_H, SCREEN_WIDTH, TOP_BAR_H, Theme.BgMain);

  Font faceXS = UIFonts_GetFace(S(8));
  Font faceSm = UIFonts_GetFace(S(9.5f));

  float btnH = TOP_BAR_H - S(4);
  float btnY = S(2);

#if !defined(PLATFORM_DRM)
  // 1. Fullscreen Button (Top-Left corner before CPU/RAM)
  t->btnFullX = t->MarginX;
  t->btnFullW = S(22);

  bool isFull = IsWindowFullscreen();
  DrawRectangle(t->btnFullX, btnY, t->btnFullW, btnH, isFull ? Theme.AccentBlue : Theme.BgMain);
  DrawRectangleLines(t->btnFullX, btnY, t->btnFullW, btnH, Theme.BorderDefault);

  float cx = t->btnFullX + t->btnFullW / 2.0f;
  float cy = btnY + btnH / 2.0f;
  float iconS = S(5.0f);
  Color iconCol = Theme.TextPrimary;

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
  // 2. CPU & RAM Telemetry Progress Bars (Stacked 2-row layout)
  float row1Y = S(3.5f);
  float row2Y = S(16.0f);
  float barH = S(6.5f);
  float barW = S(34.0f);

  float lblX = textStartX;
  float barX = lblX + S(20.0f);
  float valX = barX + barW + S(4.0f);

  Font faceMicro = UIFonts_GetFace(S(8.0f));

  // --- Row 1: CPU ---
  float cpuUsage = t->CPUUsage;
  if (cpuUsage < 0.0f) cpuUsage = 0.0f;
  if (cpuUsage > 1.0f) cpuUsage = 1.0f;

  UIDrawText("CPU", faceMicro, lblX, row1Y, S(8.0f), Theme.TextSecondary);

  DrawRectangleRounded((Rectangle){ barX, row1Y + S(1.0f), barW, barH }, 0.3f, 4, Theme.BgMain);
  DrawRectangleRoundedLines((Rectangle){ barX, row1Y + S(1.0f), barW, barH }, 0.3f, 4, 1.0f, Theme.BorderDefault);

  float cpuFillW = (barW - S(2.0f)) * cpuUsage;
  if (cpuFillW > S(1.0f)) {
    Color cpuCol = (cpuUsage > 0.85f) ? Theme.AccentRed : ((cpuUsage > 0.60f) ? Theme.AccentOrange : Theme.AccentGreen);
    DrawRectangleRounded((Rectangle){ barX + S(1.0f), row1Y + S(2.0f), cpuFillW, barH - S(2.0f) }, 0.3f, 4, cpuCol);
  }

  char cpuStr[16];
  snprintf(cpuStr, sizeof(cpuStr), "%d%%", (int)(cpuUsage * 100.0f));
  UIDrawText(cpuStr, faceMicro, valX, row1Y, S(8.0f), Theme.TextPrimary);

  // --- Row 2: RAM (2-Segment Progress Bar: App Usage + System Usage) ---
  float ramTotalMB = (t->RAMTotal > 0.0f) ? t->RAMTotal : 4096.0f;
  float ramAppMB = (t->RAMApp >= 0.0f) ? t->RAMApp : 0.0f;
  if (ramAppMB > t->RAMUsage) ramAppMB = t->RAMUsage;

  float ramSysMB = t->RAMUsage - ramAppMB;
  if (ramSysMB < 0.0f) ramSysMB = 0.0f;

  float appRatio = ramAppMB / ramTotalMB;
  float sysRatio = ramSysMB / ramTotalMB;
  float totalRatio = (ramAppMB + ramSysMB) / ramTotalMB;

  if (appRatio < 0.0f) appRatio = 0.0f;
  if (sysRatio < 0.0f) sysRatio = 0.0f;
  if (totalRatio > 1.0f) totalRatio = 1.0f;

  UIDrawText("RAM", faceMicro, lblX, row2Y, S(8.0f), Theme.TextSecondary);

  // Outer container
  DrawRectangleRounded((Rectangle){ barX, row2Y + S(1.0f), barW, barH }, 0.3f, 4, Theme.BgMain);
  DrawRectangleRoundedLines((Rectangle){ barX, row2Y + S(1.0f), barW, barH }, 0.3f, 4, 1.0f, Theme.BorderDefault);

  float maxInnerW = barW - S(2.0f);
  float appFillW = maxInnerW * appRatio;
  float sysFillW = maxInnerW * sysRatio;

  // Segment 1: App Usage (Cyan / Blue)
  if (appFillW > S(0.5f)) {
    Color appCol = Theme.AccentBlue; // Bright Cyan
    DrawRectangle((int)(barX + S(1.0f)), (int)(row2Y + S(2.0f)), (int)appFillW, (int)(barH - S(2.0f)), appCol);
  }

  // Segment 2: Other System Usage (Orange / Amber / Red)
  if (sysFillW > S(0.5f)) {
    Color sysCol = (totalRatio > 0.85f) ? Theme.AccentRed : ((totalRatio > 0.70f) ? Theme.AccentOrange : Theme.AccentOrange);
    DrawRectangle((int)(barX + S(1.0f) + appFillW), (int)(row2Y + S(2.0f)), (int)sysFillW, (int)(barH - S(2.0f)), sysCol);
  }

  char ramStr[32];
  snprintf(ramStr, sizeof(ramStr), "%dMB", (int)t->RAMUsage);
  UIDrawText(ramStr, faceMicro, valX, row2Y, S(8.0f), Theme.TextPrimary);

  // 3. Center Group
  float btnSpacing = S(6);

  t->btnBrowseW = S(88); // Enlarged button width for touch accuracy
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
                t->ActiveScreen == ScreenBrowser ? Theme.AccentBlue : Theme.BgMain);
  DrawRectangleLines(t->btnBrowseX, btnY, t->btnBrowseW, btnH, Theme.BorderDefault);
  DrawCentredText("BROWSE", faceBold, t->btnBrowseX, t->btnBrowseW, textY, S(10.5f), Theme.TextPrimary);

  // Draw MIXER
  DrawRectangle(t->btnMixerX, btnY, t->btnMixerW, btnH,
                t->ActiveScreen == ScreenMixer ? Theme.AccentBlue : Theme.BgMain);
  DrawRectangleLines(t->btnMixerX, btnY, t->btnMixerW, btnH, Theme.BorderDefault);
  DrawCentredText("MIXER", faceBold, t->btnMixerX, t->btnMixerW, textY, S(10.5f), Theme.TextPrimary);

  // Draw PAD
  DrawRectangle(t->btnPadX, btnY, t->btnPadW, btnH,
                t->ActiveScreen == ScreenPad ? Theme.AccentBlue : Theme.BgMain);
  DrawRectangleLines(t->btnPadX, btnY, t->btnPadW, btnH, Theme.BorderDefault);
  DrawCentredText("PAD", faceBold, t->btnPadX, t->btnPadW, textY, S(10.5f), Theme.TextPrimary);

  // Draw INFO
  DrawRectangle(t->btnInfoX, btnY, t->btnInfoW, btnH,
                t->ActiveScreen == ScreenInfo ? Theme.AccentBlue : Theme.BgMain);
  DrawRectangleLines(t->btnInfoX, btnY, t->btnInfoW, btnH, Theme.BorderDefault);
  DrawCentredText("INFO", faceBold, t->btnInfoX, t->btnInfoW, textY, S(10.5f), Theme.TextPrimary);

  // Draw MENU
  DrawRectangle(t->btnSettingsX, btnY, t->btnSettingsW, btnH,
                t->ActiveScreen == ScreenSettings ? Theme.AccentBlue : Theme.BgMain);
  DrawRectangleLines(t->btnSettingsX, btnY, t->btnSettingsW, btnH, Theme.BorderDefault);
  DrawCentredText("MENU", faceBold, t->btnSettingsX, t->btnSettingsW, textY, S(10.5f), Theme.TextPrimary);

  // 3. Right Status & Battery
  float batW = S(22);
  float batH = S(11);
  float batX = SCREEN_WIDTH - t->MarginX - batW - S(15);
  float batY = (TOP_BAR_H - batH) / 2.0f;

  char batPctStr[16];
  sprintf(batPctStr, "%d%%", (int)(t->BatteryLevel * 100));
  float tw = MeasureTextEx(faceSm, batPctStr, S(9.5f), 1.0f).x;
  float pctX = batX - tw - S(5);
  UIDrawText(batPctStr, faceSm, pctX, (TOP_BAR_H - S(9.5f)) / 2.0f, S(9.5f), Theme.TextPrimary);

  DrawRectangle(batX, batY, batW, batH, Theme.BgPanelAlt);
  DrawRectangleLines(batX, batY, batW, batH, Theme.BorderDefault);
  DrawRectangle(batX + batW, batY + S(3.0f), S(2.0f), S(5.0f), Theme.BorderDefault);

  if (t->BatteryLevel > 0) {
    float gap = 1.5f;
    int segments = 4;
    float segGap = 1.0f;
    float totalSegW = batW - (gap * 2);
    float segW = (totalSegW - ((segments - 1) * segGap)) / segments;

    Color fillColor = Theme.AccentGreen;
    if (t->BatteryLevel < 0.25f)
      fillColor = Theme.AccentRed;
    else if (t->BatteryLevel < 0.5f)
      fillColor = Theme.AccentOrange;

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
  t->RAMTotal = 0;
  t->RAMApp = 0;
  t->OnBrowse = NULL;
  t->OnMixer = NULL;
  t->OnInfo = NULL;
  t->OnSettings = NULL;
  t->OnPad = NULL;
  t->callbackCtx = NULL;
}
