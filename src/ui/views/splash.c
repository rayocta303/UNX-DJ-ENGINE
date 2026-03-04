#include "ui/views/splash.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"

static int Splash_Update(Component *base) {
    (void)base;
    return 0;
}

static void Splash_Draw(Component *base) {
    SplashRenderer *s = (SplashRenderer *)base;
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);

    if (s->logo.id != 0) {
        float scale = (SCREEN_WIDTH * 0.6f) / s->logo.width;
        if (scale > 1.0f) scale = 1.0f;
        
        float sw = s->logo.width * scale;
        float sh = s->logo.height * scale;
        
        float dx = (SCREEN_WIDTH - sw) / 2.0f;
        float dy = (SCREEN_HEIGHT - sh) / 2.0f - 20.0f;

        DrawTextureEx(s->logo, (Vector2){dx, dy}, 0.0f, scale, WHITE);
    }
    
    Font face = UIFonts_GetFace(14);
    DrawCentredText("Developed by UNX", face, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 60, 14, ColorWhite);
}

void SplashRenderer_Init(SplashRenderer *s) {
    s->base.Update = Splash_Update;
    s->base.Draw = Splash_Draw;
    s->logo = LoadTexture("assets/images/Pioneer.png");
}
