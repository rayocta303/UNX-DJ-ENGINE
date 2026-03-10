#include "ui/views/about.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>

static int About_Update(Component *base) {
  AboutRenderer *r = (AboutRenderer *)base;
  if (!r->State->IsActive)
    return 0;

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
    r->State->IsActive = false;
  }
  return 0;
}

static void About_Draw(Component *base) {
  AboutRenderer *r = (AboutRenderer *)base;
  if (!r->State->IsActive)
    return;

  // Background overlay
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBGUtil);

  // Header section
  DrawRectangle(0, 0, SCREEN_WIDTH, TOP_BAR_H, ColorDark1);
  DrawLine(0, TOP_BAR_H, SCREEN_WIDTH, TOP_BAR_H, ColorShadow);

  Font faceLg = UIFonts_GetFace(S(18));
  Font faceMd = UIFonts_GetFace(S(13));
  Font faceSm = UIFonts_GetFace(S(10));
  Font faceXS = UIFonts_GetFace(S(8));
  Font iconMain = UIFonts_GetIcon(S(14));
  Font iconBrand = UIFonts_GetIconBrand(S(14));

  UIDrawText("ABOUT SYSTEM", UIFonts_GetFace(S(12)), S(15), S(7), S(12),
             ColorWhite);

  float centerX = SCREEN_WIDTH / 2.0f;
  float centerY = SCREEN_HEIGHT / 2.0f;

  // Main Card Layout
  float cardW = S(320);
  float cardH = S(210);
  float cardX = centerX - (cardW / 2.0f);
  float cardY = centerY - (cardH / 2.0f) + S(10);

  // Card Shadow Effect
  DrawRectangle(cardX + S(2), cardY + S(2), cardW, cardH, ColorBlack);
  DrawRectangle(cardX, cardY, cardW, cardH, ColorDark1);
  DrawRectangleLinesEx((Rectangle){cardX, cardY, cardW, cardH}, 1.0f,
                       ColorGray);

  // Brand Section (Top part of card)
  UIDrawText("UNX DJ ENGINE", faceLg, centerX - S(55), cardY + S(25), S(18),
             ColorOrange);
  UIDrawText(r->State->Version, faceSm, centerX - S(30), cardY + S(50), S(10),
             ColorShadow);

  DrawLine(cardX + S(20), cardY + S(75), cardX + cardW - S(20), cardY + S(75),
           ColorGray);

  // Content Section (Bottom part of card)
  float labelX = cardX + S(30);
  float valueX = cardX + S(120);
  float rowSpacing = S(35);
  float startY = cardY + S(95);

  // Developer Info
  UIDrawText("\xef\x80\x87", iconMain, labelX - S(10), startY, S(14),
             ColorShadow); // User icon
  UIDrawText("DEVELOPER", faceXS, labelX + S(15), startY - S(6), S(8),
             ColorShadow);
  UIDrawText(r->State->Developer, faceMd, labelX + S(15), startY + S(6), S(13),
             ColorWhite);

  // Social Info
  UIDrawText("\xef\x85\xad", iconBrand, labelX - S(10), startY + rowSpacing,
             S(14), ColorOrange); // Instagram icon
  UIDrawText("INSTAGRAM", faceXS, labelX + S(15), startY + rowSpacing - S(6),
             S(8), ColorShadow);
  UIDrawText(r->State->Instagram, faceMd, labelX + S(15),
             startY + rowSpacing + S(6), S(13), ColorOrange);

  // System Info
  UIDrawText("\xef\x90\xbc", iconMain, labelX - S(10), startY + rowSpacing * 2,
             S(14), ColorShadow); // Microchip icon
  UIDrawText("PLATFORM", faceXS, labelX + S(15), startY + rowSpacing * 2 - S(6),
             S(8), ColorShadow);
  UIDrawText("Native (Pico Engine)", faceMd, labelX + S(15),
             startY + rowSpacing * 2 + S(6), S(13), ColorWhite);

  // Bottom text hints
  UIDrawText("Build for everyone.", faceXS, centerX - S(40),
             SCREEN_HEIGHT - S(25), S(8), ColorDark1);
  UIDrawText("Press BACK to return", faceXS, SCREEN_WIDTH - S(110),
             SCREEN_HEIGHT - S(25), S(8), ColorShadow);
}

void AboutRenderer_Init(AboutRenderer *r, AboutState *state) {
  r->base.Update = About_Update;
  r->base.Draw = About_Draw;
  r->State = state;
}
