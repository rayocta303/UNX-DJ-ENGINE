#pragma once

#include "raylib.h"

// CustomFont definition (if using baked fonts)
typedef struct {
    Texture2D image;
    int width;
    int height;
    int offset;
} CustomFont;

void UIFonts_Init(void);
void UIFonts_Unload(void);

// Returns standard raylib font configured for default face
Font UIFonts_GetFace(float size);
// Returns bold version of the UI face
Font UIFonts_GetBoldFace(float size);
// Returns standard raylib font configured for solid icon face
Font UIFonts_GetIcon(float size);
// Returns standard raylib font configured for regular icon face
Font UIFonts_GetIconRegular(float size);
// Returns standard raylib font configured for brand icon face
Font UIFonts_GetIconBrand(float size);

// Helper for drawing colored text
void UIDrawText(const char* str, Font font, float x, float y, float size, Color clr);
