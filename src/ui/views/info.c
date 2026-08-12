#include "ui/views/info.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>
#include <string.h>
#include "input/input.h"

// Helper to truncate string (simple byte truncation for C)
static void truncateStr(const char *src, char *dst, int maxLen) {
  strncpy(dst, src, maxLen);
  dst[maxLen] = '\0';
  if (strlen(src) > (size_t)maxLen) {
    // Appending '...'
    if (maxLen > 3) {
      dst[maxLen - 3] = '.';
      dst[maxLen - 2] = '.';
      dst[maxLen - 1] = '.';
    }
  }
}

static int Info_Update(Component *base) {
  InfoRenderer *r = (InfoRenderer *)base;
  if (!r || !r->State || !r->State->IsActive) return 0;

  bool touchClose = false;
  if (Touch_CheckClickInArea((Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, 0)) {
      Vector2 mouse = Input_GetPointerPos();
      float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
      float halfH = availableH / 2.0f;
      float panelH = halfH - S(4);
      float panelW = SCREEN_WIDTH - S(16);
      float panelX = S(8);
      
      Rectangle card1 = {panelX, TOP_BAR_H + S(4), panelW, panelH};
      Rectangle card2 = {panelX, TOP_BAR_H + halfH + S(4), panelW, panelH};
      
      if (!CheckCollisionPointRec(mouse, card1) && !CheckCollisionPointRec(mouse, card2)) {
          touchClose = true;
      }
  }

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || touchClose) {
    r->State->IsActive = false;
    if (touchClose) {
        Input_Consume();
    }
  }
  return 0;
}

static void Info_Draw(Component *base) {
  InfoRenderer *r = (InfoRenderer *)base;
  if (!r->State->IsActive)
    return;

  // Background
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);

  Font faceXXS = UIFonts_GetFace(S(7));
  Font faceMd = UIFonts_GetFace(S(10));
  Font faceLg = UIFonts_GetFace(S(14));
  Font faceXL = UIFonts_GetFace(S(18));
  Font iconSm = UIFonts_GetIcon(S(10));
  Font iconLg = UIFonts_GetIcon(S(20));
  Font iconBrand = UIFonts_GetIconBrand(S(10));

  float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
  float halfH = availableH / 2.0f;
  float panelH = halfH - S(4);
  float panelW = SCREEN_WIDTH - S(16);
  float panelX = S(8);

  for (int i = 0; i < 2; i++) {
    float baseY = TOP_BAR_H + (i * halfH);
    float panelY = baseY + S(4);
    InfoTrack *trk = &r->State->Tracks[i];

    // Use global artwork from DeckState
    Texture2D *tex = (Texture2D *)trk->ArtworkTexture;
    bool hasArtwork = (tex && tex->id != 0);

    // Panel Background
    DrawRectangleRec((Rectangle){panelX, panelY, panelW, panelH}, ColorDark2);
    DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 1.0f,
                         ColorDark1);

    // Deck Indicator
    DrawRectangle(panelX, panelY, S(38), S(12),
                  i == 0 ? ColorOrange : ColorBlue);
    DrawCentredText(i == 0 ? "DECK 1" : "DECK 2", faceXXS, panelX, S(38),
                    panelY + S(2.5f), S(7), ColorBlack);

    float contentX = panelX + S(12);
    float artSize = S(70);
    float artY = panelY + S(22);

    // Draw Artwork
    if (hasArtwork) {
      Rectangle src = {0, 0, (float)tex->width, (float)tex->height};
      Rectangle dest = {contentX, artY, artSize, artSize};
      DrawTexturePro(*tex, src, dest, (Vector2){0, 0}, 0, ColorWhite);
      DrawRectangleLinesEx(dest, 1, ColorShadow);
    } else {
      DrawRectangle(contentX, artY, artSize, artSize, ColorDark3);
      DrawRectangleLinesEx((Rectangle){contentX, artY, artSize, artSize}, 1,
                           ColorShadow);
      DrawCentredText("\uf001", iconLg, contentX, artSize, artY + S(25), S(20),
                      ColorShadow);
    }

    // Title & Artist Area
    float infoX = contentX + artSize + S(15);
    float textY = panelY + S(18);

    static float infoTimer[2] = {0, 0};
    static char lastTrackTitle[2][256] = {{0}, {0}};
    if (strcmp(trk->Title, lastTrackTitle[i]) != 0) {
      strncpy(lastTrackTitle[i], trk->Title, sizeof(lastTrackTitle[i]) - 1);
      infoTimer[i] = 0.0f;
    }
    infoTimer[i] += GetFrameTime();

    if (trk->Title[0] == '\0') {
      UIDrawText("NO TRACK LOADED", faceXL, infoX, textY + S(8), S(18),
                 ColorShadow);
    } else {
      float titleW = panelW - (infoX - panelX) - S(10);
      Rectangle titleRect = { infoX, textY, titleW, S(20) };
      Rectangle artistRect = { infoX, textY + S(22), titleW, S(16) };

      UIDrawScrollingText(trk->Title, faceXL, titleRect, S(18), ColorWhite, infoTimer[i]);
      UIDrawScrollingText(trk->Artist[0] ? trk->Artist : "Unknown Artist", faceLg, artistRect, S(14), ColorOrange, infoTimer[i]);

      // Metadata Grid
      float gridY = panelY + S(64);
      float colW = (panelW - (infoX - panelX) - S(10)) / 3.0f;
      float rowStep = S(20);

      // Column 1
      float c1X = infoX;
      UIDrawText("\uf017", iconSm, c1X, gridY + S(2), S(10), ColorShadow);
      UIDrawText("BPM", faceXXS, c1X + S(15), gridY, S(7), ColorShadow);
      char bpmStr[16];
      sprintf(bpmStr, "%.1f", trk->BPM);
      UIDrawText(bpmStr, faceMd, c1X + S(15), gridY + S(8), S(10), ColorWhite);

      UIDrawText("\uf084", iconSm, c1X, gridY + rowStep + S(2), S(10),
                 ColorShadow);
      UIDrawText("KEY", faceXXS, c1X + S(15), gridY + rowStep, S(7),
                 ColorShadow);
      UIDrawText(trk->Key[0] ? trk->Key : "---", faceMd, c1X + S(15),
                 gridY + rowStep + S(8), S(10), ColorOrange);

      UIDrawText("\uf2f2", iconSm, c1X, gridY + rowStep * 2 + S(2), S(10),
                 ColorShadow);
      UIDrawText("DURATION", faceXXS, c1X + S(15), gridY + rowStep * 2, S(7),
                 ColorShadow);
      char durStr[16];
      sprintf(durStr, "%02d:%02d", trk->Duration / 60, trk->Duration % 60);
      UIDrawText(durStr, faceMd, c1X + S(15), gridY + rowStep * 2 + S(8), S(10),
                 ColorWhite);

      // Column 2
      float c2X = infoX + colW;
      UIDrawText("\uf51f", iconSm, c2X, gridY + S(2), S(10), ColorShadow);
      UIDrawText("ALBUM", faceXXS, c2X + S(15), gridY, S(7), ColorShadow);
      Rectangle albumRect = { c2X + S(15), gridY + S(8), colW - S(20), S(12) };
      UIDrawScrollingText(trk->Album[0] ? trk->Album : "---", faceMd, albumRect, S(10), ColorWhite, infoTimer[i]);

      UIDrawText("\uf133", iconSm, c2X, gridY + rowStep + S(2), S(10),
                 ColorShadow);
      UIDrawText("YEAR", faceXXS, c2X + S(15), gridY + rowStep, S(7),
                 ColorShadow);
      char yrStr[8];
      if (trk->Year > 0)
        sprintf(yrStr, "%d", trk->Year);
      else
        strcpy(yrStr, "---");
      UIDrawText(yrStr, faceMd, c2X + S(15), gridY + rowStep + S(8), S(10),
                 ColorWhite);

      UIDrawText("\uf02c", iconSm, c2X, gridY + rowStep * 2 + S(2), S(10),
                 ColorShadow);
      UIDrawText("GENRE", faceXXS, c2X + S(15), gridY + rowStep * 2, S(7),
                 ColorShadow);
      Rectangle genreRect = { c2X + S(15), gridY + rowStep * 2 + S(8), colW - S(20), S(12) };
      UIDrawScrollingText(trk->Genre[0] ? trk->Genre : "---", faceMd, genreRect, S(10), ColorWhite, infoTimer[i]);

      // Column 3
      float c3X = infoX + colW * 2;
      UIDrawText("\uf001", iconSm, c3X, gridY + S(2), S(10), ColorShadow);
      UIDrawText("LABEL", faceXXS, c3X + S(15), gridY, S(7), ColorShadow);
      Rectangle labelRect = { c3X + S(15), gridY + S(8), colW - S(20), S(12) };
      UIDrawScrollingText(trk->Label[0] ? trk->Label : "---", faceMd, labelRect, S(10), ColorWhite, infoTimer[i]);

      UIDrawText("\uf005", iconSm, c3X, gridY + rowStep + S(2), S(10),
                 ColorShadow);
      UIDrawText("RATING", faceXXS, c3X + S(15), gridY + rowStep, S(7),
                 ColorShadow);
      if (trk->Rating > 0) {
        for (int s = 0; s < 5; s++) {
          UIDrawText("\uf005", iconSm, c3X + S(15) + s * S(10),
                     gridY + rowStep + S(8), S(8),
                     (s < trk->Rating) ? ColorOrange : ColorShadow);
        }
      } else {
        UIDrawText("---", faceMd, c3X + S(15), gridY + rowStep + S(8), S(10),
                   ColorShadow);
      }

      UIDrawText("\uf287", iconBrand, c3X, gridY + rowStep * 2 + S(2), S(10),
                 ColorShadow);
      UIDrawText("SOURCE", faceXXS, c3X + S(15), gridY + rowStep * 2, S(7),
                 ColorShadow);
      UIDrawText(trk->Source[0] ? trk->Source : "---", faceMd, c3X + S(15),
                 gridY + rowStep * 2 + S(8), S(10), ColorBlue);

      // Row 4
      if (trk->Remixer[0]) {
        UIDrawText("\uf001", iconSm, c1X, gridY + rowStep * 3 + S(2), S(10), ColorShadow);
        UIDrawText("REMIXER", faceXXS, c1X + S(15), gridY + rowStep * 3, S(7), ColorShadow);
        Rectangle remixRect = { c1X + S(15), gridY + rowStep * 3 + S(8), colW - S(20), S(12) };
        UIDrawScrollingText(trk->Remixer, faceMd, remixRect, S(10), ColorWhite, infoTimer[i]);
      }
      if (trk->MixName[0]) {
        UIDrawText("\uf001", iconSm, c2X, gridY + rowStep * 3 + S(2), S(10), ColorShadow);
        UIDrawText("MIX", faceXXS, c2X + S(15), gridY + rowStep * 3, S(7), ColorShadow);
        Rectangle mixRect = { c2X + S(15), gridY + rowStep * 3 + S(8), colW - S(20), S(12) };
        UIDrawScrollingText(trk->MixName, faceMd, mixRect, S(10), ColorWhite, infoTimer[i]);
      }

      // Comment
      if (trk->Comment[0]) {
        float commY = panelY + panelH - S(15);
        DrawRectangle(infoX, commY, panelW - (infoX - panelX) - S(10), S(14),
                      ColorDark1);
        UIDrawText("COMMENT:", faceXXS, infoX + S(5), commY + S(3.5f), S(7),
                   ColorShadow);
        Rectangle commRect = { infoX + S(50), commY + S(3.5f), panelW - (infoX - panelX) - S(60), S(12) };
        UIDrawScrollingText(trk->Comment, faceXXS, commRect, S(7), ColorWhite, infoTimer[i]);
      }

      // Path
      if (trk->FilePath[0]) {
        char tPath[256];
        truncateStr(trk->FilePath, tPath, 150);
        float pathY = trk->Comment[0] ? (panelY + panelH - S(22)) : (panelY + panelH - S(8));
        UIDrawText(tPath, faceXXS, panelX + S(10), pathY, S(6),
                   Fade(ColorShadow, 0.4f));
      }
    }

    // DEBUG: Artwork Path (Always show if present)
    // if (trk->ArtworkPath[0]) {
    //   UIDrawText(trk->ArtworkPath, faceXXS, panelX + S(10), panelY + panelH -
    //   S(16), S(6), ColorRed);
    // }
  }
}

void InfoRenderer_Init(InfoRenderer *r, InfoState *state) {
  r->base.Update = Info_Update;
  r->base.Draw = Info_Draw;
  r->State = state;
}
