#pragma once

#include "raylib.h"
#include <stdbool.h>

void DrawTopBar(int remainMin, int remainSec, int clockMin, int clockSec, bool showInfo);
void DrawSelectionTriangle(float x, float y, Color col);
void DrawSelectionTriangleEx(float x, float y, float size, int direction, Color col);
void DrawScrollbar(float x, float y, float w, float h, int totalItems, int currentPos, int visibleItems);
void DrawBadge(float x, float y, float w, float h, Color bg, Color textClr, const char* label);
void DrawCentredText(const char* str, Font font, float padX, float width, float y, float fontSize, Color clr);
void UIDrawTextTruncated(const char* str, Font font, float x, float y, float size, Color clr, float maxWidth);
void UIDrawText(const char* str, Font font, float x, float y, float size, Color clr);
void UIDrawKnob(float x, float y, float radius, float value, float min, float max, const char* unit, Color color, bool centerZero);
void UIDrawScrollingText(const char* str, Font font, Rectangle rect, float fontSize, Color clr, float elapsedTime);
void Toast_Show(const char *message, float durationSec, Color badgeColor);
void Toast_ShowError(const char *message);
void Toast_UpdateAndDraw(float dt);
void GetDynamicKey(const char* originalKey, float tempoPercent, bool isMasterTempoOn, char* outKey);
Color GetCamelotColor(const char* keyStr);

// Modal Helpers
void UI_DrawModalBackdrop(void);
Rectangle UI_DrawModalFrame(Rectangle modalRect, const char* title);
bool UI_UpdateModal(Rectangle modalRect);
