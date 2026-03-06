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
    BeatFXSelectBar *b = (BeatFXSelectBar *)base;
    Vector2 mouse = UIGetMousePosition();
    float barY = TOP_BAR_H + WAVE_AREA_H;
    float barH = FX_BAR_H;
    float halfW = SCREEN_WIDTH / 2.0f;

    if (!b->State->ShowBeatFXTab) {
        // --- STATUS MODE: Hot Cue Grid ---
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            DeckState *decks[2] = { b->DeckA, b->DeckB };
            float deckW = halfW;
            float gridY = barY + S(11);
            float cellW = (deckW - S(8)) / 4.0f;
            float cellH = (barH - S(14)) / 2.0f;
            
            for (int d = 0; d < 2; d++) {
                DeckState *ds = decks[d];
                float dx = d * deckW;
                
                for (int i = 0; i < 8; i++) {
                    int row = i / 4;
                    int col = i % 4;
                    float cx = dx + S(4) + col * cellW;
                    float cy = gridY + row * cellH;
                    
                    Rectangle cellRect = { cx + S(1), cy + S(1), cellW - S(2), cellH - S(2) };
                    if (CheckCollisionPointRec(mouse, cellRect)) {
                        if (ds && ds->LoadedTrack && b->AudioPlugin) {
                            for (int h = 0; h < ds->LoadedTrack->HotCuesCount; h++) {
                                if (ds->LoadedTrack->HotCues[h].ID == (unsigned int)(i + 1)) {
                                    DeckAudio_JumpToMs(&b->AudioPlugin->Decks[d], ds->LoadedTrack->HotCues[h].Start);
                                    DeckAudio_InstantPlay(&b->AudioPlugin->Decks[d]);
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        // --- BEAT FX MODE: FX Select & X-PAD ---
        float btnY = barY + S(13);
        float btnH = S(23);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // FX Select
            float trashW = S(22);
            float gap = S(2);
            float fxBtnW = (halfW - trashW - S(4) - (FXNamesCount - 1) * gap) / FXNamesCount;
            float cx = S(2);

            for (int i = 0; i < FXNamesCount; i++) {
                Rectangle fxRect = { cx, btnY, fxBtnW, btnH };
                if (CheckCollisionPointRec(mouse, fxRect)) {
                    b->State->SelectedFX = i;
                    return 1;
                }
                cx += fxBtnW + gap;
            }
        }

        // X-PAD
        float cx = halfW + S(2);
        float padAreaW = halfW - S(4);
        bool isScrubMode = (b->State->SelectedFX == 3 || b->State->SelectedFX == 5); // 3=REVERB, 5=FLANGER

        if (isScrubMode) {
            Rectangle scrubArea = { cx, btnY, padAreaW, btnH };
            if (CheckCollisionPointRec(mouse, scrubArea) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                b->State->IsXPadScrubbing = true;
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                b->State->IsXPadScrubbing = false;
            }
            if (b->State->IsXPadScrubbing) {
                float val = (mouse.x - cx) / padAreaW; // 0.0 to 1.0
                b->State->XPadScrubValue = (val * 2.0f) - 1.0f; // -1.0 to 1.0
                if (b->State->XPadScrubValue < -1.0f) b->State->XPadScrubValue = -1.0f;
                if (b->State->XPadScrubValue > 1.0f) b->State->XPadScrubValue = 1.0f;
                return 1;
            }
        } else {
            b->State->IsXPadScrubbing = false;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
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
                    Color hcColors[8] = {
                        {0, 255, 0, 255}, {255, 0, 0, 255}, {255, 128, 0, 255}, {255, 255, 0, 255},
                        {0, 0, 255, 255}, {255, 0, 255, 255}, {0, 255, 255, 255}, {128, 0, 255, 255}
                    };
                    for (int h = 0; h < ds->LoadedTrack->HotCuesCount; h++) {
                        if (ds->LoadedTrack->HotCues[h].ID == (unsigned int)(i + 1)) {
                            unsigned int ms = ds->LoadedTrack->HotCues[h].Start;
                            int totalSec = ms / 1000;
                            int min = totalSec / 60;
                            int sec = totalSec % 60;
                            int frm = (ms % 1000) / 10;
                            sprintf(timeStr, "%02d:%02d.%02d", min, sec, frm);
                            hasCue = true;
                            
                            unsigned char cR = ds->LoadedTrack->HotCues[h].Color[0];
                            unsigned char cG = ds->LoadedTrack->HotCues[h].Color[1];
                            unsigned char cB = ds->LoadedTrack->HotCues[h].Color[2];
                            
                            if (cR == 0 && cG == 0 && cB == 0) {
                                cueColor = hcColors[i % 8];
                            } else {
                                cueColor = (Color){ cR, cG, cB, 255 };
                            }
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
        bool isScrubMode = (b->State->SelectedFX == 3 || b->State->SelectedFX == 5);

        if (isScrubMode) {
            // Draw outer scrub box
            DrawRectangle(cx, btnY, padAreaW, btnH, (Color){0x1A, 0x1A, 0x1A, 0xFF});
            DrawRectangleLines(cx, btnY, padAreaW, btnH, ColorDark1);

            // Draw center tick
            float midX = cx + padAreaW / 2.0f;
            DrawLine(midX, btnY, midX, btnY + btnH, ColorDark3);

            // Draw active blue bar if scrubbing
            if (b->State->IsXPadScrubbing) {
                float valX = midX + (b->State->XPadScrubValue * (padAreaW / 2.0f));
                
                Color fillClr = (Color){0, 110, 255, 180}; 
                if (b->State->XPadScrubValue < 0.0f) {
                    DrawRectangle(valX, btnY + 1, midX - valX, btnH - 2, fillClr);
                } else {
                    DrawRectangle(midX, btnY + 1, valX - midX, btnH - 2, fillClr);
                }
            }

            // Draw Arrows
            DrawCentredText("<", faceSm, cx, S(20), btnY + S(6.5f), S(8), (Color){0, 110, 255, 255});
            DrawCentredText(">", faceSm, cx + padAreaW - S(20), S(20), btnY + S(6.5f), S(8), (Color){0, 110, 255, 255});

        } else {
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
}

void BeatFXSelectBar_Init(BeatFXSelectBar *b, BeatFXState *state, DeckState *deckA, DeckState *deckB, AudioEngine *audioPlugin) {
    b->base.Update = BottomStrip_Update;
    b->base.Draw = BottomStrip_Draw;
    b->State = state;
    b->DeckA = deckA;
    b->DeckB = deckB;
    b->AudioPlugin = audioPlugin;
}
