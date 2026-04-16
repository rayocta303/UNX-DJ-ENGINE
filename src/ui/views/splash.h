#pragma once

#include "raylib.h"
#include "ui/components/component.h"


typedef struct SplashRenderer SplashRenderer;

struct SplashRenderer {
  Component base;
  Texture2D *frames;
  int frameCount;
  int currentFrame;
  float frameTimer;
  int *Progress;
};

void SplashRenderer_Init(SplashRenderer *s, int *progress);
