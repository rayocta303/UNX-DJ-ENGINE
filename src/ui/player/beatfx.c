#include "ui/player/beatfx.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "logic/quantize.h"
#include <stdio.h>

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
    
    if (b->State->FXDropdownOpen) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float dy = fxSelectY + S(18);
            for (int i = 0; i < ALL_FX_COUNT; i++) {
                Rectangle optRect = { x + S(4), dy, w - S(8), S(14) };
                if (CheckCollisionPointRec(mouse, optRect)) {
                    b->State->SelectedFX = i;
                    break;
                }
                dy += S(14);
            }
            b->State->FXDropdownOpen = false;
        }
    } else if (b->State->ChannelDropdownOpen) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float dy = cy + S(14);
            for (int i = 0; i < 3; i++) {
                Rectangle optRect = { x + S(4), dy, w - S(8), S(14) };
                if (CheckCollisionPointRec(mouse, optRect)) {
                    b->State->SelectedChannel = i;
                    break;
                }
                dy += S(14);
            }
            b->State->ChannelDropdownOpen = false;
        }
    } else {
        if (fxHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // Cycle on scroll can be added later if needed
            b->State->FXDropdownOpen = true;
        }
    
        if (chHovered) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                // Cycle through 0 (Master), 1 (Deck 1), 2 (Deck 2)
                int next = b->State->SelectedChannel - (int)wheel;
                if (next < 0) next = 2;
                if (next > 2) next = 0;
                b->State->SelectedChannel = next;
            }
            
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                b->State->ChannelDropdownOpen = true;
            }
        }
    }
    
    // 3. Black parameter container hitboxes
    float containerY = cy + S(20);
    float rowH = S(56) / 4.0f;
    Rectangle qRect = { x + S(4), containerY + 3 * rowH, w - S(8), rowH };
    if (CheckCollisionPointRec(mouse, qRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        b->State->Quantize = !b->State->Quantize;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, b->FXButton)) {
            b->State->IsFXOn = !b->State->IsFXOn;
        }
    }

    // 4. Tab Switching Logic
    float tabY = y + h - S(18);
    float tabW = (w - S(10)) / 2;
    Rectangle statusTab = { x + S(4), tabY, tabW, S(14) };
    Rectangle beatFxTab = { x + S(6) + tabW, tabY, tabW, S(14) };

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, statusTab)) {
            b->State->ShowBeatFXTab = false;
        } else if (CheckCollisionPointRec(mouse, beatFxTab)) {
            b->State->ShowBeatFXTab = true;
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
    
    float fxDropdownY = cy + S(18); // Save for later drawing overlay
    
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
    
    float dropdownY = cy; // Save for drawing overlay
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

    Color btnColor = b->State->IsFXOn ? ColorOrange : ColorDark2;
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

    // --- OVERLAYS (Draw at bottom for highest Z-index) ---
    Vector2 mouse = UIGetMousePosition();
    if (b->State->FXDropdownOpen) {
        float dy = fxDropdownY;
        for (int i = 0; i < ALL_FX_COUNT; i++) {
            Rectangle optRect = { x + S(4), dy, w - S(8), S(14) };
            Color bg = (b->State->SelectedFX == i) ? ColorBlue : ColorDark3;
            if (CheckCollisionPointRec(mouse, optRect)) bg = ColorGray;
            
            DrawRectangleRec(optRect, bg);
            DrawRectangleLinesEx(optRect, 1, ColorDark1);
            DrawCentredText(AllFXNames[i], faceSm, optRect.x, optRect.width, optRect.y + S(2), S(9), ColorWhite);
            dy += S(14);
        }
    } else if (b->State->ChannelDropdownOpen) {
        const char* chNames[] = { "MASTER", "DECK 1", "DECK 2" };
        float dy = dropdownY + S(14);
        for (int i = 0; i < 3; i++) {
            Rectangle optRect = { x + S(4), dy, w - S(8), S(14) };
            Color bg = (b->State->SelectedChannel == i) ? ColorBlue : ColorDark3;
            if (CheckCollisionPointRec(mouse, optRect)) bg = ColorGray;
            
            DrawRectangleRec(optRect, bg);
            DrawRectangleLinesEx(optRect, 1, ColorDark1);
            DrawCentredText(chNames[i], faceSm, optRect.x, optRect.width, optRect.y + S(2), S(9), ColorWhite);
            dy += S(14);
        }
    }
}

void BeatFXPanel_Init(BeatFXPanel *b, BeatFXState *state, DeckState *deckA, DeckState *deckB) {
    b->base.Update = BeatFX_Update;
    b->base.Draw = BeatFX_Draw;
    b->State = state;
    b->DeckA = deckA;
    b->DeckB = deckB;
    b->FXButton = (Rectangle){0};
}
