#pragma once

#include "ui/components/component.h"
#include <stdbool.h>

#include "ui/components/touch_utility.h"

typedef struct {
    bool IsActive;
    float Scroll;
    TouchScroll ScrollPhysics;
} CreditsState;

typedef struct {
    Component base;
    CreditsState *State;
} CreditsRenderer;

void CreditsRenderer_Init(CreditsRenderer *r, CreditsState *state);
