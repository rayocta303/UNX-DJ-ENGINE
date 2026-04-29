#include "ui/views/credits.h"
#include "raylib.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>

static int Credits_Update(Component *base) {
  CreditsRenderer *r = (CreditsRenderer *)base;
  if (!r->State->IsActive)
    return 0;

  // Handle Scrolling
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    r->State->Scroll -= wheel * S(20.0f);
  }

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    Vector2 delta = GetMouseDelta();
    r->State->Scroll -= delta.y;
  }

  // Boundaries check (Max scroll will be calculated in Draw, but we can cap it
  // roughly here)
  if (r->State->Scroll < 0)
    r->State->Scroll = 0;

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
    r->State->IsActive = false;
    r->State->Scroll = 0;
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
  DrawRectangle(0, startY, SCREEN_WIDTH, viewH, ColorBGUtil);

  float cardW = S(420);
  float cardH = viewH - S(20);
  float cardX = (SCREEN_WIDTH - cardW) / 2.0f;
  float cardY = startY + S(10);

  // Card BG (Matching About layout style)
  DrawRectangle(cardX, cardY, cardW, cardH, ColorDark1);
  DrawRectangleLinesEx((Rectangle){cardX, cardY, cardW, cardH}, 1.0f,
                       ColorGray);

  // Sidebar (Branding area)
  float sideW = S(100);
  DrawRectangle(cardX + 1, cardY + 1, sideW, cardH - 2,
                (Color){25, 25, 25, 255});
  DrawLine(cardX + sideW, cardY + 1, cardX + sideW, cardY + cardH - 1,
           ColorDark2);

  UIDrawText("\uf091", UIFonts_GetIcon(S(36)), cardX + sideW / 2 - S(18),
             cardY + S(30), S(36), ColorShadow);
  DrawCentredText("SUPPORTERS", UIFonts_GetFace(S(8)), cardX, sideW,
                  cardY + S(75), S(8), ColorGray);
  DrawCentredText("HALL OF FAME", UIFonts_GetFace(S(7)), cardX, sideW,
                  cardY + S(85), S(7), ColorShadow);

  // Right Side: Scrollable List
  float contentX = cardX + sideW + S(15);
  float contentW = cardW - sideW - S(30);
  float contentY = cardY + S(15);
  float contentH = cardH - S(30);

  Font faceSm = UIFonts_GetFace(S(10));
  Font faceXS = UIFonts_GetFace(S(8));

  BeginScissorMode((int)contentX, (int)contentY, (int)contentW, (int)contentH);

  float ly = contentY - r->State->Scroll;
  float rowH = S(16);

  // --- SECTION: GITHUB CONTRIBUTORS ---
  UIDrawText("GITHUB CONTRIBUTORS", faceXS, contentX, ly, S(8), ColorOrange);
  ly += S(14);
  UIDrawText("@rayocta303 - Hanif Bagus Saputra", faceSm, contentX + S(10), ly,
             S(10), ColorWhite);
  ly += rowH;
  UIDrawText("@miifanboy - Eren Erver", faceSm, contentX + S(10), ly, S(10),
             ColorWhite);
  ly += rowH + S(15);

  // --- SECTION: INSPIRATION & SUPPORT ---
  UIDrawText("INSPIRATION & SUPPORT", faceXS, contentX, ly, S(8), ColorOrange);
  ly += S(14);
  const char *supporters[] = {
      "@takeoutbox.dj",          "@alyxxcould",
      "@_tepann",                "@stephanievlna",
      "@djnozalavenza_official", "@diydjtech",
      "@djbossbomb",             "@evanjoris.music",
      "@dj_equipment_development"};
  int supCount = 9;
  for (int i = 0; i < supCount; i++) {
    UIDrawText(supporters[i], faceSm, contentX + S(10), ly, S(10), ColorWhite);
    ly += rowH;
  }
  ly += S(15);

  // --- SECTION: SPECIAL THANKS ---
  UIDrawText("SPECIAL THANKS", faceXS, contentX, ly, S(8), ColorBlue);
  ly += S(14);
  const char *thanks[] = {
      "Deep Symmetry", "Mixxx Community",
      "Rekordcrate Team",       "Raylib Open Source Community",
      "Miniaudio Backend Team", "SoundTouch Library Devs",
      "All beta testers"};
  int thanksCount = 7;
  for (int i = 0; i < thanksCount; i++) {
    UIDrawText(thanks[i], faceSm, contentX + S(10), ly, S(10), ColorWhite);
    ly += rowH;
  }

  EndScissorMode();

  // Cap Scroll
  float maxScroll = (ly - (contentY - r->State->Scroll)) - contentH;
  if (maxScroll < 0)
    maxScroll = 0;
  if (r->State->Scroll > maxScroll)
    r->State->Scroll = maxScroll;

  // Scrollbar
  if (maxScroll > 0) {
    float sbW = S(2);
    float sbX = cardX + cardW - sbW - S(2);
    float sbH = (contentH / (maxScroll + contentH)) * contentH;
    float sbY =
        contentY + (r->State->Scroll / (maxScroll + contentH)) * contentH;
    DrawRectangleRounded((Rectangle){sbX, sbY, sbW, sbH}, 1.0f, 4, ColorOrange);
  }

  // Footer Hint
  DrawCentredText("ESC/BACK to return", faceXS, cardX + sideW, cardW - sideW,
                  cardY + cardH - S(12), S(8), ColorShadow);
}

void CreditsRenderer_Init(CreditsRenderer *r, CreditsState *state) {
  r->base.Update = Credits_Update;
  r->base.Draw = Credits_Draw;
  r->State = state;
  r->State->Scroll = 0;
}
