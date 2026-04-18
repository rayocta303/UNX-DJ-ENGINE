#include "ui/views/splash.h"
#include "version.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int Splash_Update(Component *base) {
  SplashRenderer *s = (SplashRenderer *)base;
  if (s->frameCount > 1) {
    s->frameTimer += GetFrameTime();
    // Assuming 0.04s average delay (approx 25 FPS)
    if (s->frameTimer >= 0.04f) {
      s->frameTimer = 0;
      s->currentFrame = (s->currentFrame + 1) % s->frameCount;
    }
  }
  return 0;
}

static void Splash_Draw(Component *base) {
  SplashRenderer *s = (SplashRenderer *)base;
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);

  Texture2D *tex = NULL;
  if (s->frames && s->frameCount > 0) {
    tex = &s->frames[s->currentFrame];
  }

  if (tex && tex->id != 0) {
    float scale = (SCREEN_WIDTH * 0.5f) / tex->width;
    if (scale > 1.0f)
      scale = 1.0f;

    float sw = tex->width * scale;
    float sh = tex->height * scale;

    float dx = (SCREEN_WIDTH - sw) / 2.0f;
    float dy = (SCREEN_HEIGHT - sh) / 2.0f - S(10.0f);

    DrawTextureEx(*tex, (Vector2){dx, dy}, 0.0f, scale, WHITE);
  }

  Font face = UIFonts_GetFace(14);
  char devInfo[128];
  sprintf(devInfo, "Developed by %s", APP_INSTAGRAM);
  DrawCentredText(devInfo, face, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 60, 14,
                  ColorWhite);

  // Loading Progress Bar
  if (s->Progress) {
    float barW = SCREEN_WIDTH * 0.4f;
    float barH = 6.0f;
    float barX = (SCREEN_WIDTH - barW) / 2.0f;
    float barY = SCREEN_HEIGHT - 100.0f;

    DrawRectangleRounded((Rectangle){barX, barY, barW, barH}, 1.0f, 4,
                         ColorDark2);

    float progress = (120.0f - (float)*s->Progress) / 120.0f;
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
  // Silence Raylib logs during frame loading to speed up startup
  SetTraceLogLevel(LOG_WARNING);

  s->base.Update = Splash_Update;
  s->base.Draw = Splash_Draw;
  
#ifdef SPLASH_FRAME_COUNT
  s->frameCount = SPLASH_FRAME_COUNT;
#else
  s->frameCount = 192;
#endif

#if defined(PLATFORM_IOS)
  // Cap frames on iOS to prevent OOM (30 frames is enough for a smooth 2s splash)
  if (s->frameCount > 30) s->frameCount = 30;
#endif
  
  s->currentFrame = 0;
  s->frameTimer = 0;
  s->frames = (Texture2D *)malloc(sizeof(Texture2D) * s->frameCount);

  bool loadedAny = false;
  for (int i = 0; i < s->frameCount; i++) {
    // Try memory bundle first (newly added)
#ifdef SPLASH_FRAME_COUNT
    Image img = LoadImageFromMemory(".png", splash_frames[i], splash_frames_size[i]);
    if (img.data != NULL) {
      s->frames[i] = LoadTextureFromImage(img);
      UnloadImage(img);
      loadedAny = true;
      continue;
    }
#endif

    // Fallback to disk
    char path[256];
    sprintf(path, "assets/splash/frame_%03d_delay-0.04s.png", i);
    if (!FileExists(path)) {
      sprintf(path, "assets/splash/frame_%03d_delay-0.05s.png", i);
    }

    Image imgDisk = LoadImage(path);
    if (imgDisk.data != NULL) {
      s->frames[i] = LoadTextureFromImage(imgDisk);
      UnloadImage(imgDisk);
      loadedAny = true;
    } else {
      if (i > 0)
        s->frames[i] = s->frames[i - 1];
      else
        s->frames[i] = (Texture2D){0};
    }
  }

  // Fallback to static if sequence failed
  if (!loadedAny) {
    Image img = LoadImage("assets/splash.png");
    if (img.data == NULL) {
      img = LoadImageFromMemory(".png", unx_logo, unx_logo_size);
    }
    if (img.data != NULL) {
      s->frameCount = 1;
      s->frames[0] = LoadTextureFromImage(img);
      UnloadImage(img);
    }
  }

  // Restore Raylib logs
  SetTraceLogLevel(LOG_INFO);

  s->Progress = progress;
}
