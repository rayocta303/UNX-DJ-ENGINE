#pragma once

#include "ui/components/component.h"
#include "audio/engine.h"
#include <stdbool.h>

typedef struct {
    bool IsActive;
    AudioEngine *AudioPlugin;
    struct BeatFXState *FXState;
    void* ActiveHandle;
} MixerState;

typedef struct {
    Component base;
    MixerState *State;
} MixerRenderer;

void MixerRenderer_Init(MixerRenderer *r, MixerState *state);
