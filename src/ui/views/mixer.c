#include "mixer.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>
#include <string.h>

static int Mixer_Update(Component *base) {
    (void)base;
    return 0;
}

// Helper to draw a clickable Color FX button
static bool DrawFXButton(const char *label, float x, float y, float w, float h, bool active) {
    bool hovered = CheckCollisionPointRec(UIGetMousePosition(), (Rectangle){x, y, w, h});
    bool pressed = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Color bg = active ? ColorOrange : (hovered ? ColorDark1 : ColorBlack);
    Color fg = active ? ColorBlack : ColorOrange;
    DrawRectangle(x, y, w, h, bg);
    DrawRectangleLines(x, y, w, h, ColorOrange);
    Font f = UIFonts_GetFace(S(9));
    DrawCentredText(label, f, x, w, y + (h - S(9)) / 2.0f, S(9), fg);
    return pressed;
}

static void HandleKnob(float *val, float cx, float cy, float r, float min, float max, bool centerZero, Vector2 mousePos, bool mDown) {
    bool hovered = CheckCollisionPointCircle(mousePos, (Vector2){cx, cy}, r + S(10));
    if (mDown && hovered) {
        Vector2 delta = GetMouseDelta();
        float range = max - min;
        *val -= (delta.y / S(100.0f)) * range;
        if (*val < min) *val = min;
        if (*val > max) *val = max;
    }
    
    // Scroll Wheel Support
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && hovered) {
        float range = max - min;
        *val += (wheel * 0.05f) * range; // 5% step per scroll tick
        if (*val < min) *val = min;
        if (*val > max) *val = max;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointCircle(mousePos, (Vector2){cx, cy}, r)) {
         static float lastClickTime = 0;
         if (GetTime() - lastClickTime < 0.3) {
             *val = centerZero ? (min + (max - min)/2.0f) : min;
         }
         lastClickTime = GetTime();
    }
}

static void Mixer_Draw(Component *base) {
    MixerRenderer *r = (MixerRenderer *)base;
    if (!r->State->IsActive || r->State->AudioPlugin == NULL) return;

    AudioEngine *eng = r->State->AudioPlugin;

    // Compact Professional Mixer Layout
    float ww = S(360);
    float wh = S(320);
    float wx = (SCREEN_WIDTH - ww) / 2.0f;
    float wy = (SCREEN_HEIGHT - wh) / 2.0f + S(20);

    // Main Panel Background
    DrawRectangle(wx, wy, ww, wh, (Color){ 20, 20, 20, 240 });
    DrawRectangleLines(wx, wy, ww, wh, ColorDark1);

    Font fTitle = UIFonts_GetFace(S(12));
    Font fSub = UIFonts_GetFace(S(8));
    Font fXXS = UIFonts_GetFace(S(7));

    // Title Bar
    DrawRectangle(wx, wy, ww, S(24), ColorDark3);
    DrawCentredText("MIXER & SOUND COLOR FX", fTitle, wx, ww, wy + S(5), S(12), ColorWhite);
    DrawLine(wx, wy + S(24), wx + ww, wy + S(24), ColorDark1);

    float contentY = wy + S(25);
    float contentH = wh - S(25);

    float chW = S(80);
    float fxW = ww - (chW * 2);
    
    float ch1X = wx;
    float fxX = wx + chW;
    float ch2X = fxX + fxW;

    // --- FX BANK (CENTER) ---
    DrawRectangle(fxX, contentY, fxW, contentH, (Color){ 10, 10, 10, 255 });
    DrawRectangleLines(fxX, contentY, fxW, contentH, ColorDark1);
    
    DrawRectangle(fxX, contentY, fxW, S(16), ColorDark2);
    DrawCentredText("BEAT FX / COLOR", fSub, fxX, fxW, contentY + S(3), S(8), ColorOrange);

    float btnW = S(65);
    float btnH = S(24);
    float btnGapX = S(10);
    float btnGapY = S(12);
    float btnStartX = fxX + (fxW - (btnW * 2 + btnGapX)) / 2.0f;
    float btnY = contentY + S(30);

    ColorFXType activeFX = eng->Decks[0].ColorFX.activeFX;

    if (DrawFXButton("SPACE", btnStartX, btnY, btnW, btnH, activeFX == COLORFX_SPACE)) activeFX = COLORFX_SPACE;
    if (DrawFXButton("DUB ECHO", btnStartX + btnW + btnGapX, btnY, btnW, btnH, activeFX == COLORFX_DUBECHO)) activeFX = COLORFX_DUBECHO;
    
    btnY += btnH + btnGapY;
    if (DrawFXButton("SWEEP", btnStartX, btnY, btnW, btnH, activeFX == COLORFX_SWEEP)) activeFX = COLORFX_SWEEP;
    if (DrawFXButton("NOISE", btnStartX + btnW + btnGapX, btnY, btnW, btnH, activeFX == COLORFX_NOISE)) activeFX = COLORFX_NOISE;
    
    btnY += btnH + btnGapY;
    if (DrawFXButton("CRUSH", btnStartX, btnY, btnW, btnH, activeFX == COLORFX_CRUSH)) activeFX = COLORFX_CRUSH;
    if (DrawFXButton("FILTER", btnStartX + btnW + btnGapX, btnY, btnW, btnH, activeFX == COLORFX_FILTER)) activeFX = COLORFX_FILTER;

    if (activeFX != eng->Decks[0].ColorFX.activeFX) {
        ColorFXManager_SetFX(&eng->Decks[0].ColorFX, activeFX);
        ColorFXManager_SetFX(&eng->Decks[1].ColorFX, activeFX);
    }

    Vector2 mousePos = UIGetMousePosition();
    bool mDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    // --- CHANNELS ---
    for (int i = 0; i < 2; i++) {
        DeckAudioState *d = &eng->Decks[i];
        float lX = (i == 0) ? ch1X : ch2X;
        
        // Channel strip background
        DrawRectangle(lX, contentY, chW, contentH, (Color){ 25, 25, 25, 255 });
        DrawRectangleLines(lX, contentY, chW, contentH, ColorDark1);

        char chTitle[16];
        sprintf(chTitle, "CH %d", i + 1);
        DrawRectangle(lX, contentY, chW, S(16), ColorDark2);
        DrawCentredText(chTitle, fSub, lX, chW, contentY + S(3), S(8), ColorWhite);

        float cx = lX + chW / 2.0f;
        float ky = contentY + S(35);
        float kH = S(45); // Tighter vertical spacing
        float kR = S(14); // Slightly smaller EQ knobs

        // TRIM
        HandleKnob(&d->Trim, cx, ky, kR, 0.0f, 2.0f, false, mousePos, mDown);
        UIDrawKnob(cx, ky, kR, d->Trim, 0.0f, 2.0f, "TRIM", ColorPaper);
        
        ky += kH;
        // HI
        HandleKnob(&d->EqHigh, cx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
        UIDrawKnob(cx, ky, kR, d->EqHigh, 0.0f, 1.0f, "HI", ColorWhite);
        
        // MID
        ky += kH;
        HandleKnob(&d->EqMid, cx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
        UIDrawKnob(cx, ky, kR, d->EqMid, 0.0f, 1.0f, "MID", ColorWhite);
        
        // LOW
        ky += kH;
        HandleKnob(&d->EqLow, cx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
        UIDrawKnob(cx, ky, kR, d->EqLow, 0.0f, 1.0f, "LOW", ColorWhite);

        // COLOR FX KNOB (Larger and distinctive)
        float colorKR = S(18);
        ky += kH + S(5);
        
        // Draw a dark enclosing box for the Color FX knob 
        DrawRectangle(lX + S(5), ky - colorKR - S(8), chW - S(10), colorKR * 2 + S(22), (Color){ 15, 15, 15, 255 });
        DrawRectangleLines(lX + S(5), ky - colorKR - S(8), chW - S(10), colorKR * 2 + S(22), ColorDark1);
        
        HandleKnob(&d->ColorFX.colorValue, cx, ky, colorKR, -1.0f, 1.0f, true, mousePos, mDown);
        UIDrawKnob(cx, ky, colorKR, d->ColorFX.colorValue, -1.0f, 1.0f, "COLOR", ColorOrange);
    }
}

void MixerRenderer_Init(MixerRenderer *r, MixerState *state) {
    if (!r) return;
    memset(r, 0, sizeof(MixerRenderer));
    r->State = state;
    r->base.Draw = Mixer_Draw;
    r->base.Update = Mixer_Update;
}
