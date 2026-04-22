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

static void Info_Draw(Component *base) {
  InfoRenderer *r = (InfoRenderer *)base;
  if (!r->State->IsActive)
    return;

  // Background shading
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);

  Font faceXXS = UIFonts_GetFace(S(7));
  Font faceSm = UIFonts_GetFace(S(9));
  Font faceMd = UIFonts_GetFace(S(11));
  Font faceLg = UIFonts_GetFace(S(15));
  Font iconFace = UIFonts_GetIcon(S(12));
  Font iconMd = UIFonts_GetIcon(S(20));

  float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H - FX_BAR_H;
  float halfH = availableH / 2.0f;
  float centerX = SCREEN_WIDTH / 2.0f;

  for (int deckIdx = 0; deckIdx < 2; deckIdx++) {
    float offsetY = deckIdx * halfH;
    float baseY = TOP_BAR_H + offsetY;
    float panelH = halfH - S(6);
    float panelW = SCREEN_WIDTH - S(20);
    float panelX = S(10);
    float panelY = baseY + S(3);

    // Draw Deck Panel Card - Darker, cleaner borders
    DrawRectangle(panelX, panelY, panelW, panelH, (Color){22, 22, 25, 255});
    DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 1.0f,
                         (Color){50, 50, 60, 255});

    // Deck Badge
    float badgeW = S(40);
    DrawRectangle(panelX, panelY, badgeW, S(16), (Color){40, 40, 50, 255});
    char deckLab[32];
    sprintf(deckLab, "DECK %d", deckIdx + 1);
    UIDrawText(deckLab, faceXXS, panelX + S(6), panelY + S(4.5f), S(7),
               ColorOrange);

    InfoTrack *trk = &r->State->Tracks[deckIdx];

    // Main Header Area (Title & Artist)
    float headX = panelX + S(75);
    float headY = panelY + S(12);

    // Large Music Icon - Slightly translucent
    UIDrawText("\xef\x80\x81", iconMd, panelX + S(25), headY + S(2), S(22),
               Fade(ColorWhite, 0.5f));

    // Title
    char tTitle[128];
    if (trk->Title[0] == '\0')
      strcpy(tTitle, "Waiting for track...");
    else
      truncateStr(trk->Title, tTitle, 60);
    UIDrawText(tTitle, faceLg, headX, headY, S(15), ColorWhite);

    // Artist
    char tArtist[128];
    if (trk->Artist[0] == '\0')
      strcpy(tArtist, "---");
    else
      truncateStr(trk->Artist, tArtist, 75);
    UIDrawText(tArtist, faceMd, headX, headY + S(20), S(11), ColorShadow);

    // Metadata Grid - Centered in remaining space
    float col1IconX = panelX + S(15);
    float col1TextX = panelX + S(35);
    float col2IconX = centerX + S(5);
    float col2TextX = centerX + S(25);

    // Adjust Y start based on panel height to center it better
    float startGridY = panelY + S(54);
    float rowH = S(21);

    // Row 1: BPM & KEY
    char bpmStr[32];
    sprintf(bpmStr, "%.1f BPM", trk->BPM);
    UIDrawText("\xef\x80\x97", iconFace, col1IconX, startGridY + S(8), S(12),
               ColorShadow); // Clock
    UIDrawText("BPM", faceXXS, col1TextX, startGridY, S(7), ColorShadow);
    UIDrawText(bpmStr, faceSm, col1TextX, startGridY + S(9), S(9), ColorWhite);

    UIDrawText("\xef\x82\x84", iconFace, col2IconX, startGridY + S(8), S(12),
               ColorShadow); // Key
    UIDrawText("KEY", faceXXS, col2TextX, startGridY, S(7), ColorShadow);
    UIDrawText(trk->Key[0] ? trk->Key : "---", faceSm, col2TextX,
               startGridY + S(9), S(9), ColorOrange);

    // Row 2: ALBUM & YEAR
    char tAlbum[128];
    if (trk->Album[0] == '\0')
      strcpy(tAlbum, "---");
    else
      truncateStr(trk->Album, tAlbum, 40);
    UIDrawText("\xef\x94\x9f", iconFace, col1IconX, startGridY + rowH + S(8),
               S(12), ColorShadow); // Compact Disc (CD) Icon \uf51f
    UIDrawText("ALBUM", faceXXS, col1TextX, startGridY + rowH, S(7),
               ColorShadow);
    UIDrawText(tAlbum, faceSm, col1TextX, startGridY + rowH + S(9), S(9),
               ColorWhite);

    char yearStr[16];
    if (trk->Year > 0) sprintf(yearStr, "%d", trk->Year);
    else strcpy(yearStr, "---");
    UIDrawText("\xef\x84\xb3", iconFace, col2IconX, startGridY + rowH + S(8),
               S(12), ColorShadow); // Calendar Icon \uf133
    UIDrawText("YEAR", faceXXS, col2TextX, startGridY + rowH, S(7),
               ColorShadow);
    UIDrawText(yearStr, faceSm, col2TextX, startGridY + rowH + S(9), S(9),
               ColorWhite);

    // Row 3: GENRE & LABEL
    char tGenre[64];
    if (trk->Genre[0] == '\0')
      strcpy(tGenre, "---");
    else
      truncateStr(trk->Genre, tGenre, 30);
    UIDrawText("\xef\x80\xac", iconFace, col1IconX,
               startGridY + rowH * 2 + S(8), S(12), ColorShadow); // Tag
    UIDrawText("GENRE", faceXXS, col1TextX, startGridY + rowH * 2, S(7),
               ColorShadow);
    UIDrawText(tGenre, faceSm, col1TextX, startGridY + rowH * 2 + S(9), S(9),
               ColorWhite);

    char tLabel[128];
    if (trk->Label[0] == '\0')
      strcpy(tLabel, "---");
    else
      truncateStr(trk->Label, tLabel, 30);
    UIDrawText("\xef\x80\x81", iconFace, col2IconX,
               startGridY + rowH * 2 + S(8), S(12), ColorShadow); // Music \uf001 for Label
    UIDrawText("LABEL", faceXXS, col2TextX, startGridY + rowH * 2, S(7),
               ColorShadow);
    UIDrawText(tLabel, faceSm, col2TextX, startGridY + rowH * 2 + S(9), S(9),
               ColorWhite);

    // Row 4: RATING & DURATION
    UIDrawText("\xef\x80\x85", iconFace, col1IconX,
               startGridY + rowH * 3 + S(8), S(12), ColorShadow); // Star
    UIDrawText("RATING", faceXXS, col1TextX, startGridY + rowH * 3, S(7),
               ColorShadow);

    if (trk->Rating > 0) {
      for (int i = 0; i < 5; i++) {
        const char *starIco = (i < trk->Rating) ? "\xef\x80\x85" : "";
        if (starIco[0]) {
          UIDrawText(starIco, iconFace, col1TextX + i * S(10),
                     startGridY + rowH * 3 + S(9), S(9), ColorOrange);
        } else {
          UIDrawText(".", faceXXS, col1TextX + i * S(10) + S(4),
                     startGridY + rowH * 3 + S(9), S(7), ColorShadow);
        }
      }
    } else {
      UIDrawText("---", faceSm, col1TextX, startGridY + rowH * 3 + S(9),
                 S(9), ColorShadow);
    }

    char durStr[32];
    sprintf(durStr, "%02d:%02d", trk->Duration / 60, trk->Duration % 60);
    UIDrawText("\xef\x80\x97", iconFace, col2IconX, startGridY + rowH * 3 + S(8),
               S(12), ColorShadow); // Clock
    UIDrawText("DURATION", faceXXS, col2TextX, startGridY + rowH * 3, S(7),
               ColorShadow);
    UIDrawText(durStr, faceSm, col2TextX, startGridY + rowH * 3 + S(9), S(9),
               ColorWhite);

    // Comment Area
    if (trk->Comment[0]) {
      char tComment[256];
      truncateStr(trk->Comment, tComment, 80);
      UIDrawText("COMMENT:", faceXXS, panelX + S(10), panelY + panelH - S(22),
                 S(7), ColorShadow);
      UIDrawText(tComment, faceXXS, panelX + S(55), panelY + panelH - S(22),
                 S(7), Fade(ColorWhite, 0.6f));
    }

    // Small source info at absolute bottom of card
    if (trk->FilePath[0]) {
      char sourceInfo[256];
      sprintf(sourceInfo, "[%s] %s", trk->Source, trk->FilePath);
      char tSource[256];
      truncateStr(sourceInfo, tSource, 110);
      UIDrawText(tSource, faceXXS, panelX + S(10), panelY + panelH - S(10),
                 S(7), Fade(ColorShadow, 0.4f));
    }
  }
}

void InfoRenderer_Init(InfoRenderer *r, InfoState *state) {
  r->base.Update = Info_Update;
  r->base.Draw = Info_Draw;
  r->State = state;
}
