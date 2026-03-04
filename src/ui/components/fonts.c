#include "ui/components/fonts.h"
#include "ui/components/theme.h"
#include <stdio.h>

static Font defaultFace;
static Font iconSolid;
static Font iconRegular;
static Font iconBrand;

// We define a helper that loads font with full Unicode support if needed.
// For now, we allow raylib to fallback to default loading or we load specific ranges.
void UIFonts_Init(void) {
    // Attempt to load standard font. Use Arial on Windows as requested.
    const char* fontPath = UI_BoldEnabled ? "C:/Windows/Fonts/arialbd.ttf" : "C:/Windows/Fonts/arial.ttf";
    defaultFace = LoadFontEx(fontPath, 64, 0, 0); 
    if (defaultFace.texture.id == 0) {
        printf("[FONT] Failed to load %s, trying regular arial.ttf\n", fontPath);
        defaultFace = LoadFontEx("C:/Windows/Fonts/arial.ttf", 64, 0, 0); 
        if (defaultFace.texture.id == 0) defaultFace = GetFontDefault();
    }

    // Common Unicode ranges for Font Awesome 5/6 (PUA range)
    // Most icons are in 0xF000 - 0xF8FF
    int codepoints[3000];
    int count = 0;
    for (int i = 32; i < 127; i++) codepoints[count++] = i;
    for (int i = 0xF000; i <= 0xF8FF; i++) codepoints[count++] = i;

    // Font Awesome 5 Solid
    iconSolid = LoadFontEx("assets/fonts/otfs/Font Awesome 5 Free-Solid-900.otf", 64, codepoints, count);
    if (iconSolid.texture.id == 0) {
        iconSolid = LoadFontEx("assets/fonts/Font Awesome 6 Free-Solid-900.otf", 64, codepoints, count);
        if (iconSolid.texture.id == 0) printf("[FONT] Failed to load solid icon font\n");
    }

    // Font Awesome 5 Regular
    iconRegular = LoadFontEx("assets/fonts/otfs/Font Awesome 5 Free-Regular-400.otf", 64, codepoints, count);
    if (iconRegular.texture.id == 0) {
        printf("[FONT] Failed to load regular icon font\n");
    }

    // Font Awesome 5 Brands
    iconBrand = LoadFontEx("assets/fonts/otfs/Font Awesome 5 Brands-Regular-400.otf", 64, codepoints, count);
    if (iconBrand.texture.id == 0) {
        printf("[FONT] Failed to load brand icon font\n");
    }
}

void UIFonts_Unload(void) {
    UnloadFont(defaultFace);
    UnloadFont(iconSolid);
    UnloadFont(iconRegular);
    UnloadFont(iconBrand);
}

Font UIFonts_GetFace(float size) {
    (void)size; // Handled directly in DrawTextEx scaling
    return defaultFace;
}

Font UIFonts_GetIcon(float size) {
    (void)size;
    return iconSolid;
}

Font UIFonts_GetIconRegular(float size) {
    (void)size;
    return iconRegular;
}

Font UIFonts_GetIconBrand(float size) {
    (void)size;
    return iconBrand;
}

void UIDrawText(const char* str, Font font, float x, float y, float size, Color clr) {
    Vector2 pos = { x, y };
    DrawTextEx(font, str, pos, size, 1.0f, clr); 
}
