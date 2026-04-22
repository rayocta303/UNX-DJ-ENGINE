#include "ui/player/bottomstrip.h"
#include <stddef.h>
#include <stdio.h>
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"

static int BottomStrip_Update(Component *base) {
    BeatFXSelectBar *b = (BeatFXSelectBar *)base;
    Vector2 mouse = UIGetMousePosition();
    float barY = TOP_BAR_H + WAVE_AREA_H;
    float halfW = SCREEN_WIDTH / 2.0f;

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (mouse.y >= barY && mouse.y <= barY + S(12)) {
            b->State->ShowBeatFXTab = !b->State->ShowBeatFXTab;
            return 1;
        }

        if (b->State->ShowBeatFXTab) {
            float btnY = barY + S(13);
            float btnH = S(23);
            
            // FX Selection
            float trashW = S(22);
            float gap = S(2);
            static const int FXNamesCount = 3;
            float fxBtnW = (halfW - trashW - S(4) - (FXNamesCount - 1) * gap) / FXNamesCount;
            
            float cx = S(2);
            for (int i = 0; i < FXNamesCount; i++) {
                Rectangle r = { cx, btnY, fxBtnW, btnH };
                if (CheckCollisionPointRec(mouse, r)) {
                    static const int FXEnumMap[] = { 0, 1, 2 }; // DELAY, ECHO, REVERB
                    b->State->SelectedFX = FXEnumMap[i];
                    return 1;
                }
                cx += fxBtnW + gap;
            }

            // Clear/Toggle FX
            Rectangle trashRect = { cx, btnY, trashW - S(2), btnH };
            if (CheckCollisionPointRec(mouse, trashRect)) {
                b->State->IsFXOn = !b->State->IsFXOn;
                return 1;
            }

            // X-Pad Interaction
            cx = halfW + S(2);
            float padAreaW = halfW - S(4);
            bool isScrubMode = (b->State->SelectedFX == 3 || b->State->SelectedFX == 5);
            if (!isScrubMode) {
                static const int XPadLabelsCount = 6;
                float padBtnW = padAreaW / XPadLabelsCount;
                for (int i = 0; i < XPadLabelsCount; i++) {
                    Rectangle padRect = { cx, btnY, padBtnW - 1, btnH };
                    if (CheckCollisionPointRec(mouse, padRect)) {
                        b->State->SelectedPad = i;
                        return 1;
                    }
                    cx += padBtnW;
                }
            }
        }
    }

    if (b->State->ShowBeatFXTab && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float btnY = barY + S(13);
        float btnH = S(23);
        float cx = halfW + S(2);
        float padAreaW = halfW - S(4);
        Rectangle xpadRect = { cx, btnY, padAreaW, btnH };
        if (CheckCollisionPointRec(mouse, xpadRect)) {
            b->State->XPadScrubValue = ((mouse.x - cx) / padAreaW) * 2.0f - 1.0f;
            b->State->IsXPadScrubbing = true;
            return 1;
        }
    } else {
        b->State->IsXPadScrubbing = false;
    }

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
                
                bool hasCue = false;
                char timeStr[16] = "";
                Color cueColor = ColorDark1;
                
                if (ds && ds->LoadedTrack) {
                    static const Color hcPalette[8] = {
                        {0, 255, 0, 255},   {255, 0, 0, 255},   {255, 128, 0, 255},
                        {255, 255, 0, 255}, {0, 0, 255, 255},   {255, 0, 255, 255},
                        {0, 255, 255, 255}, {128, 0, 255, 255}};
                        
                    for (int h = 0; h < ds->LoadedTrack->HotCuesCount; h++) {
                        if (ds->LoadedTrack->HotCues[h].ID == (unsigned int)(i + 1)) {
                            unsigned int ms = ds->LoadedTrack->HotCues[h].Start;
                            int totalSec = ms / 1000;
                            int min = totalSec / 60;
                            int sec = totalSec % 60;
                            int frm = (ms % 1000) / 10;
                            sprintf(timeStr, "%02d:%02d.%02d", min, sec, frm);
                            hasCue = true;
                            cueColor = GetCueColor(ds->LoadedTrack->HotCues[h], hcPalette[i % 8]);
                            break;
                        }
                    }
                }
                
                DrawRectangleRec(cellRect, hasCue ? Fade(cueColor, 0.35f) : (Color){20, 20, 20, 255});
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
        // --- BEAT FX MODE ---
        DrawCentredText("BEAT FX SELECT", faceXXS, 0, halfW, barY + S(2.5f), S(7), ColorShadow);
        DrawCentredText("X-PAD", faceXXS, halfW, halfW, barY + S(2.5f), S(7), ColorShadow);

        float btnY = barY + S(13);
        float btnH = S(23);
        float trashW = S(22);
        float gap = S(2);
        static const int FXNamesCount = 3;
        static const char *FXNames[] = {"DELAY", "ECHO", "REVERB"};
        static const int FXEnumMap[] = {0, 1, 2};
        
        float fxBtnW = (halfW - trashW - S(4) - (FXNamesCount - 1) * gap) / FXNamesCount;
        float cx = S(2);

        for (int i = 0; i < FXNamesCount; i++) {
            bool active = (FXEnumMap[i] == b->State->SelectedFX);
            Color bg = active ? (Color){0x44, 0x44, 0x44, 0xFF} : (Color){0x22, 0x22, 0x22, 0xFF};
            Color border = active ? ColorPaper : ColorDark1;
            Color txtClr = active ? ColorWhite : ColorPaper;
            
            DrawRectangle(cx, btnY, fxBtnW, btnH, bg);
            DrawRectangleLines(cx, btnY, fxBtnW, btnH, border);
            DrawCentredText(FXNames[i], faceSm, cx, fxBtnW, btnY + S(6.5f), S(8), txtClr);
            cx += fxBtnW + gap;
        }

        // Trash/Clear
        DrawRectangle(cx, btnY, trashW - S(2), btnH, b->State->IsFXOn ? ColorBlue : (Color){0x22, 0x22, 0x22, 0xFF});
        DrawRectangleLines(cx, btnY, trashW - S(2), btnH, ColorDark1);
        DrawCentredText(b->State->IsFXOn ? "ON" : "OFF", faceXXS, cx, trashW - S(2), btnY + S(6.5f), S(7), ColorWhite);

        // X-PAD
        cx = halfW + S(2);
        float padAreaW = halfW - S(4);
        bool isScrubMode = (b->State->SelectedFX == 3 || b->State->SelectedFX == 5);

        if (isScrubMode) {
            DrawRectangle(cx, btnY, padAreaW, btnH, (Color){0x1A, 0x1A, 0x1A, 0xFF});
            DrawRectangleLines(cx, btnY, padAreaW, btnH, ColorDark1);
            float midX = cx + padAreaW / 2.0f;
            DrawLine(midX, btnY, midX, btnY + btnH, ColorDark3);
            if (b->State->IsXPadScrubbing) {
                float valX = midX + (b->State->XPadScrubValue * (padAreaW / 2.0f));
                if (b->State->XPadScrubValue < 0.0f) DrawRectangle(valX, btnY + 1, midX - valX, btnH - 2, (Color){0, 110, 255, 180});
                else DrawRectangle(midX, btnY + 1, valX - midX, btnH - 2, (Color){0, 110, 255, 180});
            }
        } else {
            static const int XPadLabelsCount = 6;
            static const char *XPadLabels[] = {"1/8", "1/4", "1/2", "3/4", "1", "2"};
            float padBtnW = padAreaW / XPadLabelsCount;
            for (int i = 0; i < XPadLabelsCount; i++) {
                bool active = (i == b->State->SelectedPad);
                DrawRectangle(cx, btnY, padBtnW - 1, btnH, active ? (Color){0x60, 0x60, 0x60, 0xFF} : (Color){0x22, 0x22, 0x22, 0xFF});
                DrawRectangleLines(cx, btnY, padBtnW - 1, btnH, active ? ColorWhite : ColorDark1);
                DrawCentredText(XPadLabels[i], faceSm, cx, padBtnW - 1, btnY + S(6.5f), S(8), active ? ColorWhite : ColorPaper);
                cx += padBtnW;
            }
        }
    }
}

void BeatFXSelectBar_Init(BeatFXSelectBar *b, BeatFXState *state, DeckState *deckA, DeckState *deckB, AudioEngine *audioPlugin) {
    b->base.Update = BottomStrip_Update;
    b->base.Draw = BottomStrip_Draw;
    b->State = state;
    b->DeckA = deckA;
    b->DeckB = deckB;
    b->AudioPlugin = audioPlugin;
}
