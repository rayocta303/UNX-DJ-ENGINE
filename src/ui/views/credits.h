#pragma once

#include "ui/components/component.h"
#include <stdbool.h>

typedef struct {
    bool IsActive;
    float Scroll;
} CreditsState;

typedef struct {
    Component base;
    CreditsState *State;
} CreditsRenderer;

void CreditsRenderer_Init(CreditsRenderer *r, CreditsState *state);
