#pragma once

#include "raylib.h"
#include <stdbool.h>

void DrawTopBar(int remainMin, int remainSec, int clockMin, int clockSec, bool showInfo);
void DrawSelectionTriangle(float x, float y, Color col);
void DrawScrollbar(int totalItems, int currentPos, int visibleItems);
void DrawBadge(float x, float y, float w, float h, Color bg, Color textClr, const char* label);
void DrawCentredText(const char* str, Font font, float padX, float width, float y, float fontSize, Color clr);
