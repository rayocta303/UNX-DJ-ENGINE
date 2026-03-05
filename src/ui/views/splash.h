#pragma once

#include "ui/components/component.h"
#include "raylib.h"

typedef struct SplashRenderer SplashRenderer;

struct SplashRenderer {
    Component base;
    Texture2D logo;
    int *Progress; // Pointer to the progress counter (0-100 or current/total)
};

void SplashRenderer_Init(SplashRenderer *s, int *progress);
