#include "ui/components/fonts.h"
#include "ui/components/theme.h"
#include <stdio.h>

#include "ui/components/assets_bundle.h"
#include "core/logger.h"

static Font defaultFace;
static Font boldFace;
static Font iconSolid;
static Font iconRegular;
static Font iconBrand;

// We define a helper that loads font with full Unicode support if needed.
// For now, we allow raylib to fallback to default loading or we load specific
// ranges.
void UIFonts_Init(void) {
  static bool isInitialized = false;
  if (isInitialized)
    return;
  isInitialized = true;
  UNX_LOG_INFO("[FONTS] UIFonts_Init starting...");

  // Attempt to load standard font. Use Arial on Windows as requested.
  // TODO: Embed a primary UI font to avoid OS dependencies.
  // Attempt to load standard font from assets first, then fallback.
#if defined(PLATFORM_IOS)
  const char *ios_get_bundle_path(const char *filename);
  const char *fontPathBundled =
      ios_get_bundle_path("assets/fonts/Inter-Regular.ttf");
  const char *boldPathBundled =
      ios_get_bundle_path("assets/fonts/Inter-Bold.ttf");
#else
  const char *fontPathBundled = "assets/fonts/Inter-Regular.ttf";
  const char *boldPathBundled = "assets/fonts/Inter-Bold.ttf";
#endif

#if defined(PLATFORM_IOS)
  int fontSize = 40;
#else
  int fontSize = 64;
#endif

  UNX_LOG_INFO("[FONTS] Loading default from: %s", fontPathBundled);
  defaultFace = LoadFontEx(fontPathBundled, fontSize, 0, 0);
  UNX_LOG_INFO("[FONTS] Loading bold from: %s", boldPathBundled);
  boldFace = LoadFontEx(boldPathBundled, fontSize, 0, 0);
  
  UNX_LOG_INFO("[FONTS] Result: Default ID=%u, Bold ID=%u", defaultFace.texture.id, boldFace.texture.id);

  if (defaultFace.texture.id == 0) {
#ifdef _WIN32
    const char *winFont = "C:/Windows/Fonts/arial.ttf";
    defaultFace = LoadFontEx(winFont, 64, 0, 0);
#endif
    if (defaultFace.texture.id == 0) {
      printf("[FONT] Failed to load fonts, using default raylib font\n");
      defaultFace = GetFontDefault();
    }
  }

  if (boldFace.texture.id == 0) {
#ifdef _WIN32
    const char *winBold = "C:/Windows/Fonts/arialbd.ttf";
    boldFace = LoadFontEx(winBold, 64, 0, 0);
#endif
    if (boldFace.texture.id == 0) {
      boldFace = defaultFace; // Fallback to regular if bold fails
    }
  }

  // Common Unicode ranges for Font Awesome 5/6 (PUA range)
  // Most icons are in 0xF000 - 0xF8FF
  UNX_LOG_INFO("[FONTS] Preparing icon codepoints...");
  int codepoints[3000];
  int count = 0;
  for (int i = 32; i < 127; i++)
    codepoints[count++] = i;
  for (int i = 0xF000; i <= 0xF8FF; i++)
    codepoints[count++] = i;

  // UI Symbols (Stars, Triangles, etc)
  codepoints[count++] = 0x2605; // solid star
  codepoints[count++] = 0x2606; // empty star
  codepoints[count++] = 0x25BA; // right triangle
  codepoints[count++] = 0x25C4; // left triangle
  codepoints[count++] = 0x266A; // eighth note
  codepoints[count++] = 0x2022; // bullet

  UNX_LOG_INFO(
      "[FONTS] Loading icon fonts from memory (this may take a moment)...");
  // Font Awesome 5 Solid - Loaded from Memory
  iconSolid =
      LoadFontFromMemory(".otf", font_awesome_solid, font_awesome_solid_size,
                         fontSize, codepoints, count);
  if (iconSolid.texture.id == 0) {
    printf("[FONT] Failed to load solid icon font from memory\n");
  }

  // Font Awesome 5 Regular - Loaded from Memory
  iconRegular = LoadFontFromMemory(".otf", font_awesome_regular,
                                   font_awesome_regular_size, fontSize,
                                   codepoints, count);
  if (iconRegular.texture.id == 0) {
    printf("[FONT] Failed to load regular icon font from memory\n");
  }

  // Font Awesome 5 Brands - Loaded from Memory
  iconBrand =
      LoadFontFromMemory(".otf", font_awesome_brand, font_awesome_brand_size,
                         fontSize, codepoints, count);
  if (iconBrand.texture.id == 0) {
    printf("[FONT] Failed to load brand icon font from memory\n");
  }
  UNX_LOG_INFO("[FONTS] UIFonts_Init completed.");
}

void UIFonts_Unload(void) {
  UnloadFont(defaultFace);
  UnloadFont(boldFace);
  UnloadFont(iconSolid);
  UnloadFont(iconRegular);
  UnloadFont(iconBrand);
}

Font UIFonts_GetFace(float size) {
  (void)size; // Handled directly in DrawTextEx scaling
  return defaultFace;
}

Font UIFonts_GetBoldFace(float size) {
  (void)size;
  return boldFace;
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

void UIDrawText(const char *str, Font font, float x, float y, float size,
                Color clr) {
  if (!str || str[0] == '\0')
    return; // Skip empty text
  Vector2 pos = {x, y};
  DrawTextEx(font, str, pos, size, 1.0f, clr);
}
