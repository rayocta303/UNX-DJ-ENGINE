#include "ui/player/beatfx.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "core/logic/quantize.h"
#include "audio/engine.h"
#include "ui/player/waveform.h"
#include "input/input.h"
#include <stdio.h>
#include <math.h>

static const char* AllFXNames[] = {
    "DELAY", "ECHO", "PING PONG", "SPIRAL", "REVERB", "TRANS", 
    "FILTER", "FLANGER", "PHASER", "PITCH", "SLIP ROLL", "ROLL", 
    "VINYL BRAKE", "HELIX"
};
#define ALL_FX_COUNT 14

static int BeatFX_Update(Component *base) {
    BeatFXPanel *b = (BeatFXPanel *)base;
    Vector2 mouse = Input_GetPointerPos();
    
    float x = BEAT_FX_X;
    float y = TOP_BAR_H;
    float w = BEAT_FX_W;
    float h = WAVE_AREA_H;
    
    // Calculate FX Label and Select hit box
    float fxSelectY = y + S(4) + S(13); // Label + Spacing
    Rectangle fxSelectRect = { x + S(4), fxSelectY, w - S(8), S(26) };

    // Calculate CH SELECT hit box (following Draw logic)
    float cy = fxSelectY + S(30); // FXSelect(26) + Spacing(4)
    cy += S(10); // Spacing after "CH SELECT" label
    
    // 1. Sync MIDI Hardware state for Focused Effect
    // If the Mixxx script modified the focused effect (1 to 3), sync it to our 0-5 index
    if (b->State->MidiFocusedEffectUnit1 != b->State->PrevMidiFocusedEffectUnit1 && b->State->MidiFocusedEffectUnit1 > 0) {
        // Hardware forced focus change (usually 1, 2, or 3)
        // Keep it within Unit 1 for simplicity (slots 0-2) or Unit 2 (slots 3-5) depending on current bank
        int currentBank = (b->State->FocusedSlot >= 3) ? 1 : 0;
        b->State->FocusedSlot = currentBank * 3 + (b->State->MidiFocusedEffectUnit1 - 1);
        if (b->State->FocusedSlot > 5) b->State->FocusedSlot = 5;
        b->State->PrevMidiFocusedEffectUnit1 = b->State->MidiFocusedEffectUnit1;
    }

    Rectangle chRect = { x + S(4), cy, w - S(8), S(20) };
    

    if (b->State->FXDropdownOpen) {
        float modalW = S(420);
        float modalH = S(270);
        if (b->State->ActiveSlotDropdown == -1) {
            modalW = S(380);
            modalH = S(200);
        }
        float modalX = (SCREEN_WIDTH - modalW) / 2.0f;
        float modalY = (SCREEN_HEIGHT - modalH) / 2.0f;
        Rectangle modalRect = { modalX, modalY, modalW, modalH };

        if (UI_UpdateModal(modalRect)) {
            if (b->State->ActiveSlotDropdown >= 0) {
                b->State->ActiveSlotDropdown = -1; // Go back to slots view
            } else {
                b->State->FXDropdownOpen = false;
            }
        } else if (Input_IsReleased()) {
            if (b->State->ActiveSlotDropdown >= 0) {
                // 14-grid selection
                float cols = 3;
                float pad = S(10);
                float btnW = (modalW - pad * 4) / cols;
                float btnH = (modalH - S(45) - pad * 6) / 5;

                for (int i = 0; i < ALL_FX_COUNT; i++) {
                    int row = i / (int)cols;
                    int col = i % (int)cols;
                    float bx = modalX + pad + col * (btnW + pad);
                    float by = modalY + S(45) + row * (btnH + pad);
                    if (CheckCollisionPointRec(mouse, (Rectangle){bx, by, btnW, btnH})) {
                        b->State->Slots[b->State->ActiveSlotDropdown].FXType = i;
                        if (b->State->FocusedSlot == b->State->ActiveSlotDropdown && b->AudioPlugin) {
                            BeatFXManager_SetFX(&b->AudioPlugin->BeatFX, i);
                        }
                        b->State->ActiveSlotDropdown = -1;
                        Input_Consume();
                        break;
                    }
                }
            } else {
                // 6-slot selection
                float cols = 2; // FX1, FX2
                float rows = 3;
                float pad = S(8);
                float btnW = (modalW - pad * 3) / cols;
                float btnH = (modalH - S(45) - pad * 4) / rows;

                for (int i = 0; i < 6; i++) {
                    int col = i / 3;
                    int row = i % 3;
                    float bx = modalX + pad + col * (btnW + pad);
                    float by = modalY + S(45) + row * (btnH + pad);
                    if (CheckCollisionPointRec(mouse, (Rectangle){bx, by, btnW, btnH})) {
                        b->State->FocusedSlot = i;
                        
                        // Sync back to MIDI
                        int unit = (i >= 3) ? 2 : 1;
                        int idx = (i % 3) + 1;
                        if (unit == 1) {
                            b->State->MidiFocusedEffectUnit1 = idx;
                            b->State->PrevMidiFocusedEffectUnit1 = idx;
                        } else {
                            b->State->MidiFocusedEffectUnit2 = idx;
                            b->State->PrevMidiFocusedEffectUnit2 = idx;
                        }

                        b->State->ActiveSlotDropdown = i; // Open dropdown for this slot
                        
                        if (b->AudioPlugin) {
                            BeatFXManager_SetFX(&b->AudioPlugin->BeatFX, b->State->Slots[i].FXType);
                            BeatFXManager_SetFXOn(&b->AudioPlugin->BeatFX, b->State->Slots[i].IsOn);
                        }
                        
                        Input_Consume();
                        break;
                    }
                }
            }
        }

        if (b->State->MidiBrowseDelta != 0) {
            int currentIdx = (b->State->ActiveSlotDropdown >= 0) ? b->State->ActiveSlotDropdown : b->State->FocusedSlot;
            int current = b->State->Slots[currentIdx].FXType;
            if (b->State->MidiBrowseDelta > 0) {
                current = (current + 1) % ALL_FX_COUNT;
            } else if (b->State->MidiBrowseDelta < 0) {
                current = (current - 1 + ALL_FX_COUNT) % ALL_FX_COUNT;
            }
            b->State->Slots[currentIdx].FXType = current;
            if (currentIdx == b->State->FocusedSlot && b->AudioPlugin) {
                BeatFXManager_SetFX(&b->AudioPlugin->BeatFX, current);
            }
            b->State->MidiBrowseDelta = 0;
        }

        if (b->State->MidiRequestEnter || b->State->MidiRequestBack || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
            if (b->State->ActiveSlotDropdown >= 0) {
                b->State->ActiveSlotDropdown = -1;
            } else {
                b->State->FXDropdownOpen = false;
            }
            b->State->MidiRequestEnter = false;
            b->State->MidiRequestBack = false;
        }
        return 0; // Block other interactions
    } else if (b->State->ChannelDropdownOpen) {
        float modalW = S(280);
        float modalH = S(220);
        float modalX = (SCREEN_WIDTH - modalW) / 2.0f;
        float modalY = (SCREEN_HEIGHT - modalH) / 2.0f;
        Rectangle modalRect = { modalX, modalY, modalW, modalH };

        if (UI_UpdateModal(modalRect)) {
            b->State->ChannelDropdownOpen = false;
        } else if (Input_IsReleased()) {
            float pad = S(12);
            float btnW = modalW - pad * 2;
            float btnH = S(44);

            for (int i = 0; i < 3; i++) {
                float bx = modalX + pad;
                float by = modalY + S(45) + i * (btnH + pad);
                if (CheckCollisionPointRec(mouse, (Rectangle){bx, by, btnW, btnH})) {
                    b->State->SelectedChannel = i;
                    if (b->AudioPlugin) b->AudioPlugin->BeatFX.targetChannel = i;
                    b->State->ChannelDropdownOpen = false;
                    Input_Consume();
                    break;
                }
            }
        }

        if (b->State->MidiBrowseDelta != 0) {
            int current = b->State->SelectedChannel;
            if (b->State->MidiBrowseDelta > 0) {
                current = (current + 1) % 3;
            } else if (b->State->MidiBrowseDelta < 0) {
                current = (current - 1 + 3) % 3;
            }
            b->State->SelectedChannel = current;
            if (b->AudioPlugin) b->AudioPlugin->BeatFX.targetChannel = current;
            b->State->MidiBrowseDelta = 0;
        }

        if (b->State->MidiRequestEnter || b->State->MidiRequestBack || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
            b->State->ChannelDropdownOpen = false;
            b->State->MidiRequestEnter = false;
            b->State->MidiRequestBack = false;
        }

        return 0; // Block other interactions
    } else {
        float slotH = S(26);
        Rectangle slotRect = { x + S(4), fxSelectY, w - S(8), slotH };
        if (Touch_CheckClick(slotRect, S(2))) {
            b->State->FXDropdownOpen = true; // Open the Effect Rack Modal
        }
    
        bool chHovered = CheckCollisionPointRec(mouse, chRect);
        if (chHovered) {
            float wheel = Mouse_GetWheel();
            if (wheel != 0) {
                int next = b->State->SelectedChannel - (int)wheel;
                if (next < 0) next = 2;
                if (next > 2) next = 0;
                b->State->SelectedChannel = next;
                if (b->AudioPlugin) b->AudioPlugin->BeatFX.targetChannel = next;
            }
        }

        if (Touch_CheckClick(chRect, S(4))) {
            b->State->ChannelDropdownOpen = true;
        }
    }
    
    float containerY = cy + S(20);
    float rowH = S(56) / 4.0f;
    Rectangle qRect = { x + S(4), containerY + 3 * rowH, w - S(8), rowH };
    if (Touch_CheckClick(qRect, S(2))) {
        b->State->Quantize = !b->State->Quantize;
    }

    if (Touch_CheckClick(b->FXButton, S(2))) {
        int focus = b->State->FocusedSlot;
        b->State->Slots[focus].IsOn = !b->State->Slots[focus].IsOn;
        // In Option A (Single DSP), we just toggle the master DSP engine state if the focused slot changes
        if (b->AudioPlugin) BeatFXManager_SetFXOn(&b->AudioPlugin->BeatFX, b->State->Slots[focus].IsOn);
    }

    // 4. Tab Switching Logic
    float tabY = y + h - S(18);
    float tabW = (w - S(10)) / 2;
    Rectangle statusTab = { x + S(4), tabY, tabW, S(14) };
    Rectangle beatFxTab = { x + S(6) + tabW, tabY, tabW, S(14) };

    if (Touch_CheckClick(statusTab, S(5))) {
        b->State->ShowBeatFXTab = false;
    } else if (Touch_CheckClick(beatFxTab, S(5))) {
        b->State->ShowBeatFXTab = true;
    }

    // --- ZOOM BUTTONS ---
    float plusMinusY = b->FXButton.y + b->FXButton.height + S(12);
    float halfB = (w - S(12)) / 2;
    Rectangle minusRect = { x + S(4), plusMinusY, halfB, S(20) };
    Rectangle plusRect = { x + S(8) + halfB, plusMinusY, halfB, S(20) };

    int focusDelta = 0;
    if (Touch_CheckClick(minusRect, S(4))) focusDelta = -1; // UP / LEFT
    if (Touch_CheckClick(plusRect, S(4))) focusDelta = 1;   // DOWN / RIGHT

    if (focusDelta != 0) {
        int next = b->State->FocusedSlot + focusDelta;
        if (next < 0) next = 2;
        if (next > 2) next = 0;
        b->State->FocusedSlot = next;
        
        // Sync single DSP engine to the newly focused slot
        if (b->AudioPlugin) {
            BeatFXManager_SetFX(&b->AudioPlugin->BeatFX, b->State->Slots[next].FXType);
            BeatFXManager_SetFXOn(&b->AudioPlugin->BeatFX, b->State->Slots[next].IsOn);
        }

        // Sync back to MIDI
        int unit = (next >= 3) ? 2 : 1;
        int idx = (next % 3) + 1;
        if (unit == 1) {
            b->State->MidiFocusedEffectUnit1 = idx;
            b->State->PrevMidiFocusedEffectUnit1 = idx;
        } else {
            b->State->MidiFocusedEffectUnit2 = idx;
            b->State->PrevMidiFocusedEffectUnit2 = idx;
        }
    }

    return 0;
}

static void BeatFX_Draw(Component *base) {
    BeatFXPanel *b = (BeatFXPanel *)base;
    (void)b;
    float x = BEAT_FX_X;
    float y = TOP_BAR_H;
    float w = BEAT_FX_W;
    float h = WAVE_AREA_H;

    DrawRectangle(x, y, w, h, Theme.BgPanel);
    DrawLine(x, y, x, y + h, Theme.BorderDefault);

    Font faceXXS = UIFonts_GetFace(S(7));
    Font faceXS = UIFonts_GetFace(S(8));
    Font faceSm = UIFonts_GetFace(S(9));
    Font faceMd = UIFonts_GetFace(S(10));
    Font faceLg = UIFonts_GetFace(S(12));

    float cy = y + S(4);
    DrawCentredText("BEAT FX", faceXS, x, w, cy, S(8), Theme.TextPrimary);
    cy += S(13);

    // 1. Focused Effect
    float slotH = S(26);
    Rectangle slotRect = { x + S(4), cy, w - S(8), slotH };
    int focusIdx = b->State->FocusedSlot;
    bool isOn = b->State->Slots[focusIdx].IsOn;
    
    Color bgCol = isOn ? Theme.SelectedItem : Theme.BgMain;
    Color borderCol = isOn ? Theme.AccentBlue : Theme.BgPanel;
    
    DrawRectangleRec(slotRect, bgCol);
    DrawRectangleLinesEx(slotRect, 1.0f, borderCol);
    
    const char* fxName = AllFXNames[b->State->Slots[focusIdx].FXType % ALL_FX_COUNT];
    Color textCol = isOn ? Theme.TextPrimary : Theme.TextPrimary;
    DrawCentredText(fxName, faceMd, slotRect.x, slotRect.width, slotRect.y + S(7), S(10), textCol);
    
    // Number indicator
    char numStr[4];
    sprintf(numStr, "%d", focusIdx + 1);
    DrawCentredText(numStr, faceSm, slotRect.x, S(16), slotRect.y + S(8), S(9), Theme.AccentOrange);
    
    cy += slotH + S(12); // Reduced gap between FX slot and CH SELECT


    // 2. CH SELECT
    DrawCentredText("CH SELECT", faceXXS, x, w, cy, S(7), Theme.BorderDefault);
    cy += S(10);
    
    const char* chName = "MASTER";
    if (b->State->SelectedChannel == 1) chName = "DECK 1";
    if (b->State->SelectedChannel == 2) chName = "DECK 2";
    
    DrawRectangle(x + S(4), cy, w - S(8), S(22), Theme.BgPanelAlt);
    DrawRectangleLines(x + S(4), cy, w - S(8), S(22), Theme.BorderDefault);
    DrawCentredText(chName, faceSm, x + S(4), w - S(8), cy + S(6.5f), S(9), Theme.TextPrimary);
    DrawTriangle((Vector2){x + w - S(12), cy + S(9)}, (Vector2){x + w - S(6), cy + S(9)}, (Vector2){x + w - S(9), cy + S(15)}, Theme.TextPrimary);
    
    cy += S(28); // CH button(22) + gap(6)


    // 3. Black parameter container
    float containerH = S(56);
    DrawRectangle(x + S(4), cy, w - S(8), containerH, Theme.BgMain);
    DrawRectangleLines(x + S(4), cy, w - S(8), containerH, Theme.BorderDefault);

    float rowH = containerH / 4.0f;
    for (int i = 1; i < 4; i++) {
        float lx = x + S(4);
        float ly = cy + i * rowH;
        DrawLine(lx, ly, lx + w - S(8), ly, Theme.BorderDefault);
    }

    // BPM & Beat FX Sync Logic
    float masterBpm = 120.0f;
    DeckState *masterDeck = b->DeckA;
    if (b->DeckA->IsMaster) {
        masterBpm = b->DeckA->CurrentBPM;
        masterDeck = b->DeckA;
    } else if (b->DeckB->IsMaster) {
        masterBpm = b->DeckB->CurrentBPM;
        masterDeck = b->DeckB;
    } else {
        masterBpm = b->DeckA->CurrentBPM;
    }

    // DSP DJ Logic: X-PAD Ratios
    static const float XPadRatios[] = {0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f};
    static const char* XPadLabels[] = {"1/8", "1/4", "1/2", "3/4", "1", "2"};
    
    int padIdx = b->State->SelectedPad;
    if (padIdx < 0) padIdx = 4; // Default to "1"
    if (padIdx >= 6) padIdx = 5; 
    
    float ratio = XPadRatios[padIdx];
    float fxBpm = 0.0f;
    float fxMs = 0.0f;
    
    if (masterBpm > 0.0f) {
        fxBpm = masterBpm / ratio; // Kecepatan (LFO) = BPM / ratio
        
        if (b->State->Quantize && masterDeck && masterDeck->LoadedTrack) {
            float qMs = Quantize_GetBeatFXLengthMs(masterDeck->LoadedTrack, ratio);
            if (qMs > 0.0f) fxMs = qMs;
            else fxMs = (60000.0f / masterBpm) * ratio; // Fallback
        } else {
            fxMs = (60000.0f / masterBpm) * ratio; // Waktu (ms) = (60000/BPM) * ratio
        }
    }

    // Draw Calculated BPM
    char bpmStr[16] = "--.-";
    if (masterBpm > 0.0f) sprintf(bpmStr, "%.1f", fxBpm);
    UIDrawText(bpmStr, faceMd, x + S(8), cy + S(2), S(10), Theme.TextPrimary);
    UIDrawText("BPM", faceXXS, x + w - S(26), cy + S(4), S(7), Theme.BorderDefault);
    cy += rowH;

    // msec display
    char msStr[16] = "---";
    if (masterBpm > 0.0f) sprintf(msStr, "%.0f", fxMs);
    UIDrawText(msStr, faceSm, x + S(8), cy + S(2), S(9), Theme.TextPrimary);
    UIDrawText("msec", faceXXS, x + w - S(26), cy + S(3), S(7), Theme.BorderDefault);
    cy += rowH;

    // BEAT display
    int selFX = b->State->Slots[b->State->FocusedSlot].FXType;
    if ((selFX == 3 || selFX == 5) && b->State->IsXPadScrubbing) {
        float scrubVal = b->State->XPadScrubValue;
        if (selFX == 3) { // REVERB
            if (scrubVal < -0.05f) {
                UIDrawText("LPF", faceSm, x + S(8), cy + S(2), S(9), Theme.AccentOrange);
            } else if (scrubVal > 0.05f) {
                UIDrawText("HPF", faceSm, x + S(8), cy + S(2), S(9), Theme.AccentOrange);
            } else {
                UIDrawText("OFF", faceSm, x + S(8), cy + S(2), S(9), Theme.TextPrimary);
            }
            UIDrawText("FILTER", faceXXS, x + w - S(32), cy + S(3), S(7), Theme.TextPrimary);
        } else { // FLANGER
            char ptStr[16];
            sprintf(ptStr, "%+.0f%%", scrubVal * 100.0f); 
            UIDrawText(ptStr, faceSm, x + S(8), cy + S(2), S(9), Theme.AccentOrange);
            UIDrawText("PITCH", faceXXS, x + w - S(30), cy + S(3), S(7), Theme.TextPrimary);
        }
    } else {
        UIDrawText(XPadLabels[padIdx], faceSm, x + S(8), cy + S(2), S(9), Theme.TextPrimary);
        UIDrawText("BEAT", faceXXS, x + w - S(26), cy + S(3), S(7), Theme.BorderDefault);
    }
    cy += rowH;

    // QUANTIZE
    DrawCentredText("QUANTIZE", faceXXS, x + S(4), w - S(8), cy + S(3), S(7), b->State->Quantize ? Theme.AccentRed : Theme.BorderDefault);
    cy += rowH + S(12);

    // 3.5 FX ON / OFF Toggle
    float btnH = S(18);
    float btnW = w - S(8); 
    
    b->FXButton = (Rectangle){ x + S(4), cy, btnW, btnH };

    bool isFxBlinking = (fmod(GetTime(), 0.5) < 0.25);
    bool isFocusedOn = b->State->Slots[b->State->FocusedSlot].IsOn;
    Color btnColor = isFocusedOn ? (isFxBlinking ? Theme.AccentBlue : Theme.HoverActive) : Theme.BgPanel;
    DrawRectangleRec(b->FXButton, btnColor);
    DrawRectangleLinesEx(b->FXButton, 1.0f, Theme.TextPrimary);
    
    const char* fxBtnText = isFocusedOn ? "BEAT FX ON" : "BEAT FX OFF";
    DrawCentredText(fxBtnText, faceSm, b->FXButton.x, b->FXButton.width, b->FXButton.y + S(5.0f), S(9), Theme.TextPrimary);
    
    cy += btnH + S(12);

    // 4b. +/- BUTTONS (Restored without ZOOM/GRID label)
    float halfB = (w - S(12)) / 2;
    Rectangle minusRect = { x + S(4), cy, halfB, S(20) };
    Color minusColor = (Input_IsDown() && CheckCollisionPointRec(Input_GetPointerPos(), minusRect)) ? Theme.TextSecondary : Theme.BgPanel;
    DrawRectangleRec(minusRect, minusColor);
    DrawRectangleLinesEx(minusRect, 1.0f, Theme.BorderDefault);
    DrawCentredText("-", faceSm, x + S(4), halfB, cy + S(5.5f), S(9), Theme.TextPrimary);

    Rectangle plusRect = { x + S(8) + halfB, cy, halfB, S(20) };
    Color plusColor = (Input_IsDown() && CheckCollisionPointRec(Input_GetPointerPos(), plusRect)) ? Theme.TextSecondary : Theme.BgPanel;
    DrawRectangleRec(plusRect, plusColor);
    DrawRectangleLinesEx(plusRect, 1.0f, Theme.BorderDefault);
    DrawCentredText("+", faceSm, x + S(8) + halfB, halfB, cy + S(5.5f), S(9), Theme.TextPrimary);

    cy = y + h - S(18); // Bottom alignment

    // 5. STATUS / BEAT FX tabs
    float tabW = (w - S(10)) / 2;
    
    // STATUS Tab
    bool statusActive = !b->State->ShowBeatFXTab;
    DrawRectangle(x + S(4), cy, tabW, S(14), statusActive ? Theme.BgPanelAlt : Theme.BgMain);
    DrawRectangleLines(x + S(4), cy, tabW, S(14), Theme.BorderDefault);
    DrawCentredText("STATUS", faceXXS, x + S(4), tabW, cy + S(3.5f), S(7), statusActive ? Theme.TextPrimary : Theme.BorderDefault);

    // BEAT FX Tab
    bool beatFxActive = b->State->ShowBeatFXTab;
    DrawRectangle(x + S(6) + tabW, cy, tabW, S(14), beatFxActive ? Theme.BgPanelAlt : Theme.BgMain);
    DrawRectangleLines(x + S(6) + tabW, cy, tabW, S(14), Theme.BorderDefault);
    DrawCentredText("BEAT FX", faceXXS, x + S(6) + tabW, tabW, cy + S(3.5f), S(7), beatFxActive ? Theme.TextPrimary : Theme.BorderDefault);

}

void BeatFXPanel_DrawOverlays(BeatFXPanel *b) {
    if (!b->State->FXDropdownOpen && !b->State->ChannelDropdownOpen) return;
    
    Font faceSm = UIFonts_GetFace(S(9));
    Font faceMd = UIFonts_GetFace(S(10));
    Font faceLg = UIFonts_GetFace(S(12));
    
    Vector2 mouse = Input_GetPointerPos();
    if (b->State->FXDropdownOpen) {
        UI_DrawModalBackdrop();
        
        float modalW = S(420);
        float modalH = S(270);
        float modalX = (SCREEN_WIDTH - modalW) / 2.0f;
        float modalY = (SCREEN_HEIGHT - modalH) / 2.0f;
        
        Rectangle body = UI_DrawModalFrame((Rectangle){modalX, modalY, modalW, modalH}, "SELECT BEAT FX");
        
        if (b->State->ActiveSlotDropdown >= 0) {
            float cols = 3;
            float rows = 5;
            float pad = S(10);
            float btnW = (modalW - pad * 4) / cols;
            float btnH = (modalH - S(45) - pad * 6) / rows;
            
            for (int i = 0; i < ALL_FX_COUNT; i++) {
                int row = i / (int)cols;
                int col = i % (int)cols;
                float bx = modalX + pad + col * (btnW + pad);
                float by = modalY + S(45) + row * (btnH + pad);
                Rectangle optRect = { bx, by, btnW, btnH };
                
                Color bg = (b->State->Slots[b->State->ActiveSlotDropdown].FXType == i) ? Theme.AccentBlue : Theme.BgPanel;
                if (CheckCollisionPointRec(mouse, optRect)) bg = Theme.TextSecondary;
                
                DrawRectangleRec(optRect, bg);
                DrawRectangleLinesEx(optRect, 1.0f, (b->State->Slots[b->State->ActiveSlotDropdown].FXType == i) ? Theme.TextPrimary : Theme.BgPanelAlt);
                DrawCentredText(AllFXNames[i], faceSm, optRect.x, optRect.width, optRect.y + (btnH - S(10)) / 2.0f, S(10), Theme.TextPrimary);
            }
        } else {
            float cols = 2; // FX1, FX2
            float rows = 3;
            float pad = S(8);
            float btnW = (modalW - pad * 3) / cols;
            float btnH = (modalH - S(45) - pad * 4) / rows;
            
            for (int i = 0; i < 6; i++) {
                int col = i / 3;
                int row = i % 3;
                float bx = modalX + pad + col * (btnW + pad);
                float by = modalY + S(45) + row * (btnH + pad);
                Rectangle optRect = { bx, by, btnW, btnH };
                
                bool isFocused = (b->State->FocusedSlot == i);
                bool isOn = b->State->Slots[i].IsOn;
                
                Color bgCol = isOn ? Theme.SelectedItem : Theme.BgPanel;
                Color borderCol = isFocused ? Theme.TextPrimary : (isOn ? Theme.AccentBlue : Theme.BgPanelAlt);
                if (CheckCollisionPointRec(mouse, optRect)) bgCol = Theme.TextSecondary;
                
                DrawRectangleRec(optRect, bgCol);
                DrawRectangleLinesEx(optRect, isFocused ? 1.5f : 1.0f, borderCol);
                
                const char* fxName = AllFXNames[b->State->Slots[i].FXType % ALL_FX_COUNT];
                Color textCol = isOn ? Theme.TextPrimary : (isFocused ? Theme.TextPrimary : Theme.BorderDefault);
                DrawCentredText(fxName, faceMd, optRect.x, optRect.width, optRect.y + (btnH - S(10)) / 2.0f, S(10), textCol);
                
                // Number indicator
                char numStr[4];
                sprintf(numStr, "%d", i + 1);
                DrawCentredText(numStr, faceSm, optRect.x, S(16), optRect.y + (btnH - S(10)) / 2.0f + S(1), S(9), isFocused ? Theme.AccentOrange : Theme.BorderDefault);
            }
        }
    } else if (b->State->ChannelDropdownOpen) {
        UI_DrawModalBackdrop();
        
        float modalW = S(280);
        float modalH = S(220);
        float modalX = (SCREEN_WIDTH - modalW) / 2.0f;
        float modalY = (SCREEN_HEIGHT - modalH) / 2.0f;
        
        Rectangle body = UI_DrawModalFrame((Rectangle){modalX, modalY, modalW, modalH}, "SELECT CHANNEL");
        
        const char* chNames[] = { "MASTER", "DECK 1", "DECK 2" };
        float pad = S(12);
        float btnW = modalW - pad * 2;
        float btnH = S(44);
        
        for (int i = 0; i < 3; i++) {
            float bx = modalX + pad;
            float by = modalY + S(45) + i * (btnH + pad);
            Rectangle optRect = { bx, by, btnW, btnH };
            
            Color bg = (b->State->SelectedChannel == i) ? Theme.AccentBlue : Theme.BgPanel;
            if (CheckCollisionPointRec(mouse, optRect)) bg = Theme.TextSecondary;
            
            // Sharp rectangular buttons
            DrawRectangleRec(optRect, bg);
            DrawRectangleLinesEx(optRect, 1.0f, (b->State->SelectedChannel == i) ? Theme.TextPrimary : Theme.BgPanelAlt);
            DrawCentredText(chNames[i], faceMd, optRect.x, optRect.width, optRect.y + (btnH - S(12)) / 2.0f, S(12), Theme.TextPrimary);
        }
    }
}

void BeatFXPanel_Init(BeatFXPanel *b, BeatFXState *state, DeckState *deckA, DeckState *deckB, struct AudioEngine *audioPlugin) {
    b->base.Update = BeatFX_Update;
    b->base.Draw = BeatFX_Draw;
    b->State = state;
    b->DeckA = deckA;
    b->DeckB = deckB;
    b->AudioPlugin = audioPlugin;
    b->FXButton = (Rectangle){0};
}
