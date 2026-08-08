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
  s->currentFrame = 0;
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
    float dy = (SCREEN_HEIGHT - sh) / 2.0f;

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
  // Silence Raylib logs during loading to speed up startup
  SetTraceLogLevel(LOG_WARNING);

  s->base.Update = Splash_Update;
  s->base.Draw = Splash_Draw;
  
  s->frameCount = 1;
  s->currentFrame = 0;
  s->frameTimer = 0;
  s->frames = (Texture2D *)malloc(sizeof(Texture2D));

  bool loaded = false;

  // 1. Try static logo from memory bundle
  Image img = LoadImageFromMemory(".png", unx_logo, unx_logo_size);
  if (img.data != NULL) {
    if (img.width > 1080) {
      float aspect = (float)img.height / (float)img.width;
      ImageResize(&img, 1080, (int)(1080.0f * aspect));
    }
    s->frames[0] = LoadTextureFromImage(img);
    SetTextureFilter(s->frames[0], TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
    loaded = true;
  }

#if defined(SPLASH_FRAME_COUNT) && (SPLASH_FRAME_COUNT > 0)
  // 2. Try first frame of splash bundle if logo was not available
  if (!loaded && splash_frames[0] != NULL) {
    Image imgFrame = LoadImageFromMemory(".png", splash_frames[0], splash_frames_size[0]);
    if (imgFrame.data != NULL) {
      if (imgFrame.width > 1080) {
        float aspect = (float)imgFrame.height / (float)imgFrame.width;
        ImageResize(&imgFrame, 1080, (int)(1080.0f * aspect));
      }
      s->frames[0] = LoadTextureFromImage(imgFrame);
      SetTextureFilter(s->frames[0], TEXTURE_FILTER_BILINEAR);
      UnloadImage(imgFrame);
      loaded = true;
    }
  }
#endif

  // 3. Fallback to disk image
  if (!loaded) {
    const char *diskPaths[] = {
      "assets/splash.png",
      "assets/splash/frame_000_delay-0.04s.png",
      "assets/splash/frame_000_delay-0.05s.png"
    };
    for (int i = 0; i < 3; i++) {
      if (FileExists(diskPaths[i])) {
        Image imgDisk = LoadImage(diskPaths[i]);
        if (imgDisk.data != NULL) {
          if (imgDisk.width > 1080) {
            float aspect = (float)imgDisk.height / (float)imgDisk.width;
            ImageResize(&imgDisk, 1080, (int)(1080.0f * aspect));
          }
          s->frames[0] = LoadTextureFromImage(imgDisk);
          SetTextureFilter(s->frames[0], TEXTURE_FILTER_BILINEAR);
          UnloadImage(imgDisk);
          loaded = true;
          break;
        }
      }
    }
  }

  if (!loaded) {
    s->frames[0] = (Texture2D){0};
  }

  // Restore Raylib logs
  SetTraceLogLevel(LOG_INFO);

  s->Progress = progress;
}

void SplashRenderer_Unload(SplashRenderer *s) {
  if (s->frames != NULL) {
    for (int i = 0; i < s->frameCount; i++) {
      if (s->frames[i].id != 0) {
        // Only unload if it's not a shared texture (in case of re-use logic)
        bool isShared = false;
        for(int j=0; j<i; j++) {
            if(s->frames[i].id == s->frames[j].id) {
                isShared = true;
                break;
            }
        }
        if(!isShared) UnloadTexture(s->frames[i]);
      }
    }
    free(s->frames);
    s->frames = NULL;
    s->frameCount = 0;
  }
}
