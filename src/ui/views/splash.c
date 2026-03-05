#include "ui/views/splash.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <math.h>

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

    // Loading Progress Bar
    if (s->Progress) {
        float barW = SCREEN_WIDTH * 0.4f;
        float barH = 6.0f;
        float barX = (SCREEN_WIDTH - barW) / 2.0f;
        float barY = SCREEN_HEIGHT - 100.0f;
        
        // Background
        DrawRectangleRounded((Rectangle){barX, barY, barW, barH}, 1.0f, 4, ColorDark2);
        
        // Progress (assuming 120 is max from main.c logic, we can normalize)
        // Actually since main.c decrements, 120 -> 0. Let's invert it.
        float progress = (120.0f - (float)*s->Progress) / 120.0f;
        if (progress < 0) progress = 0;
        if (progress > 1) progress = 1;

        DrawRectangleRounded((Rectangle){barX, barY, barW * progress, barH}, 1.0f, 4, ColorBlue);
        
        // Pulsing loading text
        static float pulse = 0;
        pulse += GetFrameTime() * 4.0f;
        Color textClr = ColorWhite;
        textClr.a = (unsigned char)(150 + 105 * sinf(pulse));
        
        UIDrawText("LOADING SYSTEM...", UIFonts_GetFace(S(7.5f)), barX, barY - S(10), S(7.5f), textClr);
    }
}

void SplashRenderer_Init(SplashRenderer *s, int *progress) {
    s->base.Update = Splash_Update;
    s->base.Draw = Splash_Draw;
    s->logo = LoadTexture("assets/images/Pioneer.png");
    s->Progress = progress;
}
