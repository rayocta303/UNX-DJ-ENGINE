#include "ui/components/fonts.h"
#include "ui/components/theme.h"
#include <stdio.h>
#include <string.h>

#include "ui/components/assets_bundle.h"
#include "core/logger.h"

static Font defaultFace;
static Font boldFace;
static Font timeFace;
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

#if defined(PLATFORM_IOS)
  int fontSize = 40;
#else
  int fontSize = 64;
#endif

  // 1. Default UI Face (Nexa Heavy)
  const char *pathDefault = NULL;
  if (FileExists("assets/fonts/nexa/Nexa-Heavy.ttf")) pathDefault = "assets/fonts/nexa/Nexa-Heavy.ttf";
  else if (FileExists("../assets/fonts/nexa/Nexa-Heavy.ttf")) pathDefault = "../assets/fonts/nexa/Nexa-Heavy.ttf";

  if (pathDefault) {
    UNX_LOG_INFO("[FONTS] Loading default (Nexa Heavy) from file: %s", pathDefault);
    defaultFace = LoadFontEx(pathDefault, fontSize, 0, 0);
  }
  if (defaultFace.texture.id == 0) {
    UNX_LOG_INFO("[FONTS] Loading default (Nexa Heavy) from memory bundle...");
    defaultFace = LoadFontFromMemory(".ttf", font_nexa_heavy, font_nexa_heavy_size, fontSize, 0, 0);
  }

  // 2. Bold UI Face (Nexa Heavy)
  const char *pathBold = NULL;
  if (FileExists("assets/fonts/nexa/Nexa-Heavy.ttf")) pathBold = "assets/fonts/nexa/Nexa-Heavy.ttf";
  else if (FileExists("../assets/fonts/nexa/Nexa-Heavy.ttf")) pathBold = "../assets/fonts/nexa/Nexa-Heavy.ttf";

  if (pathBold) {
    UNX_LOG_INFO("[FONTS] Loading bold from file: %s", pathBold);
    boldFace = LoadFontEx(pathBold, fontSize, 0, 0);
  }
  if (boldFace.texture.id == 0) {
    UNX_LOG_INFO("[FONTS] Loading bold (Nexa Heavy) from memory bundle...");
    boldFace = LoadFontFromMemory(".ttf", font_nexa_heavy, font_nexa_heavy_size, fontSize, 0, 0);
  }

  // 3. Time Counter Face (Inter Regular)
  const char *pathTime = NULL;
  if (FileExists("assets/fonts/Inter-Regular.ttf")) pathTime = "assets/fonts/Inter-Regular.ttf";
  else if (FileExists("../assets/fonts/Inter-Regular.ttf")) pathTime = "../assets/fonts/Inter-Regular.ttf";

  if (pathTime) {
    UNX_LOG_INFO("[FONTS] Loading time counter font from file: %s", pathTime);
    timeFace = LoadFontEx(pathTime, fontSize, 0, 0);
  }
  if (timeFace.texture.id == 0) {
    UNX_LOG_INFO("[FONTS] Loading time counter font (Inter Regular) from memory bundle...");
    timeFace = LoadFontFromMemory(".ttf", font_inter_regular, font_inter_regular_size, fontSize, 0, 0);
  }

  UNX_LOG_INFO("[FONTS] Result: Default ID=%u, Bold ID=%u, Time ID=%u", defaultFace.texture.id, boldFace.texture.id, timeFace.texture.id);

  if (defaultFace.texture.id == 0) {
    defaultFace = GetFontDefault();
  }
  if (boldFace.texture.id == 0) {
    boldFace = defaultFace;
  }
  if (timeFace.texture.id == 0) {
    timeFace = defaultFace;
  }

  // Common Unicode ranges for Font Awesome 5/6 (PUA range)
  // Most active icons are in 0xF000 - 0xF350
  UNX_LOG_INFO("[FONTS] Preparing icon codepoints...");
  int codepoints[1024];
  int count = 0;
  for (int i = 32; i < 127; i++)
    codepoints[count++] = i;
  for (int i = 0xF000; i <= 0xF350; i++)
    codepoints[count++] = i;

  // UI Symbols (Stars, Triangles, etc)
  codepoints[count++] = 0x2605; // solid star
  codepoints[count++] = 0x2606; // empty star
  codepoints[count++] = 0x25BA; // right triangle
  codepoints[count++] = 0x25C4; // left triangle
  codepoints[count++] = 0x266A; // eighth note
  codepoints[count++] = 0x2022; // bullet

  int iconFontSize = 32;

  UNX_LOG_INFO(
      "[FONTS] Loading icon fonts from memory (optimized range)...");
  // Font Awesome 5 Solid - Loaded from Memory
  iconSolid =
      LoadFontFromMemory(".otf", font_awesome_solid, font_awesome_solid_size,
                         iconFontSize, codepoints, count);
  if (iconSolid.texture.id == 0) {
    printf("[FONT] Failed to load solid icon font from memory\n");
  }

  // Font Awesome 5 Regular - Loaded from Memory
  iconRegular = LoadFontFromMemory(".otf", font_awesome_regular,
                                   font_awesome_regular_size, iconFontSize,
                                   codepoints, count);
  if (iconRegular.texture.id == 0) {
    printf("[FONT] Failed to load regular icon font from memory\n");
  }

  // Font Awesome 5 Brands - Loaded from Memory
  iconBrand =
      LoadFontFromMemory(".otf", font_awesome_brand, font_awesome_brand_size,
                         iconFontSize, codepoints, count);
  if (iconBrand.texture.id == 0) {
    printf("[FONT] Failed to load brand icon font from memory\n");
  }
  UNX_LOG_INFO("[FONTS] UIFonts_Init completed.");
}

void UIFonts_Unload(void) {
  if (defaultFace.texture.id != 0) UnloadFont(defaultFace);
  if (boldFace.texture.id != 0 && boldFace.texture.id != defaultFace.texture.id) {
    UnloadFont(boldFace);
  }
  if (timeFace.texture.id != 0 && timeFace.texture.id != defaultFace.texture.id && timeFace.texture.id != boldFace.texture.id) {
    UnloadFont(timeFace);
  }
  if (iconSolid.texture.id != 0) UnloadFont(iconSolid);
  if (iconRegular.texture.id != 0) UnloadFont(iconRegular);
  if (iconBrand.texture.id != 0) UnloadFont(iconBrand);
}

Font UIFonts_GetFace(float size) {
  (void)size; // Handled directly in DrawTextEx scaling
  return defaultFace;
}

Font UIFonts_GetBoldFace(float size) {
  (void)size;
  return boldFace;
}

Font UIFonts_GetTimeFace(float size) {
  (void)size;
  return timeFace;
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
