#pragma once

#include "raylib.h"
#include "ui/components/component.h"


typedef struct SplashRenderer SplashRenderer;

struct SplashRenderer {
  Component base;
  Texture2D Logo;
  double StartTime;
};

void SplashRenderer_Init(SplashRenderer *s);
void SplashRenderer_Unload(SplashRenderer *s);
