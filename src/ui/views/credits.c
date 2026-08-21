#include "ui/views/credits.h"
#include "raylib.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "input/input.h"
#include <stdio.h>
#include <math.h>

static int Credits_Update(Component *base) {
  CreditsRenderer *r = (CreditsRenderer *)base;
  if (!r->State->IsActive)
    return 0;

  // View bounds for max scroll calculation (approximate)
  float viewH = SCREEN_HEIGHT - DECK_STR_H - TOP_BAR_H;
  float cardH = viewH - S(20);
  float contentH = cardH - S(30);
  // Estimate content height based on number of rows
  float totalContentH = S(14)*3 + S(16)*2 + S(15) + S(14) + S(16)*9 + S(15) + S(14) + S(16)*7;
  float maxScroll = totalContentH - contentH;
  if (maxScroll < 0) maxScroll = 0;

  // Touch drag integration
  Vector2 mouse = Input_GetPointerPos();
  if (Input_IsPressed()) {
      r->State->ScrollPhysics.DragStartPos = mouse.y;
      r->State->ScrollPhysics.DragStartScroll = r->State->ScrollPhysics.Scroll;
      r->State->ScrollPhysics.IsDragging = false;
  }
  
  if (Input_IsDown()) {
      float dy = mouse.y - r->State->ScrollPhysics.DragStartPos;
      if (!r->State->ScrollPhysics.IsDragging && fabsf(dy) > S(4.0f)) {
          r->State->ScrollPhysics.IsDragging = true;
          r->State->ScrollPhysics.DragStartPos = mouse.y;
          r->State->ScrollPhysics.DragStartScroll = r->State->ScrollPhysics.Scroll;
      }
      
      if (r->State->ScrollPhysics.IsDragging) {
          float newDy = mouse.y - r->State->ScrollPhysics.DragStartPos;
          r->State->ScrollPhysics.Scroll = r->State->ScrollPhysics.DragStartScroll - newDy;
          r->State->ScrollPhysics.Velocity = -Mouse_GetDelta().y / (GetFrameTime() > 0 ? GetFrameTime() : 0.016f);
      }
  } else if (Input_IsReleased()) {
      r->State->ScrollPhysics.IsDragging = false;
  }

  // Handle Wheel
  float wheel = Mouse_GetWheel();
  if (wheel != 0) {
    r->State->ScrollPhysics.Scroll -= wheel * S(20.0f);
    r->State->ScrollPhysics.Velocity = 0;
  }

  TouchScroll_Update(&r->State->ScrollPhysics, maxScroll, GetFrameTime());
  r->State->Scroll = r->State->ScrollPhysics.Scroll;

  // Allow closing by tapping anywhere outside the card or the back button
  float cardW = S(420);
  float cardX = (SCREEN_WIDTH - cardW) / 2.0f;
  float startY = TOP_BAR_H;
  float cardY = startY + S(10);
  Rectangle cardRect = {cardX, cardY, cardW, cardH};
  
  bool closeClicked = false;
  if (Touch_CheckClickInArea((Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, 0) && !CheckCollisionPointRec(mouse, cardRect)) {
      closeClicked = true;
      Input_Consume();
  }
  
  float btnW = S(120);
  float btnH = S(30);
  float btnX = cardX + (cardW - btnW) / 2.0f;
  float btnY = cardY + cardH - btnH - S(10);
  if (Touch_CheckClick((Rectangle){btnX, btnY, btnW, btnH}, S(5))) {
      closeClicked = true;
      Input_Consume();
  }

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || closeClicked) {
    r->State->IsActive = false;
    TouchScroll_Reset(&r->State->ScrollPhysics);
    r->State->Scroll = 0;
    Input_Consume();
  }

  return 0;
}

static void Credits_Draw(Component *base) {
  CreditsRenderer *r = (CreditsRenderer *)base;
  if (!r->State->IsActive)
    return;

  float viewH = SCREEN_HEIGHT - DECK_STR_H - TOP_BAR_H;
  float startY = TOP_BAR_H;

  // Dark background for the whole view area
  DrawRectangle(0, startY, SCREEN_WIDTH, viewH, Theme.BgMain);

  float cardW = S(420);
  float cardH = viewH - S(20);
  float cardX = (SCREEN_WIDTH - cardW) / 2.0f;
  float cardY = startY + S(10);

  // Card BG (Matching About layout style)
  DrawRectangle(cardX, cardY, cardW, cardH, Theme.BgMain);
  DrawRectangleLinesEx((Rectangle){cardX, cardY, cardW, cardH}, 1.0f,
                       Theme.TextSecondary);

  // Sidebar (Branding area)
  float sideW = S(100);
  DrawRectangle(cardX + 1, cardY + 1, sideW, cardH - 2,
                Theme.BgMain);
  DrawLine(cardX + sideW, cardY + 1, cardX + sideW, cardY + cardH - 1,
           Theme.BgPanel);

  UIDrawText("\uf091", UIFonts_GetIcon(S(36)), cardX + sideW / 2 - S(18),
             cardY + S(30), S(36), Theme.BorderDefault);
  DrawCentredText("SUPPORTERS", UIFonts_GetFace(S(8)), cardX, sideW,
                  cardY + S(75), S(8), Theme.TextSecondary);
  DrawCentredText("HALL OF FAME", UIFonts_GetFace(S(7)), cardX, sideW,
                  cardY + S(85), S(7), Theme.BorderDefault);

  // Right Side: Scrollable List
  float contentX = cardX + sideW + S(15);
  float contentW = cardW - sideW - S(30);
  float contentY = cardY + S(15);
  float contentH = cardH - S(30);

  Font faceSm = UIFonts_GetFace(S(10));
  Font faceXS = UIFonts_GetFace(S(8));

  BeginScissorMode((int)contentX, (int)contentY, (int)contentW, (int)contentH);

  float ly = contentY - r->State->ScrollPhysics.VisualScroll;
  float rowH = S(16);

  // --- SECTION: GITHUB CONTRIBUTORS ---
  UIDrawText("GITHUB CONTRIBUTORS", faceXS, contentX, ly, S(8), Theme.AccentOrange);
  ly += S(14);
  UIDrawText("@rayocta303 - Hanif Bagus Saputra", faceSm, contentX + S(10), ly,
             S(10), Theme.TextPrimary);
  ly += rowH;
  UIDrawText("@miifanboy - Eren Erver", faceSm, contentX + S(10), ly, S(10),
             Theme.TextPrimary);
  ly += rowH + S(15);

  // --- SECTION: INSPIRATION & SUPPORT ---
  UIDrawText("INSPIRATION & SUPPORT", faceXS, contentX, ly, S(8), Theme.AccentOrange);
  ly += S(14);
  const char *supporters[] = {
      "@takeoutbox.dj",          "@alyxxcould",
      "@_tepann",                "@stephanievlna",
      "@djnozalavenza_official", "@diydjtech",
      "@djbossbomb",             "@evanjoris.music",
      "@dj_equipment_development"};
  int supCount = 9;
  for (int i = 0; i < supCount; i++) {
    UIDrawText(supporters[i], faceSm, contentX + S(10), ly, S(10), Theme.TextPrimary);
    ly += rowH;
  }
  ly += S(15);

  // --- SECTION: SPECIAL THANKS ---
  UIDrawText("SPECIAL THANKS", faceXS, contentX, ly, S(8), Theme.AccentBlue);
  ly += S(14);
  const char *thanks[] = {
      "Deep Symmetry", "Mixxx Community",
      "Rekordcrate Team",       "Raylib Open Source Community",
      "Miniaudio Backend Team", "SoundTouch Library Devs",
      "All beta testers"};
  int thanksCount = 7;
  for (int i = 0; i < thanksCount; i++) {
    UIDrawText(thanks[i], faceSm, contentX + S(10), ly, S(10), Theme.TextPrimary);
    ly += rowH;
  }

  EndScissorMode();

  // Cap Scroll
  float maxScroll = (ly - (contentY - r->State->ScrollPhysics.VisualScroll)) - contentH;
  if (maxScroll < 0)
    maxScroll = 0;

  // Scrollbar
  if (maxScroll > 0) {
    float sbW = S(2);
    float sbX = cardX + cardW - sbW - S(2);
    float sbH = (contentH / (maxScroll + contentH)) * contentH;
    float sbY =
        contentY + (r->State->ScrollPhysics.VisualScroll / maxScroll) * (contentH - sbH);
    DrawRectangleRounded((Rectangle){sbX, sbY, sbW, sbH}, 1.0f, 4, Theme.AccentOrange);
  }

  // Draw Touch-Friendly CLOSE Button
  float btnW = S(120);
  float btnH = S(30);
  float btnX = cardX + (cardW - btnW) / 2.0f;
  float btnY = cardY + cardH - btnH - S(10);
  
  DrawRectangleRounded((Rectangle){btnX, btnY, btnW, btnH}, 0.2f, 4, Theme.BgPanel);
  DrawRectangleRoundedLines((Rectangle){btnX, btnY, btnW, btnH}, 0.2f, 4, 1.5f, Theme.TextSecondary);
  DrawCentredText("CLOSE", faceSm, btnX, btnW, btnY + (btnH - S(10)) / 2.0f, S(10), Theme.TextPrimary);
}

void CreditsRenderer_Init(CreditsRenderer *r, CreditsState *state) {
  r->base.Update = Credits_Update;
  r->base.Draw = Credits_Draw;
  r->State = state;
  r->State->Scroll = 0;
  TouchScroll_Init(&r->State->ScrollPhysics);
}
