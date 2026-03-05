#include "ui/views/info.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>
#include <string.h>

// Helper to truncate string (simple byte truncation for C)
static void truncateStr(const char* src, char* dst, int maxLen) {
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
    if (!r->State->IsActive) return;

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);
    DrawLine(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2, ColorDark1);

    Font faceXXS = UIFonts_GetFace(7);
    Font faceXS  = UIFonts_GetFace(8);
    Font faceSm  = UIFonts_GetFace(9);
    Font faceLg  = UIFonts_GetFace(14);
    Font iconFace = UIFonts_GetIcon(14);

    for (int deckIdx = 0; deckIdx < 2; deckIdx++) {
        float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
        float halfH = availableH / 2.0f;
        float offsetY = deckIdx * halfH;
        float baseY = TOP_BAR_H + offsetY;

        InfoTrack *trk = &r->State->Tracks[deckIdx];

        char deckLab[32];
        sprintf(deckLab, "DECK %d", deckIdx + 1);
        UIDrawText(deckLab, faceXXS, 10, baseY + 4, 7, ColorRed);

        UIDrawText("\xef\x80\x81", iconFace, 10, baseY + 18, 14, ColorWhite); // f001 music note
        
        char tTitle[128]; truncateStr(trk->Title, tTitle, 80);
        UIDrawText(tTitle, faceLg, 30, baseY + 18, 14, ColorWhite);

        char tArtist[128]; truncateStr(trk->Artist, tArtist, 80);
        UIDrawText(tArtist, faceSm, 24, baseY + 36, 9, ColorShadow);

        DrawLine(8, baseY + 52, SCREEN_WIDTH - 8, baseY + 52, ColorDark1);

        // Rows
        struct { const char* l1; const char* v1; const char* l2; const char* v2; } rows[4];
        char tAlbum[128]; truncateStr(trk->Album, tAlbum, 40);
        char tGenre[64];  truncateStr(trk->Genre, tGenre, 25);
        rows[0].l1 = "ALBUM"; rows[0].v1 = tAlbum;
        rows[0].l2 = "GENRE"; rows[0].v2 = tGenre;

        char bpmStr[32]; sprintf(bpmStr, "%.1f", trk->BPM);
        rows[1].l1 = "BPM"; rows[1].v1 = bpmStr;
        rows[1].l2 = "KEY"; rows[1].v2 = trk->Key;

        char durStr[32]; sprintf(durStr, "%02d:%02d", trk->Duration/60, trk->Duration%60);
        char ratingStr[32] = "";
        for(int i=0; i<5; i++) {
            if(i < trk->Rating) strcat(ratingStr, "\xe2\x98\x85"); // solid star
            else strcat(ratingStr, "\xe2\x98\x86"); // outline star
        }
        rows[2].l1 = "DURATION"; rows[2].v1 = durStr;
        rows[2].l2 = "RATING"; rows[2].v2 = ratingStr;

        char tFile[256]; truncateStr(trk->FilePath, tFile, 45);
        rows[3].l1 = "SOURCE"; rows[3].v1 = trk->Source;
        rows[3].l2 = "FILE"; rows[3].v2 = tFile;

        float rowH = 22;
        float col1X = 10;
        float col2X = 360;

        for (int i = 0; i < 4; i++) {
            float ry = baseY + 58 + i * rowH;
            UIDrawText(rows[i].l1, faceXXS, col1X, ry, 7, ColorShadow);
            UIDrawText(rows[i].v1, faceXS, col1X + 54, ry, 8, ColorWhite);
            
            UIDrawText(rows[i].l2, faceXXS, col2X, ry, 7, ColorShadow);
            UIDrawText(rows[i].v2, faceXS, col2X + 50, ry, 8, ColorWhite);
        }

        // Comment
        float ry = baseY + 58 + 4 * rowH;
        UIDrawText("COMMENT", faceXXS, col1X, ry, 7, ColorShadow);
        char tComment[256]; truncateStr(trk->Comment, tComment, 80);
        UIDrawText(tComment, faceXS, col1X + 54, ry, 8, ColorWhite);
    }
}

void InfoRenderer_Init(InfoRenderer *r, InfoState *state) {
    r->base.Update = Info_Update;
    r->base.Draw = Info_Draw;
    r->State = state;
}
