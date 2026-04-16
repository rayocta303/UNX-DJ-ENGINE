#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

void DrawScrollbar(float x, float y, float w, float h, int totalItems, int currentPos, int visibleItems) {
    if (totalItems <= visibleItems) {
        return;
    }
    float thumbH = h * (float)visibleItems / (float)totalItems;
    if (thumbH < S(10)) thumbH = S(10);
    
    // Scale currentPos (offset) to the track
    float thumbY = h * (float)currentPos / (float)totalItems;
    
    DrawRectangle(x, y, w, h, ColorDark1);
    DrawRectangle(x, y + thumbY, w, thumbH, ColorWhite);
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

void UIDrawKnob(float x, float y, float radius, float value, float min, float max, const char* unit, Color color) {
    // Background arc
    DrawCircleSectorLines((Vector2){x, y}, radius, 135, 405, 32, ColorShadow);
    
    // Active arc
    float normalized = (value - min) / (max - min);
    if (normalized < 0) normalized = 0;
    if (normalized > 1) normalized = 1;
    
    float endAngle = 135 + (normalized * 270);
    DrawCircleSectorLines((Vector2){x, y}, radius, 135, endAngle, 32, color);
    
    // Needle
    float angleRad = (endAngle - 90) * (M_PI / 180.0f);
    Vector2 needleEnd = {
        x + cosf(angleRad) * radius,
        y + sinf(angleRad) * radius
    };
    DrawLineEx((Vector2){x, y}, needleEnd, 2.0f, color);
    
    // Value / Label text
    const char *finalLabel = unit ? unit : "";
    Font face = UIFonts_GetFace(S(8));
    DrawCentredText(finalLabel, face, x - radius, radius * 2, y + radius + 1, S(8), ColorWhite);
}
