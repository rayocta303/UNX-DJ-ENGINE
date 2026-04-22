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

void DrawSelectionTriangleEx(float x, float y, float size, int direction, Color col) {
    if (direction == 0) { // Right
        DrawLine(x, y, x, y + size, col);
        DrawLine(x, y, x + size / 2.0f, y + size / 2.0f, col);
        DrawLine(x, y + size, x + size / 2.0f, y + size / 2.0f, col);
    } else { // Left
        DrawLine(x + size / 2.0f, y, x + size / 2.0f, y + size, col);
        DrawLine(x + size / 2.0f, y, x, y + size / 2.0f, col);
        DrawLine(x + size / 2.0f, y + size, x, y + size / 2.0f, col);
    }
}

void DrawSelectionTriangle(float x, float y, Color col) {
    DrawSelectionTriangleEx(x, y, 10.0f, 0, col);
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
    float normalized = (value - min) / (max - min);
    if (normalized < 0) normalized = 0;
    if (normalized > 1) normalized = 1;

    // 1. Background Track (Groove)
    // Draw a dark track where the knob "sits"
    DrawCircleV((Vector2){x, y}, radius + S(3), ColorDark1);
    DrawRing((Vector2){x, y}, radius + S(1), radius + S(4), 135, 405, 32, Fade(BLACK, 0.4f));

    // 2. Progress Arc (The colored part)
    float startAngle = 135;
    float endAngle = 135 + (normalized * 270);
    
    // Draw the active part of the ring
    DrawRing((Vector2){x, y}, radius + S(1.5f), radius + S(3.5f), startAngle, endAngle, 36, color);
    // Subtle glow for the active part
    DrawRing((Vector2){x, y}, radius + S(1.0f), radius + S(4.0f), startAngle, endAngle, 36, Fade(color, 0.2f));

    // 3. Knob Body (3D cylindrical look)
    // Dark to light gradient to simulate lighting from top-left
    DrawCircleGradient((int)x, (int)y, radius, ColorDark2, ColorDark3);
    DrawCircleLines((int)x, (int)y, radius, Fade(WHITE, 0.1f));
    
    // Outer rim highlight
    DrawRing((Vector2){x, y}, radius - S(1), radius, 0, 360, 32, Fade(WHITE, 0.05f));

    // 4. Indicator (Modern Dot)
    float angleRad = (endAngle - 90) * (PI / 180.0f);
    Vector2 dotPos = {
        x + cosf(angleRad) * (radius - S(5)),
        y + sinf(angleRad) * (radius - S(5))
    };
    // Draw the dot indicator
    DrawCircleV(dotPos, S(2), ColorWhite);
    // Dot glow
    DrawCircleV(dotPos, S(3.5f), Fade(WHITE, 0.2f));

    // 5. Label text (below knob)
    if (unit && unit[0] != '\0') {
        Font face = UIFonts_GetFace(S(8));
        DrawCentredText(unit, face, x - radius, radius * 2, y + radius + S(6), S(8), ColorShadow);
    }
}
