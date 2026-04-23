#include "ui/views/info.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>
#include <string.h>

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
  (void)base;
  return 0; // nothing to do
}

static Texture2D deckArt[2] = {0};
static char lastArtPath[2][256] = {{0}, {0}};

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

    // Artwork Management
    if (strcmp(trk->ArtworkPath, lastArtPath[i]) != 0) {
      if (deckArt[i].id != 0)
        UnloadTexture(deckArt[i]);
      deckArt[i] = (Texture2D){0};

      if (trk->ArtworkPath[0] != '\0') {
        Image img = LoadImage(trk->ArtworkPath);
        if (img.data != NULL) {
          ImageResize(&img, (int)S(70), (int)S(70));
          deckArt[i] = LoadTextureFromImage(img);
          UnloadImage(img);
          SetTextureFilter(deckArt[i], TEXTURE_FILTER_BILINEAR);
        }
      }
      strncpy(lastArtPath[i], trk->ArtworkPath, 255);
    }

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
    if (deckArt[i].id != 0) {
      DrawTexture(deckArt[i], contentX, artY, ColorWhite);
      DrawRectangleLinesEx((Rectangle){contentX, artY, artSize, artSize}, 1,
                           ColorShadow);
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

    if (trk->Title[0] == '\0') {
      UIDrawText("NO TRACK LOADED", faceXL, infoX, textY + S(8), S(18),
                 ColorShadow);
    } else {
      char tTitle[128], tArtist[128];
      truncateStr(trk->Title, tTitle, 50);
      truncateStr(trk->Artist, tArtist, 60);

      UIDrawText(tTitle, faceXL, infoX, textY, S(18), ColorWhite);
      UIDrawText(tArtist, faceLg, infoX, textY + S(22), S(14), ColorOrange);

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
      char tAlbum[64];
      truncateStr(trk->Album, tAlbum, 30);
      UIDrawText(tAlbum[0] ? tAlbum : "---", faceMd, c2X + S(15), gridY + S(8),
                 S(10), ColorWhite);

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
      char tGenre[64];
      truncateStr(trk->Genre, tGenre, 30);
      UIDrawText(tGenre[0] ? tGenre : "---", faceMd, c2X + S(15),
                 gridY + rowStep * 2 + S(8), S(10), ColorWhite);

      // Column 3
      float c3X = infoX + colW * 2;
      UIDrawText("\uf001", iconSm, c3X, gridY + S(2), S(10), ColorShadow);
      UIDrawText("LABEL", faceXXS, c3X + S(15), gridY, S(7), ColorShadow);
      char tLabel[64];
      truncateStr(trk->Label, tLabel, 30);
      UIDrawText(tLabel[0] ? tLabel : "---", faceMd, c3X + S(15), gridY + S(8),
                 S(10), ColorWhite);

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

      // Comment
      if (trk->Comment[0]) {
        float commY = panelY + panelH - S(26);
        DrawRectangle(infoX, commY, panelW - (infoX - panelX) - S(10), S(14),
                      ColorDark1);
        UIDrawText("COMMENT:", faceXXS, infoX + S(5), commY + S(3.5f), S(7),
                   ColorShadow);
        char tComm[128];
        truncateStr(trk->Comment, tComm, 85);
        UIDrawText(tComm, faceXXS, infoX + S(50), commY + S(3.5f), S(7),
                   ColorWhite);
      }

      // Path
      if (trk->FilePath[0]) {
        char tPath[256];
        truncateStr(trk->FilePath, tPath, 150);
        UIDrawText(tPath, faceXXS, panelX + S(10), panelY + panelH - S(9), S(6),
                   Fade(ColorShadow, 0.4f));
      }
    }

    // DEBUG: Artwork Path (Always show if present)
    if (trk->ArtworkPath[0]) {
      UIDrawText(trk->ArtworkPath, faceXXS, panelX + S(10), panelY + panelH - S(16), S(6), ColorRed);
    }
  }
}

void InfoRenderer_Init(InfoRenderer *r, InfoState *state) {
  r->base.Update = Info_Update;
  r->base.Draw = Info_Draw;
  r->State = state;
}
