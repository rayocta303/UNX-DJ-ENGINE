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
    if (mDown && CheckCollisionPointCircle(mousePos, (Vector2){cx, cy}, r + S(10))) {
        Vector2 delta = GetMouseDelta();
        float range = max - min;
        *val -= (delta.y / S(100.0f)) * range;
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

    // Background Panel
    float wx = S(60);
    float wy = S(40);
    float ww = SCREEN_WIDTH - S(120);
    float wh = SCREEN_HEIGHT - S(80);

    DrawRectangle(wx, wy, ww, wh, ColorDark3);
    DrawRectangleLines(wx, wy, ww, wh, ColorDark1);

    Font fTitle = UIFonts_GetFace(S(16));
    Font fSub = UIFonts_GetFace(S(9));

    // Title
    UIDrawText("MIXER / EQ / COLOR FX", fTitle, wx + S(20), wy + S(15), S(16), ColorWhite);
    DrawLine(wx, wy + S(40), wx + ww, wy + S(40), ColorDark1);

    // Three columns: CH1 (Left), FX Bank (Center), CH2 (Right)
    float colW = ww / 3.0f;
    float ch1X = wx;
    float fxBankX = wx + colW;
    float ch2X = wx + 2.0f * colW;
    float contentY = wy + S(50);
    float contentH = wh - S(50);

    // --- FX BANK (CENTER) ---
    DrawRectangle(fxBankX, contentY, colW, contentH, (Color){15, 15, 15, 255});
    DrawRectangleLines(fxBankX, contentY, colW, contentH, ColorDark1);
    DrawCentredText("SOUND COLOR FX", fSub, fxBankX, colW, contentY + S(10), S(9), ColorWhite);

    float btnW = S(80);
    float btnH = S(30);
    float btnStartX = fxBankX + (colW - btnW * 2 - S(10)) / 2.0f;
    float btnY = contentY + S(40);

    ColorFXType activeFX = eng->Decks[0].ColorFX.activeFX;

    if (DrawFXButton("SPACE", btnStartX, btnY, btnW, btnH, activeFX == COLORFX_SPACE)) activeFX = COLORFX_SPACE;
    if (DrawFXButton("DUB ECHO", btnStartX + btnW + S(10), btnY, btnW, btnH, activeFX == COLORFX_DUBECHO)) activeFX = COLORFX_DUBECHO;
    
    if (DrawFXButton("SWEEP", btnStartX, btnY + btnH + S(10), btnW, btnH, activeFX == COLORFX_SWEEP)) activeFX = COLORFX_SWEEP;
    if (DrawFXButton("NOISE", btnStartX + btnW + S(10), btnY + btnH + S(10), btnW, btnH, activeFX == COLORFX_NOISE)) activeFX = COLORFX_NOISE;
    
    if (DrawFXButton("CRUSH", btnStartX, btnY + btnH * 2 + S(20), btnW, btnH, activeFX == COLORFX_CRUSH)) activeFX = COLORFX_CRUSH;
    if (DrawFXButton("FILTER", btnStartX + btnW + S(10), btnY + btnH * 2 + S(20), btnW, btnH, activeFX == COLORFX_FILTER)) activeFX = COLORFX_FILTER;

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
        
        DrawRectangle(lX, contentY, colW, contentH, (Color){30, 30, 30, 255});
        DrawRectangleLines(lX, contentY, colW, contentH, ColorDark1);

        char chTitle[16];
        sprintf(chTitle, "CH %d", i + 1);
        DrawCentredText(chTitle, fTitle, lX, colW, contentY + S(10), S(16), ColorWhite);

        float cx = lX + colW / 2.0f;
        float ky = contentY + S(60);
        float kH = S(60);
        float kR = S(16);

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

        // COLOR FX KNOB
        ky += kH + S(20);
        HandleKnob(&d->ColorFX.colorValue, cx, ky, kR + S(4), -1.0f, 1.0f, true, mousePos, mDown);
        UIDrawKnob(cx, ky, kR + S(4), d->ColorFX.colorValue, -1.0f, 1.0f, "COLOR", ColorOrange);
    }
}

void MixerRenderer_Init(MixerRenderer *r, MixerState *state) {
    if (!r) return;
    memset(r, 0, sizeof(MixerRenderer));
    r->State = state;
    r->base.Draw = Mixer_Draw;
    r->base.Update = Mixer_Update;
}
