#pragma once

#include "ui/components/component.h"
#include <raylib.h>

typedef struct SplashRenderer SplashRenderer;

struct SplashRenderer {
    Component base;
    Texture2D logo;
};

void SplashRenderer_Init(SplashRenderer *s);
