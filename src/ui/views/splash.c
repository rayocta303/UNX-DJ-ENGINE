#include "ui/views/splash.h"
#include "version.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int Splash_Update(Component *base) {
  (void)base;
  return 0;
}

static void Splash_Draw(Component *base) {
  SplashRenderer *s = (SplashRenderer *)base;
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);

  Texture2D *tex = &s->Logo;

  if (tex && tex->id != 0) {
    float scale = (SCREEN_WIDTH * 0.5f) / tex->width;
    if (scale > 1.0f)
      scale = 1.0f;

    float sw = tex->width * scale;
    float sh = tex->height * scale;

    float dx = (SCREEN_WIDTH - sw) / 2.0f;
    float dy = (SCREEN_HEIGHT - sh) / 2.0f;

    DrawTextureEx(*tex, (Vector2){dx, dy}, 0.0f, scale, WHITE);
  }

  Font face = UIFonts_GetFace(S(14));
  char devInfo[128];
  sprintf(devInfo, "Developed by %s", APP_INSTAGRAM);
  DrawCentredText(devInfo, face, 0, SCREEN_WIDTH, SCREEN_HEIGHT - S(60), S(14),
                  ColorWhite);

  // Loading Progress Bar
  if (s->Progress) {
    float barW = SCREEN_WIDTH * 0.4f;
    float barH = S(6.0f);
    float barX = (SCREEN_WIDTH - barW) / 2.0f;
    float barY = SCREEN_HEIGHT - S(100.0f);

    DrawRectangleRounded((Rectangle){barX, barY, barW, barH}, 1.0f, 4,
                         ColorDark2);

    float progress = (120.0f - (float)*s->Progress) / 90.0f; // Target 90 frames (1.5s at 60fps)
    if (progress < 0)
      progress = 0;
    if (progress > 1)
      progress = 1;

    DrawRectangleRounded((Rectangle){barX, barY, barW * progress, barH}, 1.0f,
                         4, ColorBlue);

    static float pulse = 0;
    pulse += GetFrameTime() * 4.0f;
    Color textClr = ColorWhite;
    textClr.a = (unsigned char)(150 + 105 * sinf(pulse));

    UIDrawText("LOADING " APP_NAME "...", UIFonts_GetFace(S(7.5f)), barX,
               barY - S(10), S(7.5f), textClr);
  }
}

#include "ui/components/assets_bundle.h"

void SplashRenderer_Init(SplashRenderer *s, int *progress) {
  // Silence Raylib logs during loading to speed up startup
  SetTraceLogLevel(LOG_WARNING);

  s->base.Update = Splash_Update;
  s->base.Draw = Splash_Draw;
  
  s->Progress = progress;
  s->Logo = (Texture2D){0};

  bool loaded = false;

  // 1. Try static logo from memory bundle
  Image img = LoadImageFromMemory(".png", unx_logo, unx_logo_size);
  if (img.data != NULL) {
    if (img.width > 1080) {
      float aspect = (float)img.height / (float)img.width;
      ImageResize(&img, 1080, (int)(1080.0f * aspect));
    }
    s->Logo = LoadTextureFromImage(img);
    SetTextureFilter(s->Logo, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
    loaded = true;
  }

  // 2. Fallback to disk image
  if (!loaded) {
    if (FileExists("assets/splash.png")) {
      Image imgDisk = LoadImage("assets/splash.png");
      if (imgDisk.data != NULL) {
        if (imgDisk.width > 1080) {
          float aspect = (float)imgDisk.height / (float)imgDisk.width;
          ImageResize(&imgDisk, 1080, (int)(1080.0f * aspect));
        }
        s->Logo = LoadTextureFromImage(imgDisk);
        SetTextureFilter(s->Logo, TEXTURE_FILTER_BILINEAR);
        UnloadImage(imgDisk);
        loaded = true;
      }
    }
  }

  // Restore Raylib logs
  SetTraceLogLevel(LOG_INFO);

  s->Progress = progress;
}

void SplashRenderer_Unload(SplashRenderer *s) {
  if (s->Logo.id != 0) {
    UnloadTexture(s->Logo);
    s->Logo = (Texture2D){0};
  }
}
