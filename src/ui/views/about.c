#include "ui/views/about.h"
#include "version.h"
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

  float viewH = SCREEN_HEIGHT - DECK_STR_H;

  // Background overlay
  DrawRectangle(0, 0, SCREEN_WIDTH, viewH, ColorBGUtil);

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
  float centerY = viewH / 2.0f;

  // Main Card Layout
  float cardW = S(380);
  float cardH = S(240); 
  float cardX = centerX - (cardW / 2.0f);
  float cardY = centerY - (cardH / 2.0f) + (TOP_BAR_H / 2.0f);

  // Card Shadow & Background
  DrawRectangle(cardX + S(4), cardY + S(4), cardW, cardH, Fade(ColorBlack, 0.5f));
  DrawRectangle(cardX, cardY, cardW, cardH, ColorDark1);
  DrawRectangleLinesEx((Rectangle){cardX, cardY, cardW, cardH}, 1.0f, ColorGray);
  
  // Left Sidebar of the Card (Branding)
  float sideW = S(110);
  DrawRectangle(cardX + 1, cardY + 1, sideW, cardH - 2, (Color){25, 25, 25, 255});
  DrawLine(cardX + sideW, cardY + 1, cardX + sideW, cardY + cardH - 1, ColorDark2);

  // Large Device Icon in Sidebar
  UIDrawText("\xef\x8a\x92", UIFonts_GetIcon(S(48)), cardX + sideW/2 - S(24), cardY + S(40), S(48), ColorShadow);
  DrawCentredText("XDJ-UNX", faceXS, cardX, sideW, cardY + S(95), S(8), ColorGray);
  DrawCentredText("SYSTEM", faceXS, cardX, sideW, cardY + S(105), S(8), ColorGray);

  // Right Side Content
  float contentX = cardX + sideW + S(20);
  float startY = cardY + S(20);
  float rowH = S(38);

  // App Title & Version
  UIDrawText(APP_NAME, faceLg, contentX, startY, S(18), ColorOrange);
  UIDrawText(r->State->Version, faceSm, contentX, startY + S(22), S(10), ColorShadow);

  float detailsY = startY + S(50);

  // --- Row 1: Developer ---
  UIDrawText("\xef\x80\x87", iconMain, contentX, detailsY + S(2), S(12), ColorShadow);
  UIDrawText("DEVELOPER", faceXS, contentX + S(18), detailsY - S(4), S(7), ColorShadow);
  UIDrawText(r->State->Developer, faceMd, contentX + S(18), detailsY + S(6), S(11), ColorWhite);

  // --- Row 2: Instagram ---
  UIDrawText("\xef\x85\xad", iconBrand, contentX, detailsY + rowH + S(2), S(12), ColorOrange);
  UIDrawText("INSTAGRAM", faceXS, contentX + S(18), detailsY + rowH - S(4), S(7), ColorShadow);
  UIDrawText(r->State->Instagram, faceMd, contentX + S(18), detailsY + rowH + S(6), S(11), ColorOrange);

  // --- Row 3: Platform ---
  UIDrawText("\xef\x90\xbc", iconMain, contentX, detailsY + rowH * 2 + S(2), S(12), ColorShadow);
  UIDrawText("PLATFORM", faceXS, contentX + S(18), detailsY + rowH * 2 - S(4), S(7), ColorShadow);
  UIDrawText(APP_PLATFORM, faceMd, contentX + S(18), detailsY + rowH * 2 + S(6), S(11), ColorWhite);

  // --- Row 4: Audio ---
  UIDrawText("\xef\x8a\x93", iconMain, contentX, detailsY + rowH * 3 + S(2), S(12), ColorShadow);
  UIDrawText("AUDIO ENGINE", faceXS, contentX + S(18), detailsY + rowH * 3 - S(4), S(7), ColorShadow);
  char audioBuf[128];
  snprintf(audioBuf, 128, "%s (%s)", r->State->AudioDevice, r->State->AudioDriver);
  UIDrawText(audioBuf, faceSm, contentX + S(18), detailsY + rowH * 3 + S(6), S(9), ColorGray);

  // Footer Hints
  UIDrawText("Engineered for precision performance.", faceXS, centerX - S(70), viewH - S(25), S(8), ColorDark3);
  UIDrawText("Press BACK to exit", faceXS, SCREEN_WIDTH - S(100), viewH - S(25), S(8), ColorShadow);
}

void AboutRenderer_Init(AboutRenderer *r, AboutState *state) {
  r->base.Update = About_Update;
  r->base.Draw = About_Draw;
  r->State = state;
}
