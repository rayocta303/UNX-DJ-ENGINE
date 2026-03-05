#include "ui/player/beatfx.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>

static int BeatFX_Update(Component *base) {
    BeatFXPanel *b = (BeatFXPanel *)base;
    Vector2 mouse = UIGetMousePosition();
    
    float x = BEAT_FX_X;
    float y = TOP_BAR_H;
    float w = BEAT_FX_W;
    
    // Calculate CH SELECT hit box (following Draw logic)
    float cy = y + S(4) + S(13) + S(22); // Label + FXSelect + Spacing
    cy += S(10); // Spacing after "CH SELECT" label
    
    Rectangle chRect = { x + S(4), cy, w - S(8), S(14) };
    bool hovered = CheckCollisionPointRec(mouse, chRect);
    
    if (b->State->ChannelDropdownOpen) {
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
        if (hovered) {
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
    const char* name = "DELAY"; // Mock state for now
    DrawRectangle(x + S(4), cy, w - S(8), S(18), ColorBlack);
    DrawRectangleLines(x + S(4), cy, w - S(8), S(18), ColorDark1);
    DrawCentredText(name, faceLg, x + S(4), w - S(8), cy + S(3), S(12), ColorWhite);
    cy += S(22);

    // 2. CH SELECT
    DrawCentredText("CH SELECT", faceXXS, x, w, cy, S(7), ColorShadow);
    cy += S(10);
    
    const char* chNames[] = { "MASTER", "DECK 1", "DECK 2" };
    DrawRectangle(x + S(4), cy, w - S(8), S(14), ColorBlue);
    DrawCentredText(chNames[b->State->SelectedChannel % 3], faceSm, x + S(4), w - S(8), cy + S(2), S(9), ColorWhite);
    
    // Dropdown arrow
    DrawTriangle((Vector2){x + w - S(12), cy + S(5)}, (Vector2){x + w - S(8), cy + S(5)}, (Vector2){x + w - S(10), cy + S(9)}, ColorWhite);
    
    float dropdownY = cy; // Save for later drawing overlay
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

    // BPM
    UIDrawText("120.0", faceMd, x + S(8), cy + S(2), S(10), ColorWhite);
    UIDrawText("BPM", faceXXS, x + w - S(26), cy + S(4), S(7), ColorShadow);
    cy += rowH;

    // msec
    UIDrawText("500", faceSm, x + S(8), cy + S(2), S(9), ColorWhite);
    UIDrawText("msec", faceXXS, x + w - S(26), cy + S(3), S(7), ColorShadow);
    cy += rowH;

    // BEAT
    UIDrawText("1", faceSm, x + S(8), cy + S(2), S(9), ColorWhite);
    UIDrawText("BEAT", faceXXS, x + w - S(26), cy + S(3), S(7), ColorShadow);
    cy += rowH;

    // QUANTIZE
    DrawCentredText("QUANTIZE", faceXXS, x + S(4), w - S(8), cy + S(3), S(7), ColorShadow);
    cy += rowH + S(12);

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
    
    // STATUS (Active mock)
    DrawRectangle(x + S(4), cy, tabW, S(14), (Color){0x33, 0x33, 0x33, 0xFF});
    DrawRectangleLines(x + S(4), cy, tabW, S(14), ColorShadow);
    DrawCentredText("STATUS", faceXXS, x + S(4), tabW, cy + S(3.5f), S(7), ColorWhite);

    // BEAT FX (Inactive mock)
    DrawRectangle(x + S(6) + tabW, cy, tabW, S(14), (Color){0x15, 0x15, 0x15, 0xFF});
    DrawRectangleLines(x + S(6) + tabW, cy, tabW, S(14), ColorShadow);
    DrawCentredText("BEAT FX", faceXXS, x + S(6) + tabW, tabW, cy + S(3.5f), S(7), ColorShadow);

    // --- OVERLAYS (Draw at bottom for highest Z-index) ---
    if (b->State->ChannelDropdownOpen) {
        const char* chNames[] = { "MASTER", "DECK 1", "DECK 2" };
        float dy = dropdownY + S(14);
        Vector2 mouse = UIGetMousePosition();
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

void BeatFXPanel_Init(BeatFXPanel *b, BeatFXState *state) {
    b->base.Update = BeatFX_Update;
    b->base.Draw = BeatFX_Draw;
    b->State = state;
}
