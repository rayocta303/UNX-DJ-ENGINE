#include "ui/player/beatfx.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "core/logic/quantize.h"
#include "audio/engine.h"
#include "ui/player/waveform.h"
#include "ui/components/touch_utility.h"
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
    Vector2 mouse = UIGetMousePosition();
    
    float x = BEAT_FX_X;
    float y = TOP_BAR_H;
    float w = BEAT_FX_W;
    float h = WAVE_AREA_H;
    
    // Calculate FX Label and Select hit box
    float fxSelectY = y + S(4) + S(13); // Label + Spacing
    Rectangle fxSelectRect = { x + S(4), fxSelectY, w - S(8), S(18) };
    bool fxHovered = CheckCollisionPointRec(mouse, fxSelectRect);

    // Calculate CH SELECT hit box (following Draw logic)
    float cy = fxSelectY + S(22); // FXSelect + Spacing
    cy += S(10); // Spacing after "CH SELECT" label
    
    Rectangle chRect = { x + S(4), cy, w - S(8), S(14) };
    bool chHovered = CheckCollisionPointRec(mouse, chRect);
    
    // Sync UI State with Engine State (if engine exists)
    if (b->AudioPlugin) {
        b->State->IsFXOn = b->AudioPlugin->BeatFX.isFxOn;
        b->State->SelectedFX = b->AudioPlugin->BeatFX.activeFX;
        b->State->SelectedChannel = b->AudioPlugin->BeatFX.targetChannel;
    }

    if (b->State->FXDropdownOpen) {
        float modalW = S(360);
        float modalH = S(220);
        float modalX = (SCREEN_WIDTH - modalW) / 2.0f;
        float modalY = (SCREEN_HEIGHT - modalH) / 2.0f;
        
        bool clickedOutside = Touch_CheckClick((Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, 0) && !CheckCollisionPointRec(mouse, (Rectangle){modalX, modalY, modalW, modalH});
        
        float cols = 3;
        float rows = 5;
        float pad = S(10);
        float btnW = (modalW - pad * 4) / cols;
        float btnH = (modalH - S(40) - pad * 6) / rows;
        
        for (int i = 0; i < ALL_FX_COUNT; i++) {
            int row = i / (int)cols;
            int col = i % (int)cols;
            float bx = modalX + pad + col * (btnW + pad);
            float by = modalY + S(40) + row * (btnH + pad);
            
            if (Touch_CheckClick((Rectangle){bx, by, btnW, btnH}, S(2))) {
                b->State->SelectedFX = i;
                if (b->AudioPlugin) BeatFXManager_SetFX(&b->AudioPlugin->BeatFX, i);
                b->State->FXDropdownOpen = false;
                UI_ConsumeTouch();
                break;
            }
        }
        
        if (clickedOutside || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
            b->State->FXDropdownOpen = false;
            UI_ConsumeTouch();
        }
        return 0; // Block other interactions
    } else if (b->State->ChannelDropdownOpen) {
        float modalW = S(240);
        float modalH = S(180);
        float modalX = (SCREEN_WIDTH - modalW) / 2.0f;
        float modalY = (SCREEN_HEIGHT - modalH) / 2.0f;
        
        bool clickedOutside = Touch_CheckClick((Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, 0) && !CheckCollisionPointRec(mouse, (Rectangle){modalX, modalY, modalW, modalH});
        
        float pad = S(10);
        float btnW = modalW - pad * 2;
        float btnH = S(36);
        
        for (int i = 0; i < 3; i++) {
            float bx = modalX + pad;
            float by = modalY + S(40) + i * (btnH + pad);
            
            if (Touch_CheckClick((Rectangle){bx, by, btnW, btnH}, S(2))) {
                b->State->SelectedChannel = i;
                if (b->AudioPlugin) b->AudioPlugin->BeatFX.targetChannel = i;
                b->State->ChannelDropdownOpen = false;
                UI_ConsumeTouch();
                break;
            }
        }
        
        if (clickedOutside || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
            b->State->ChannelDropdownOpen = false;
            UI_ConsumeTouch();
        }
        return 0; // Block other interactions
    } else {
        if (fxHovered && UI_IsReleased()) {
            b->State->FXDropdownOpen = true;
        }
    
        if (chHovered) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                int next = b->State->SelectedChannel - (int)wheel;
                if (next < 0) next = 2;
                if (next > 2) next = 0;
                b->State->SelectedChannel = next;
                if (b->AudioPlugin) b->AudioPlugin->BeatFX.targetChannel = next;
            }
            
            if (UI_IsReleased()) {
                b->State->ChannelDropdownOpen = true;
            }
        }
    }
    
    // ... (rest of the logic)
    float containerY = cy + S(20);
    float rowH = S(56) / 4.0f;
    Rectangle qRect = { x + S(4), containerY + 3 * rowH, w - S(8), rowH };
    if (CheckCollisionPointRec(mouse, qRect) && UI_IsReleased()) {
        b->State->Quantize = !b->State->Quantize;
    }

    if (UI_IsReleased()) {
        if (CheckCollisionPointRec(mouse, b->FXButton)) {
            b->State->IsFXOn = !b->State->IsFXOn;
        }
    }

    // 4. Tab Switching Logic
    float tabY = y + h - S(18);
    float tabW = (w - S(10)) / 2;
    Rectangle statusTab = { x + S(4), tabY, tabW, S(14) };
    Rectangle beatFxTab = { x + S(6) + tabW, tabY, tabW, S(14) };

    if (UI_IsReleased()) {
        if (CheckCollisionPointRec(mouse, statusTab)) {
            b->State->ShowBeatFXTab = false;
        } else if (CheckCollisionPointRec(mouse, beatFxTab)) {
            b->State->ShowBeatFXTab = true;
        }
    }

    // Zoom buttons interaction
    float plusMinusY = b->FXButton.y + b->FXButton.height + S(12) + S(18);
    float halfB = (w - S(12)) / 2;
    Rectangle minusRect = { x + S(4), plusMinusY, halfB, S(14) };
    Rectangle plusRect = { x + S(8) + halfB, plusMinusY, halfB, S(14) };

    if (UI_IsReleased()) {
        int zoomDelta = 0;
        if (CheckCollisionPointRec(mouse, minusRect)) zoomDelta = 1;  // Zoom OUT (Show more track)
        if (CheckCollisionPointRec(mouse, plusRect)) zoomDelta = -1; // Zoom IN (Show more detail)

        if (zoomDelta != 0) {
            DeckState* decks[2] = { b->DeckA, b->DeckB };
            for (int d = 0; d < 2; d++) {
                DeckState* ds = decks[d];
                int currentIndex = 0;
                float minDiff = 9999.0f;
                for (int i = 0; i < NUM_ZOOM_LEVELS; i++) {
                    float diff = fabsf(ds->ZoomScale - ZOOM_LEVELS[i]);
                    if (diff < minDiff) {
                        minDiff = diff;
                        currentIndex = i;
                    }
                }
                currentIndex += zoomDelta;
                if (currentIndex < 0) currentIndex = 0;
                if (currentIndex >= NUM_ZOOM_LEVELS) currentIndex = NUM_ZOOM_LEVELS - 1;
                ds->ZoomScale = ZOOM_LEVELS[currentIndex];
            }
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

    DrawRectangle(x, y, w, h, ColorDark2);
    DrawLine(x, y, x, y + h, ColorDark1);

    Font faceXXS = UIFonts_GetFace(S(7));
    Font faceXS = UIFonts_GetFace(S(8));
    Font faceSm = UIFonts_GetFace(S(9));
    Font faceMd = UIFonts_GetFace(S(10));
    Font faceLg = UIFonts_GetFace(S(12));

    float cy = y + S(4);
    DrawCentredText("BEAT FX", faceXS, x, w, cy, S(8), ColorWhite);
    cy += S(13);

    // 1. FX Name Select
    const char* fxName = AllFXNames[b->State->SelectedFX % ALL_FX_COUNT];
    DrawRectangle(x + S(4), cy, w - S(8), S(18), ColorBlack);
    DrawRectangleLines(x + S(4), cy, w - S(8), S(18), ColorDark1);
    DrawCentredText(fxName, faceLg, x + S(4), w - S(8), cy + S(3), S(12), ColorWhite);
    DrawTriangle((Vector2){x + w - S(14), cy + S(7)}, (Vector2){x + w - S(8), cy + S(7)}, (Vector2){x + w - S(11), cy + S(12)}, ColorWhite);
    
    cy += S(22);

    // 2. CH SELECT
    DrawCentredText("CH SELECT", faceXXS, x, w, cy, S(7), ColorShadow);
    cy += S(10);
    
    const char* chName = "MASTER";
    if (b->State->SelectedChannel == 1) chName = "DECK 1";
    if (b->State->SelectedChannel == 2) chName = "DECK 2";
    
    DrawRectangle(x + S(4), cy, w - S(8), S(14), ColorDark3);
    DrawRectangleLines(x + S(4), cy, w - S(8), S(14), ColorDark1);
    DrawCentredText(chName, faceSm, x + S(4), w - S(8), cy + S(2), S(9), ColorWhite);
    DrawTriangle((Vector2){x + w - S(12), cy + S(5)}, (Vector2){x + w - S(6), cy + S(5)}, (Vector2){x + w - S(9), cy + S(10)}, ColorWhite);
    
    cy += S(20);


    // 3. Black parameter container
    float containerH = S(56);
    DrawRectangle(x + S(4), cy, w - S(8), containerH, ColorBlack);
    DrawRectangleLines(x + S(4), cy, w - S(8), containerH, ColorDark1);

    float rowH = containerH / 4.0f;
    for (int i = 1; i < 4; i++) {
        float lx = x + S(4);
        float ly = cy + i * rowH;
        DrawLine(lx, ly, lx + w - S(8), ly, (Color){0x20, 0x20, 0x20, 0xFF});
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
    UIDrawText(bpmStr, faceMd, x + S(8), cy + S(2), S(10), ColorWhite);
    UIDrawText("BPM", faceXXS, x + w - S(26), cy + S(4), S(7), ColorShadow);
    cy += rowH;

    // msec display
    char msStr[16] = "---";
    if (masterBpm > 0.0f) sprintf(msStr, "%.0f", fxMs);
    UIDrawText(msStr, faceSm, x + S(8), cy + S(2), S(9), ColorWhite);
    UIDrawText("msec", faceXXS, x + w - S(26), cy + S(3), S(7), ColorShadow);
    cy += rowH;

    // BEAT display
    int selFX = b->State->SelectedFX;
    if ((selFX == 3 || selFX == 5) && b->State->IsXPadScrubbing) {
        float scrubVal = b->State->XPadScrubValue;
        if (selFX == 3) { // REVERB
            if (scrubVal < -0.05f) {
                UIDrawText("LPF", faceSm, x + S(8), cy + S(2), S(9), ColorOrange);
            } else if (scrubVal > 0.05f) {
                UIDrawText("HPF", faceSm, x + S(8), cy + S(2), S(9), ColorOrange);
            } else {
                UIDrawText("OFF", faceSm, x + S(8), cy + S(2), S(9), ColorWhite);
            }
            UIDrawText("FILTER", faceXXS, x + w - S(32), cy + S(3), S(7), ColorWhite);
        } else { // FLANGER
            char ptStr[16];
            sprintf(ptStr, "%+.0f%%", scrubVal * 100.0f); 
            UIDrawText(ptStr, faceSm, x + S(8), cy + S(2), S(9), ColorOrange);
            UIDrawText("PITCH", faceXXS, x + w - S(30), cy + S(3), S(7), ColorWhite);
        }
    } else {
        UIDrawText(XPadLabels[padIdx], faceSm, x + S(8), cy + S(2), S(9), ColorWhite);
        UIDrawText("BEAT", faceXXS, x + w - S(26), cy + S(3), S(7), ColorShadow);
    }
    cy += rowH;

    // QUANTIZE
    DrawCentredText("QUANTIZE", faceXXS, x + S(4), w - S(8), cy + S(3), S(7), b->State->Quantize ? ColorRed : ColorShadow);
    cy += rowH + S(12);

    // 3.5 FX ON / OFF Toggle
    float btnH = S(16);
    float btnW = w - S(8); 
    
    b->FXButton = (Rectangle){ x + S(4), cy, btnW, btnH };

    bool isFxBlinking = (fmod(GetTime(), 0.5) < 0.25);
    Color btnColor = b->State->IsFXOn ? (isFxBlinking ? (Color){0, 140, 255, 255} : (Color){0, 40, 110, 255}) : ColorDark2;
    DrawRectangleRec(b->FXButton, btnColor);
    DrawRectangleLinesEx(b->FXButton, 1, b->State->IsFXOn ? ColorWhite : ColorShadow);
    
    const char* fxBtnText = b->State->IsFXOn ? "BEAT FX ON" : "BEAT FX OFF";
    DrawCentredText(fxBtnText, faceXXS, b->FXButton.x, b->FXButton.width, b->FXButton.y + S(4.5f), S(7), ColorWhite);
    
    cy += btnH + S(12);

    // 4. ZOOM / GRID
    float halfB = (w - S(12)) / 2;
    DrawRectangle(x + S(4), cy, halfB, S(14), ColorBlue);
    DrawCentredText("ZOOM", faceXXS, x + S(4), halfB, cy + S(3.5f), S(7), ColorWhite);

    DrawRectangle(x + S(8) + halfB, cy, halfB, S(14), ColorDark1);
    DrawRectangleLines(x + S(8) + halfB, cy, halfB, S(14), ColorShadow);
    DrawCentredText("GRID", faceXXS, x + S(8) + halfB, halfB, cy + S(3.5f), S(7), ColorShadow);

    // 4b. +/- BUTTONS (Below Zoom/Grid)
    cy += S(18); // Row height + spacing
    DrawRectangle(x + S(4), cy, halfB, S(14), ColorDark2);
    DrawRectangleLines(x + S(4), cy, halfB, S(14), ColorShadow);
    DrawCentredText("-", faceSm, x + S(4), halfB, cy + S(2.5f), S(9), ColorWhite);

    DrawRectangle(x + S(8) + halfB, cy, halfB, S(14), ColorDark2);
    DrawRectangleLines(x + S(8) + halfB, cy, halfB, S(14), ColorShadow);
    DrawCentredText("+", faceSm, x + S(8) + halfB, halfB, cy + S(2.5f), S(9), ColorWhite);

    cy = y + h - S(18); // Bottom alignment

    // 5. STATUS / BEAT FX tabs
    float tabW = (w - S(10)) / 2;
    
    // STATUS Tab
    bool statusActive = !b->State->ShowBeatFXTab;
    DrawRectangle(x + S(4), cy, tabW, S(14), statusActive ? (Color){0x33, 0x33, 0x33, 0xFF} : (Color){0x15, 0x15, 0x15, 0xFF});
    DrawRectangleLines(x + S(4), cy, tabW, S(14), ColorShadow);
    DrawCentredText("STATUS", faceXXS, x + S(4), tabW, cy + S(3.5f), S(7), statusActive ? ColorWhite : ColorShadow);

    // BEAT FX Tab
    bool beatFxActive = b->State->ShowBeatFXTab;
    DrawRectangle(x + S(6) + tabW, cy, tabW, S(14), beatFxActive ? (Color){0x33, 0x33, 0x33, 0xFF} : (Color){0x15, 0x15, 0x15, 0xFF});
    DrawRectangleLines(x + S(6) + tabW, cy, tabW, S(14), ColorShadow);
    DrawCentredText("BEAT FX", faceXXS, x + S(6) + tabW, tabW, cy + S(3.5f), S(7), beatFxActive ? ColorWhite : ColorShadow);

}

void BeatFXPanel_DrawOverlays(BeatFXPanel *b) {
    if (!b->State->FXDropdownOpen && !b->State->ChannelDropdownOpen) return;
    
    Font faceSm = UIFonts_GetFace(S(9));
    Font faceMd = UIFonts_GetFace(S(10));
    Font faceLg = UIFonts_GetFace(S(12));
    
    Vector2 mouse = UIGetMousePosition();
    if (b->State->FXDropdownOpen) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 200});
        
        float modalW = S(360);
        float modalH = S(220);
        float modalX = (SCREEN_WIDTH - modalW) / 2.0f;
        float modalY = (SCREEN_HEIGHT - modalH) / 2.0f;
        
        DrawRectangleRounded((Rectangle){modalX, modalY, modalW, modalH}, 0.1f, 4, ColorDark1);
        DrawRectangleRoundedLines((Rectangle){modalX, modalY, modalW, modalH}, 0.1f, 4, 1.5f, ColorOrange);
        DrawCentredText("SELECT BEAT FX", faceLg, modalX, modalW, modalY + S(12), S(12), ColorWhite);
        DrawLine(modalX + S(10), modalY + S(30), modalX + modalW - S(10), modalY + S(30), ColorDark2);
        
        float cols = 3;
        float rows = 5;
        float pad = S(10);
        float btnW = (modalW - pad * 4) / cols;
        float btnH = (modalH - S(40) - pad * 6) / rows;
        
        for (int i = 0; i < ALL_FX_COUNT; i++) {
            int row = i / (int)cols;
            int col = i % (int)cols;
            float bx = modalX + pad + col * (btnW + pad);
            float by = modalY + S(40) + row * (btnH + pad);
            Rectangle optRect = { bx, by, btnW, btnH };
            
            Color bg = (b->State->SelectedFX == i) ? ColorBlue : ColorDark2;
            if (CheckCollisionPointRec(mouse, optRect)) bg = ColorGray;
            
            DrawRectangleRounded(optRect, 0.2f, 4, bg);
            DrawRectangleRoundedLines(optRect, 0.2f, 4, 1.0f, ColorShadow);
            DrawCentredText(AllFXNames[i], faceSm, optRect.x, optRect.width, optRect.y + (btnH - S(9)) / 2.0f, S(9), ColorWhite);
        }
    } else if (b->State->ChannelDropdownOpen) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 200});
        
        float modalW = S(240);
        float modalH = S(180);
        float modalX = (SCREEN_WIDTH - modalW) / 2.0f;
        float modalY = (SCREEN_HEIGHT - modalH) / 2.0f;
        
        DrawRectangleRounded((Rectangle){modalX, modalY, modalW, modalH}, 0.1f, 4, ColorDark1);
        DrawRectangleRoundedLines((Rectangle){modalX, modalY, modalW, modalH}, 0.1f, 4, 1.5f, ColorOrange);
        DrawCentredText("SELECT CHANNEL", faceLg, modalX, modalW, modalY + S(12), S(12), ColorWhite);
        DrawLine(modalX + S(10), modalY + S(30), modalX + modalW - S(10), modalY + S(30), ColorDark2);
        
        const char* chNames[] = { "MASTER", "DECK 1", "DECK 2" };
        float pad = S(10);
        float btnW = modalW - pad * 2;
        float btnH = S(36);
        
        for (int i = 0; i < 3; i++) {
            float bx = modalX + pad;
            float by = modalY + S(40) + i * (btnH + pad);
            Rectangle optRect = { bx, by, btnW, btnH };
            
            Color bg = (b->State->SelectedChannel == i) ? ColorBlue : ColorDark2;
            if (CheckCollisionPointRec(mouse, optRect)) bg = ColorGray;
            
            DrawRectangleRounded(optRect, 0.2f, 4, bg);
            DrawRectangleRoundedLines(optRect, 0.2f, 4, 1.0f, ColorShadow);
            DrawCentredText(chNames[i], faceMd, optRect.x, optRect.width, optRect.y + (btnH - S(10)) / 2.0f, S(12), ColorWhite);
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
