#include "ui/player/bottomstrip.h"
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "input/input.h"

static int BottomStrip_Update(Component *base) {
    BeatFXSelectBar *b = (BeatFXSelectBar *)base;
    Vector2 mouse = Input_GetPointerPos();
    float barY = TOP_BAR_H + WAVE_AREA_H;
    float halfW = SCREEN_WIDTH / 2.0f;

    Rectangle topBarRect = { 0, barY, SCREEN_WIDTH, S(12) };
    if (Touch_CheckClick(topBarRect, 0)) {
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
            if (Touch_CheckClick(r, S(2))) {
                static const int FXEnumMap[] = { 0, 1, 2 }; // DELAY, ECHO, REVERB
                int focus = b->State->FocusedSlot;
                b->State->Slots[focus].FXType = FXEnumMap[i];
                if (b->AudioPlugin) BeatFXManager_SetFX(&b->AudioPlugin->BeatFX, b->State->Slots[focus].FXType);
                return 1;
            }
            cx += fxBtnW + gap;
        }

        // Clear/Toggle FX
        Rectangle trashRect = { cx, btnY, trashW - S(2), btnH };
        if (Touch_CheckClick(trashRect, S(2))) {
            int focus = b->State->FocusedSlot;
            b->State->Slots[focus].IsOn = !b->State->Slots[focus].IsOn;
            if (b->AudioPlugin) BeatFXManager_SetFXOn(&b->AudioPlugin->BeatFX, b->State->Slots[focus].IsOn);
            return 1;
        }

        // X-Pad Interaction
        cx = halfW + S(2);
        float padAreaW = halfW - S(4);
        int selFX = b->State->Slots[b->State->FocusedSlot].FXType;
        bool isScrubMode = (selFX == 3 || selFX == 5);
        if (!isScrubMode) {
            static const int XPadLabelsCount = 6;
            float padBtnW = padAreaW / XPadLabelsCount;
            for (int i = 0; i < XPadLabelsCount; i++) {
                Rectangle padRect = { cx, btnY, padBtnW - 1, btnH };
                if (Touch_CheckClick(padRect, S(2))) {
                    b->State->SelectedPad = i;
                    return 1;
                }
                cx += padBtnW;
            }
        }
    }

    if (b->State->ShowBeatFXTab && Input_IsDown()) {
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

    DrawRectangle(0, barY, SCREEN_WIDTH, barH, Theme.BgMain);
    DrawLine(0, barY, SCREEN_WIDTH, barY, Theme.BgMain);
    DrawLine(0, barY + barH, SCREEN_WIDTH, barY + barH, Theme.BgMain);

    float halfW = SCREEN_WIDTH / 2.0f;
    DrawLine(halfW, barY + S(1), halfW, barY + barH - S(1), Theme.BgMain);

    Font faceXXS = UIFonts_GetFace(S(7));
    Font faceSm = UIFonts_GetFace(S(9));

    if (!b->State->ShowBeatFXTab) {
        // --- STATUS MODE: Hot Cue Grid ---
        DeckState *decks[2] = { b->DeckA, b->DeckB };
        float deckW = halfW;
        
        for (int d = 0; d < 2; d++) {
            DeckState *ds = decks[d];
            float dx = d * deckW;
            
            DrawCentredText(d == 0 ? "DECK 1 HOT CUE" : "DECK 2 HOT CUE", faceXXS, dx, deckW, barY + S(2.5f), S(7), Theme.BorderDefault);
            
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
                Color cueColor = Theme.BgMain;
                
                if (ds && ds->LoadedTrack) {
                    static const Color hcPalette[8] = {
                        {0, 255, 0, 255},   {255, 0, 0, 255},   {255, 128, 0, 255},
                        {255, 255, 0, 255}, {0, 0, 255, 255},   {255, 0, 255, 255},
                        {0, 255, 255, 255}, {128, 0, 255, 255}};
                        
                    for (int h = 0; h < ds->LoadedTrack->HotCuesCount; h++) {
                        HotCue hc = ds->LoadedTrack->HotCues[h];
                        if (hc.ID == (unsigned int)(i + 1)) {
                            unsigned int ms = hc.Start;
                            int totalSec = ms / 1000;
                            int min = totalSec / 60;
                            int sec = totalSec % 60;
                            int frm = (ms % 1000) / 10;
                            sprintf(timeStr, "%02d:%02d.%02d", min, sec, frm);
                            hasCue = true;
                            cueColor = GetCueColor(hc, hcPalette[i % 8]);
                            
                            bool isApproaching = false;
                            if (ds->CurrentBPM > 0) {
                                uint32_t currentPosMs = ds->PositionMs;
                                extern AudioEngine *globalAudioEngine;
                                if (ds->IsPreviewing && globalAudioEngine && ds->ID >= 0 && ds->ID < 2) {
                                    DeckAudioState *audio = &globalAudioEngine->Decks[ds->ID];
                                    if (audio->SampleRate > 0) {
                                        currentPosMs = (uint32_t)((audio->Position / (double)audio->SampleRate) * 1000.0);
                                    }
                                }

                                double distanceMs = (double)hc.Start - (double)currentPosMs;
                                if (distanceMs > 0 && distanceMs <= (60000.0 / ds->CurrentBPM) * 16.0) {
                                    isApproaching = true;
                                }
                            }
                            
                            // Active Loop or Approaching blinking
                            if ((hc.Status == 4 || isApproaching) && (int)(GetTime() * 4) % 2 == 0) {
                                cueColor = Theme.TextPrimary;
                            }
                            break;
                        }
                    }
                }
                
                DrawRectangleRec(cellRect, hasCue ? Fade(cueColor, 0.35f) : Theme.BgMain);
                DrawRectangleLinesEx(cellRect, 1, hasCue ? cueColor : Theme.TextSecondary);
                
                char idStr[4];
                sprintf(idStr, "%c", 'A' + i);
                UIDrawText(idStr, faceXXS, cx + S(3), cy + S(3), S(7), hasCue ? Theme.TextPrimary : Theme.BorderDefault);
                
                if (hasCue) {
                    UIDrawText(timeStr, faceXXS, cx + S(12), cy + S(3.5f), S(7), Theme.TextPrimary);
                }
            }
        }
    } else {
        // --- BEAT FX MODE ---
        DrawCentredText("BEAT FX SELECT", faceXXS, 0, halfW, barY + S(2.5f), S(7), Theme.BorderDefault);
        DrawCentredText("X-PAD", faceXXS, halfW, halfW, barY + S(2.5f), S(7), Theme.BorderDefault);

        float btnY = barY + S(13);
        float btnH = S(23);
        float trashW = S(22);
        float gap = S(2);
        static const int FXNamesCount = 3;
        static const char *FXNames[] = {"DELAY", "ECHO", "REVERB"};
        static const int FXEnumMap[] = {0, 1, 2};
        
        float fxBtnW = (halfW - trashW - S(4) - (FXNamesCount - 1) * gap) / FXNamesCount;
        float cx = S(2);

        int focus = b->State->FocusedSlot;
        for (int i = 0; i < FXNamesCount; i++) {
            bool active = (FXEnumMap[i] == b->State->Slots[focus].FXType);
            Color bg = active ? Theme.BgPanelAlt : Theme.BgPanel;
            Color border = active ? Theme.TextPrimary : Theme.BorderDefault;
            Color txtClr = active ? Theme.TextPrimary : Theme.TextSecondary;
            
            DrawRectangle(cx, btnY, fxBtnW, btnH, bg);
            DrawRectangleLines(cx, btnY, fxBtnW, btnH, border);
            DrawCentredText(FXNames[i], faceSm, cx, fxBtnW, btnY + S(6.5f), S(8), txtClr);
            cx += fxBtnW + gap;
        }

        // Trash/Clear
        bool isFxBlinking = (fmod(GetTime(), 0.5) < 0.25);
        Color onBgColor = isFxBlinking ? Theme.AccentBlue : Theme.BgPanelAlt;
        DrawRectangle(cx, btnY, trashW - S(2), btnH, b->State->Slots[focus].IsOn ? onBgColor : Theme.BgPanel);
        DrawRectangleLines(cx, btnY, trashW - S(2), btnH, b->State->Slots[focus].IsOn ? Theme.TextPrimary : Theme.BorderDefault);
        DrawCentredText(b->State->Slots[focus].IsOn ? "ON" : "OFF", faceXXS, cx, trashW - S(2), btnY + S(6.5f), S(7), b->State->Slots[focus].IsOn ? Theme.TextPrimary : Theme.TextSecondary);

        // X-PAD
        cx = halfW + S(2);
        float padAreaW = halfW - S(4);
        int selFX2 = b->State->Slots[focus].FXType;
        bool isScrubMode = (selFX2 == 3 || selFX2 == 5);

        if (isScrubMode) {
            DrawRectangle(cx, btnY, padAreaW, btnH, Theme.BgPanelAlt);
            DrawRectangleLines(cx, btnY, padAreaW, btnH, Theme.BgMain);
            float midX = cx + padAreaW / 2.0f;
            DrawLine(midX, btnY, midX, btnY + btnH, Theme.BgPanelAlt);
            if (b->State->IsXPadScrubbing) {
                float valX = midX + (b->State->XPadScrubValue * (padAreaW / 2.0f));
                if (b->State->XPadScrubValue < 0.0f) DrawRectangle(valX, btnY + 1, midX - valX, btnH - 2, Theme.SelectedItem);
                else DrawRectangle(midX, btnY + 1, valX - midX, btnH - 2, Theme.SelectedItem);
            }
        } else {
            static const int XPadLabelsCount = 6;
            static const char *XPadLabels[] = {"1/8", "1/4", "1/2", "3/4", "1", "2"};
            float padBtnW = padAreaW / XPadLabelsCount;
            for (int i = 0; i < XPadLabelsCount; i++) {
                bool active = (i == b->State->SelectedPad);
                DrawRectangle(cx, btnY, padBtnW - 1, btnH, active ? Theme.BgPanelAlt : Theme.BgPanel);
                DrawRectangleLines(cx, btnY, padBtnW - 1, btnH, active ? Theme.TextPrimary : Theme.BorderDefault);
                DrawCentredText(XPadLabels[i], faceSm, cx, padBtnW - 1, btnY + S(6.5f), S(8), active ? Theme.TextPrimary : Theme.TextSecondary);
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
