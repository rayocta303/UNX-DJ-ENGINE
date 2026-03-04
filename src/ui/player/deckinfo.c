#include "ui/player/deckinfo.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>

static int DeckInfo_Update(Component *base) {
    (void)base;
    return 0;
}

static void DeckInfo_Draw(Component *base) {
    DeckInfoPanel *d = (DeckInfoPanel *)base;
    
    float deckInfoW = SIDE_PANEL_W;
    float deckInfoH = (SCREEN_HEIGHT - TOP_BAR_H - FX_BAR_H - DECK_STR_H) / 2.0f;
    float y = TOP_BAR_H + (d->ID * deckInfoH);
    float margin = 10.0f;

    DrawRectangle(0, y, deckInfoW, deckInfoH, ColorDark2);
    
    float headerH = 12.0f;
    DrawRectangle(0, y, deckInfoW, S(headerH), ColorShadow);
    DrawRectangle(0, y + S(headerH) - S(0.5f), deckInfoW, S(1.0f), ColorBlue);

    DrawLine(deckInfoW - 1, y, deckInfoW - 1, y + deckInfoH, ColorDark1);
    DrawLine(0, y + deckInfoH - 1, deckInfoW, y + deckInfoH - 1, ColorDark1);

    Font faceXXS = UIFonts_GetFace(S(7.5f));
    Font faceXS = UIFonts_GetFace(S(8.5f));
    Font faceSm = UIFonts_GetFace(S(10));
    Font iconBrand = UIFonts_GetIconBrand(S(10));

    char deckLabel[32];
    sprintf(deckLabel, "DECK %d", d->ID + 1);
    UIDrawText(deckLabel, faceXXS, S(margin), y + S(1.5f), S(7.5f), ColorWhite);

    float contentY = y + S(headerH);
    float contentH = deckInfoH - S(headerH);
    float rowH = contentH / 4.0f;

    for (int i = 1; i < 4; i++) {
        float dividerY = contentY + i * rowH;
        DrawLine(0, dividerY, deckInfoW, dividerY, (Color){0x22, 0x22, 0x22, 0xFF});
    }

    // 1. Source Row
    float sourceY = contentY + (rowH - S(10)) / 2.0f;
    UIDrawText("\xef\xaa\x87", iconBrand, S(margin), sourceY, S(10), ColorWhite); // f287 usb
    UIDrawText(d->State->SourceName, faceXS, S(margin + 16), sourceY - S(1), S(8.5f), ColorWhite);

    // 2. Key Row
    float keyY = contentY + rowH + (rowH - S(10)) / 2.0f;
    DrawRectangleLines(S(margin), keyY, S(14), S(10), ColorShadow);
    UIDrawText("\xe2\x99\xad#", faceXXS, S(margin + 2), keyY + S(1), S(7.5f), ColorShadow); // flat sharp
    
    const char* keyStr = "---";
    if (d->State->LoadedTrack != NULL) {
        keyStr = d->State->TrackKey;
    }
    UIDrawText(keyStr, faceXS, S(margin + 20), keyY, S(8.5f), ColorWhite);

    // 3. Bars Row
    float barsY = contentY + rowH * 2.0f + (rowH - S(11)) / 2.0f;
    Color barsColor = ColorWhite;
    char barsVal[32] = "---.-";

    if (d->State->LoadedTrack != NULL) {
        unsigned int posMs = (unsigned int)d->State->PositionMs;
        int beatIdx = -1;

        for (int i = 0; i < 1024; i++) {
            unsigned int bMs = d->State->LoadedTrack->BeatGrid[i];
            if (bMs == 0 || bMs == 0xFFFFFFFF) break;
            if (bMs <= posMs) beatIdx = i;
            else break;
        }

        if (beatIdx >= 0) {
            int downbeatTarget = (1 - (int)d->State->LoadedTrack->GridOffset) & 3;
            int offsetIdx = beatIdx - downbeatTarget;
            
            int currentBar, currentBeat;
            if (offsetIdx >= 0) {
                currentBar = (offsetIdx / 4) + 1;
                currentBeat = (offsetIdx % 4) + 1;
            } else {
                currentBar = 0;
                currentBeat = 4 + offsetIdx + 1;
            }
            sprintf(barsVal, "%02d.%d", currentBar, currentBeat);
        } else {
            sprintf(barsVal, "01.1");
        }
    }

    if (d->ID == 0) barsColor = ColorOrange;

    UIDrawText(barsVal, faceSm, S(margin), barsY, S(10), barsColor);
    UIDrawText("Bars", faceXXS, S(margin + 32), barsY + S(2.5f), S(7.5f), barsColor);
}

void DeckInfoPanel_Init(DeckInfoPanel *p, int id, DeckState *state) {
    p->base.Update = DeckInfo_Update;
    p->base.Draw = DeckInfo_Draw;
    p->ID = id;
    p->State = state;
}
