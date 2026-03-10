#pragma once

#include "ui/components/component.h"
#include <stdbool.h>

typedef struct {
  bool IsActive;
  char Version[16];
  char Developer[64];
  char Instagram[32];
} AboutState;

typedef struct AboutRenderer AboutRenderer;

struct AboutRenderer {
  Component base;
  AboutState *State;
};

void AboutRenderer_Init(AboutRenderer *r, AboutState *state);
