#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include <stdio.h>

void DrawTopBar(int remainMin, int remainSec, int clockMin, int clockSec, bool showInfo) {
    DrawRectangle(0, 0, SCREEN_WIDTH, TOP_BAR_H, ColorDark2);
    DrawLine(0, TOP_BAR_H, SCREEN_WIDTH, TOP_BAR_H, ColorDark1);

    Font faceXS = UIFonts_GetFace(S(8));
    Font faceSm = UIFonts_GetFace(S(9));

    // Remain label
    UIDrawText("REMAIN", faceXS, 4, 3, S(8), ColorShadow);

    // Remain time value
    char remainStr[32];
    sprintf(remainStr, "%02d:%02d:00", remainMin, remainSec);
    UIDrawText(remainStr, faceSm, 36, 2, S(9), ColorPaper);

    // Clock
    char clockStr[32];
    sprintf(clockStr, "%02d:%02d:00", clockMin, clockSec);
    UIDrawText(clockStr, faceSm, 330, 2, S(9), ColorPaper);

    // INFO button
    Color infoBg = showInfo ? ColorGray : ColorDark1;
    DrawRectangle(440, 1, 38, 15, infoBg);
    DrawRectangleLines(440, 1, 38, 15, ColorShadow);
    UIDrawText("INFO", faceXS, 449, 2, S(8), ColorPaper);
}

void DrawSelectionTriangle(float x, float y, Color col) {
    DrawLine(x, y, x, y+10, col);
    DrawLine(x, y, x+5, y+5, col);
    DrawLine(x, y+10, x+5, y+5, col);
}

void DrawScrollbar(int totalItems, int currentPos, int visibleItems) {
    if (totalItems <= visibleItems) {
        return;
    }
    float trackH = 240.0f;
    float thumbH = trackH * (float)visibleItems / (float)totalItems;
    if (thumbH < 5.0f) {
        thumbH = 5.0f;
    }
    float thumbY = trackH * (float)currentPos / (float)totalItems;
    DrawRectangle(3, 20, 3, trackH, ColorDark1);
    DrawRectangle(3, 20 + thumbY, 3, thumbH, ColorPaper);
}

void DrawBadge(float x, float y, float w, float h, Color bg, Color textClr, const char* label) {
    DrawRectangle(x, y, w, h, bg);
    Font face = UIFonts_GetFace(S(7));
    UIDrawText(label, face, x+2, y+1, S(7), textClr);
}

void DrawCentredText(const char* str, Font font, float padX, float width, float y, float fontSize, Color clr) {
    Vector2 size = MeasureTextEx(font, str, fontSize, 1.0f);
    float x = padX + (width - size.x) / 2.0f;
    UIDrawText(str, font, x, y, fontSize, clr);
}
