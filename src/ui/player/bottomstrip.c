#include "ui/player/bottomstrip.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"

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
    Font faceSm = UIFonts_GetFace(S(8));

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

void BeatFXSelectBar_Init(BeatFXSelectBar *b, BeatFXState *state) {
    b->base.Update = BottomStrip_Update;
    b->base.Draw = BottomStrip_Draw;
    b->State = state;
}
