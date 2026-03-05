#include "ui/player/bottomstrip.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>

static const char* FXNames[] = {"DELAY", "ECHO", "SPIRAL", "REVERB", "TRANS", "FLANGER", "PITCH", "ROLL"};
static const int FXNamesCount = 8;
static const char* XPadLabels[] = {"1/8", "1/4", "1/2", "3/4", "1", "2"};
static const int XPadLabelsCount = 6;

static int BottomStrip_Update(Component *base) {
    (void)base;
    return 0;
}

static void BottomStrip_Draw(Component *base) {
    BeatFXSelectBar *b = (BeatFXSelectBar *)base;
    float barY = TOP_BAR_H + WAVE_AREA_H;
    float barH = FX_BAR_H;

    DrawRectangle(0, barY, SCREEN_WIDTH, barH, (Color){0x18, 0x18, 0x18, 0xFF});
    DrawLine(0, barY, SCREEN_WIDTH, barY, ColorDark1);
    DrawLine(0, barY + barH, SCREEN_WIDTH, barY + barH, ColorDark1);

    float halfW = SCREEN_WIDTH / 2.0f;
    DrawLine(halfW, barY + S(1), halfW, barY + barH - S(1), ColorDark1);

    Font faceXXS = UIFonts_GetFace(S(7));
    Font faceSm = UIFonts_GetFace(S(9));

    if (!b->State->ShowBeatFXTab) {
        // --- STATUS MODE: Hot Cue Grid ---
        DeckState *decks[2] = { b->DeckA, b->DeckB };
        float deckW = halfW;
        
        for (int d = 0; d < 2; d++) {
            DeckState *ds = decks[d];
            float dx = d * deckW;
            
            DrawCentredText(d == 0 ? "DECK 1 HOT CUE" : "DECK 2 HOT CUE", faceXXS, dx, deckW, barY + S(2.5f), S(7), ColorShadow);
            
            float gridY = barY + S(11);
            float cellW = (deckW - S(8)) / 4.0f;
            float cellH = (barH - S(14)) / 2.0f;
            
            for (int i = 0; i < 8; i++) {
                int row = i / 4;
                int col = i % 4;
                float cx = dx + S(4) + col * cellW;
                float cy = gridY + row * cellH;
                
                Rectangle cellRect = { cx + S(1), cy + S(1), cellW - S(2), cellH - S(2) };
                
                // Hot Cue logic from player_state.h: HotCue contains Start (ms) and ID
                bool hasCue = false;
                char timeStr[16] = "";
                Color cueColor = ColorDark1;
                
                if (ds && ds->LoadedTrack) {
                    for (int h = 0; h < ds->LoadedTrack->HotCuesCount; h++) {
                        if (ds->LoadedTrack->HotCues[h].ID == (unsigned int)(i + 1)) {
                            unsigned int ms = ds->LoadedTrack->HotCues[h].Start;
                            int totalSec = ms / 1000;
                            int min = totalSec / 60;
                            int sec = totalSec % 60;
                            int frm = (ms % 1000) / 10;
                            sprintf(timeStr, "%02d:%02d.%02d", min, sec, frm);
                            hasCue = true;
                            cueColor = (d == 0) ? ColorOrange : ColorBlue;
                            break;
                        }
                    }
                }
                
                DrawRectangleRec(cellRect, hasCue ? (Color){cueColor.r/2, cueColor.g/2, cueColor.b/2, 255} : (Color){20, 20, 20, 255});
                DrawRectangleLinesEx(cellRect, 1, hasCue ? cueColor : ColorGray);
                
                char idStr[4];
                sprintf(idStr, "%c", 'A' + i);
                UIDrawText(idStr, faceXXS, cx + S(3), cy + S(3), S(7), hasCue ? ColorWhite : ColorShadow);
                
                if (hasCue) {
                    UIDrawText(timeStr, faceXXS, cx + S(12), cy + S(3.5f), S(7), ColorWhite);
                }
            }
        }
    } else {
        // --- BEAT FX MODE: Existing UI ---
        DrawCentredText("BEAT FX SELECT", faceXXS, 0, halfW, barY + S(2.5f), S(7), ColorShadow);
        DrawCentredText("X-PAD", faceXXS, halfW, halfW, barY + S(2.5f), S(7), ColorShadow);

        float btnY = barY + S(13);
        float btnH = S(23);

        float trashW = S(22);
        float gap = S(2);
        float fxBtnW = (halfW - trashW - S(4) - (FXNamesCount - 1) * gap) / FXNamesCount;
        float cx = S(2);

        for (int i = 0; i < FXNamesCount; i++) {
            bool active = (i == b->State->SelectedFX);
            Color bg = {0x22, 0x22, 0x22, 0xFF};
            Color border = ColorDark1;
            Color txtClr = ColorPaper;
            if (active) {
                bg = (Color){0x44, 0x44, 0x44, 0xFF};
                border = ColorPaper;
                txtClr = ColorWhite;
            }
            DrawRectangle(cx, btnY, fxBtnW, btnH, bg);
            DrawRectangleLines(cx, btnY, fxBtnW, btnH, border);
            DrawCentredText(FXNames[i], faceSm, cx, fxBtnW, btnY + S(6.5f), S(8), txtClr);
            cx += fxBtnW + gap;
        }

        DrawRectangle(cx, btnY, trashW - S(2), btnH, (Color){0x22, 0x22, 0x22, 0xFF});
        DrawRectangleLines(cx, btnY, trashW - S(2), btnH, ColorDark1);
        DrawCentredText("Clear", faceXXS, cx, trashW - S(2), btnY + S(6.5f), S(7), ColorShadow);

        cx = halfW + S(2);
        float padAreaW = halfW - S(4);
        float padBtnW = padAreaW / XPadLabelsCount;

        for (int i = 0; i < XPadLabelsCount; i++) {
            bool active = (i == b->State->SelectedPad);
            Color bg = {0x22, 0x22, 0x22, 0xFF};
            Color border = ColorDark1;
            Color txtClr = ColorPaper;
            if (active) {
                bg = (Color){0x60, 0x60, 0x60, 0xFF};
                border = ColorPaper;
                txtClr = ColorWhite;
            }
            DrawRectangle(cx, btnY, padBtnW - 1, btnH, bg);
            DrawRectangleLines(cx, btnY, padBtnW - 1, btnH, border);
            DrawCentredText(XPadLabels[i], faceSm, cx, padBtnW - 1, btnY + S(6.5f), S(8), txtClr);
            cx += padBtnW;
        }
    }
}

void BeatFXSelectBar_Init(BeatFXSelectBar *b, BeatFXState *state, DeckState *deckA, DeckState *deckB) {
    b->base.Update = BottomStrip_Update;
    b->base.Draw = BottomStrip_Draw;
    b->State = state;
    b->DeckA = deckA;
    b->DeckB = deckB;
}
