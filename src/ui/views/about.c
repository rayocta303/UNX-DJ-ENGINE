#include "ui/views/about.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "version.h"
#include "input/input.h"
#include <stdio.h>

static int About_Update(Component *base) {
  AboutRenderer *r = (AboutRenderer *)base;
  if (!r->State->IsActive)
    return 0;

  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  float centerX = SCREEN_WIDTH / 2.0f;
  float centerY = viewH / 2.0f;

  float cardW = S(380);
  float cardH = S(240);
  float cardX = centerX - (cardW / 2.0f);
  float cardY = centerY - (cardH / 2.0f) + (TOP_BAR_H / 2.0f);
  Rectangle cardRect = {cardX, cardY, cardW, cardH};

  Vector2 mouse = Input_GetPointerPos();
  bool closeClicked = false;

  // Tap outside card to close
  if (Touch_CheckClickInArea((Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, 0) && !CheckCollisionPointRec(mouse, cardRect)) {
      closeClicked = true;
      Input_Consume();
  }

  // Close button hitbox
  float btnW = S(90);
  float btnH = S(24);
  float btnX = cardX + cardW - btnW - S(20);
  float btnY = cardY + cardH - btnH - S(10);
  if (Touch_CheckClick((Rectangle){btnX, btnY, btnW, btnH}, S(5))) {
      closeClicked = true;
      Input_Consume();
  }

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || closeClicked) {
    r->State->IsActive = false;
    Input_Consume();
  }
  return 0;
}

static void About_Draw(Component *base) {
  AboutRenderer *r = (AboutRenderer *)base;
  if (!r->State->IsActive)
    return;

  float viewH = SCREEN_HEIGHT - DECK_STR_H;

  // Background overlay
  DrawRectangle(0, 0, SCREEN_WIDTH, viewH, Theme.BgMain);

  // Header section
  DrawRectangle(0, 0, SCREEN_WIDTH, TOP_BAR_H, Theme.BgMain);
  DrawLine(0, TOP_BAR_H, SCREEN_WIDTH, TOP_BAR_H, Theme.BorderDefault);

  Font faceLg = UIFonts_GetFace(S(18));
  Font faceMd = UIFonts_GetFace(S(13));
  Font faceSm = UIFonts_GetFace(S(10));
  Font faceXS = UIFonts_GetFace(S(8));
  Font iconMain = UIFonts_GetIcon(S(14));
  Font iconBrand = UIFonts_GetIconBrand(S(14));

  UIDrawText("ABOUT SYSTEM", UIFonts_GetFace(S(12)), S(15), S(7), S(12),
             Theme.TextPrimary);

  float centerX = SCREEN_WIDTH / 2.0f;
  float centerY = viewH / 2.0f;

  // Main Card Layout
  float cardW = S(380);
  float cardH = S(240);
  float cardX = centerX - (cardW / 2.0f);
  float cardY = centerY - (cardH / 2.0f) + (TOP_BAR_H / 2.0f);

  // Card Shadow & Background
  DrawRectangle(cardX + S(4), cardY + S(4), cardW, cardH,
                Fade(Theme.BgMain, 0.5f));
  DrawRectangle(cardX, cardY, cardW, cardH, Theme.BgMain);
  DrawRectangleLinesEx((Rectangle){cardX, cardY, cardW, cardH}, 1.0f,
                       Theme.TextSecondary);

  // Left Sidebar of the Card (Branding)
  float sideW = S(110);
  DrawRectangle(cardX + 1, cardY + 1, sideW, cardH - 2,
                Theme.BgMain);
  DrawLine(cardX + sideW, cardY + 1, cardX + sideW, cardY + cardH - 1,
           Theme.BgPanel);

  // Large Device Icon in Sidebar
  UIDrawText("\xef\x8a\x92", UIFonts_GetIcon(S(48)), cardX + sideW / 2 - S(24),
             cardY + S(40), S(48), Theme.BorderDefault);
  DrawCentredText("UNX DECK", faceXS, cardX, sideW, cardY + S(95), S(8),
                  Theme.TextSecondary);
  DrawCentredText("SYSTEM", faceXS, cardX, sideW, cardY + S(105), S(8),
                  Theme.TextSecondary);

  // Right Side Content
  float contentX = cardX + sideW + S(20);
  float startY = cardY + S(20);
  float rowH = S(38);

  // App Title & Version
  UIDrawText(APP_NAME, faceLg, contentX, startY, S(18), Theme.AccentOrange);
  char versionStr[128];
  sprintf(versionStr, "Version %s", r->State->Version);
  UIDrawText(versionStr, faceSm, contentX, startY + S(22), S(10),
             Theme.BorderDefault);

  float detailsY = startY + S(50);

  // --- Row 1: Developer ---
  UIDrawText("\xef\x80\x87", iconMain, contentX, detailsY + S(2), S(12),
             Theme.BorderDefault);
  UIDrawText("DEVELOPER", faceXS, contentX + S(18), detailsY - S(4), S(7),
             Theme.BorderDefault);
  UIDrawText(r->State->Developer, faceMd, contentX + S(18), detailsY + S(6),
             S(11), Theme.TextPrimary);

  // --- Row 2: Instagram ---
  UIDrawText("\xef\x85\xad", iconBrand, contentX, detailsY + rowH + S(2), S(12),
             Theme.AccentOrange);
  UIDrawText("INSTAGRAM", faceXS, contentX + S(18), detailsY + rowH - S(4),
             S(7), Theme.BorderDefault);
  UIDrawText(r->State->Instagram, faceMd, contentX + S(18),
             detailsY + rowH + S(6), S(11), Theme.AccentOrange);

  // --- Row 3: Platform ---
  UIDrawText("\xef\x90\xbc", iconMain, contentX, detailsY + rowH * 2 + S(2),
             S(12), Theme.BorderDefault);
  UIDrawText("PLATFORM", faceXS, contentX + S(18), detailsY + rowH * 2 - S(4),
             S(7), Theme.BorderDefault);
  UIDrawText(APP_PLATFORM, faceMd, contentX + S(18), detailsY + rowH * 2 + S(6),
             S(11), Theme.TextPrimary);

  // --- Row 4: Audio ---
  UIDrawText("\xef\x80\x81", iconMain, contentX, detailsY + rowH * 3 + S(2),
             S(12),
             Theme.BorderDefault); // Changed to music icon for better compatibility
  UIDrawText("AUDIO INTERFACE", faceXS, contentX + S(18),
             detailsY + rowH * 3 - S(4), S(7), Theme.BorderDefault);
  char audioBuf[128];
  snprintf(audioBuf, 128, "%s (%s)", r->State->AudioDevice,
           r->State->AudioDriver);
  UIDrawText(audioBuf, faceSm, contentX + S(18), detailsY + rowH * 3 + S(6),
             S(9), Theme.TextSecondary);

  // Footer Hint
  UIDrawText("The Sound of Nusantara, For Everyone.", faceXS, contentX,
             cardY + cardH - S(18), S(7), Theme.BorderDefault);
             
  // Draw Touch-Friendly CLOSE Button
  float btnW = S(90);
  float btnH = S(24);
  float btnX = cardX + cardW - btnW - S(20);
  float btnY = cardY + cardH - btnH - S(10);
  
  DrawRectangleRounded((Rectangle){btnX, btnY, btnW, btnH}, 0.2f, 4, Theme.BgPanel);
  DrawRectangleRoundedLines((Rectangle){btnX, btnY, btnW, btnH}, 0.2f, 4, 1.0f, Theme.TextSecondary);
  DrawCentredText("CLOSE", faceSm, btnX, btnW, btnY + (btnH - S(10)) / 2.0f, S(10), Theme.TextPrimary);
}

void AboutRenderer_Init(AboutRenderer *r, AboutState *state) {
  r->base.Update = About_Update;
  r->base.Draw = About_Draw;
  r->State = state;
}
