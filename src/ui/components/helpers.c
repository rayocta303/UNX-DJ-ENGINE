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
    float hSize = size / 2.0f;
    if (direction == 0) { // Right
        DrawLine(x, y - hSize, x, y + hSize, col);
        DrawLine(x, y - hSize, x + hSize, y, col);
        DrawLine(x, y + hSize, x + hSize, y, col);
    } else { // Left
        DrawLine(x + hSize, y - hSize, x + hSize, y + hSize, col);
        DrawLine(x + hSize, y - hSize, x, y, col);
        DrawLine(x + hSize, y + hSize, x, y, col);
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

void UIDrawKnob(float x, float y, float radius, float value, float min, float max, const char* unit, Color color, bool centerZero) {
    float normalized = (value - min) / (max - min);
    if (normalized < 0) normalized = 0;
    if (normalized > 1) normalized = 1;

    // 1. Background Track (Groove) - Thicker and more distinct
    DrawRing((Vector2){x, y}, radius + S(1), radius + S(4.5f), 135, 405, 32, ColorDark1);
    DrawRing((Vector2){x, y}, radius + S(1.5f), radius + S(4.0f), 135, 405, 32, ColorBlack);

    // 2. Progress Arc
    float startAngle, endAngle;
    if (centerZero) {
        float mid = (min + max) / 2.0f;
        startAngle = 270.0f; // Top
        if (value >= mid) {
            float range = max - mid;
            float t = (value - mid) / range;
            endAngle = 270.0f + (t * 135.0f);
        } else {
            float range = mid - min;
            float t = (mid - value) / range;
            endAngle = 270.0f - (t * 135.0f);
            // Swap for DrawRing correctly
            float tmp = startAngle;
            startAngle = endAngle;
            endAngle = tmp;
        }
    } else {
        startAngle = 135.0f;
        endAngle = 135.0f + (normalized * 270.0f);
    }

    // Main Progress Ring
    DrawRing((Vector2){x, y}, radius + S(1.5f), radius + S(4.0f), startAngle, endAngle, 36, color);
    // Subtle glow/inner shine
    DrawRing((Vector2){x, y}, radius + S(2.5f), radius + S(3.0f), startAngle, endAngle, 36, Fade(WHITE, 0.3f));

    // 3. Knob Body (3D look)
    DrawCircleGradient((int)x, (int)y, radius, ColorDark2, ColorDark3);
    DrawCircleLines((int)x, (int)y, radius, Fade(WHITE, 0.1f));
    DrawRing((Vector2){x, y}, radius - S(1.5f), radius, 0, 360, 32, Fade(WHITE, 0.05f));

    // 4. Indicator (Professional thick bar)
    float indicatorAngle = 135.0f + (normalized * 270.0f);
    float angleRad = indicatorAngle * (PI / 180.0f);
    Vector2 needleStart = {
        x + cosf(angleRad) * (radius * 0.4f),
        y + sinf(angleRad) * (radius * 0.4f)
    };
    Vector2 needleEnd = {
        x + cosf(angleRad) * (radius - S(0.5f)),
        y + sinf(angleRad) * (radius - S(0.5f))
    };
    DrawLineEx(needleStart, needleEnd, S(4.5f), ColorWhite);
    // Indicator cap
    DrawCircleV(needleEnd, S(1.8f), ColorWhite);

    // 5. Label text (Adjusted position and color to match premium UI)
    if (unit && unit[0] != '\0') {
        Font face = UIFonts_GetFace(S(9));
        DrawCentredText(unit, face, x - radius * 2.0f, radius * 4.0f, y + radius + S(12), S(9), Fade(ColorWhite, 0.7f));
    }
}
