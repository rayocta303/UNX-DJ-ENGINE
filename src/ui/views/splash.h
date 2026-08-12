#pragma once

#include "raylib.h"
#include "ui/components/component.h"


typedef struct SplashRenderer SplashRenderer;

struct SplashRenderer {
  Component base;
  Texture2D Logo;
  int *Progress;
};

void SplashRenderer_Init(SplashRenderer *s, int *progress);
void SplashRenderer_Unload(SplashRenderer *s);
