#include "colorfx_manager.h"
#include <string.h>

void ColorFXManager_Init(ColorFXManager* mgr) {
    mgr->activeFX = COLORFX_NONE; // Default to OFF
    mgr->colorValue = 0.0f;
    mgr->parameter = 0.5f; // Neutral mid point

    Space_Init(&mgr->space);
    DubEcho_Init(&mgr->dubEcho);
    Sweep_Init(&mgr->sweep);
    Noise_Init(&mgr->noise);
    Crush_Init(&mgr->crush);
    Filter_Init(&mgr->filter);
    Jet_Init(&mgr->jet);
}

void ColorFXManager_Free(ColorFXManager* mgr) {
    Space_Free(&mgr->space);
    DubEcho_Free(&mgr->dubEcho);
    Sweep_Free(&mgr->sweep);
    Noise_Free(&mgr->noise);
    Crush_Free(&mgr->crush);
    Filter_Free(&mgr->filter);
    Jet_Free(&mgr->jet);
}

void ColorFXManager_SetFX(ColorFXManager* mgr, ColorFXType type) {
    if (mgr->activeFX != type) {
        mgr->activeFX = type;
        // Optionally reset state of the newly selected effect to prevent pops
    }
}

void ColorFXManager_Process(ColorFXManager* mgr, float* outL, float* outR, float inL, float inR, float sampleRate) {
    if (mgr->colorValue == 0.0f || mgr->activeFX == COLORFX_NONE) {
        *outL = inL;
        *outR = inR;
        return;
    }

    switch (mgr->activeFX) {
        case COLORFX_SPACE:
            Space_Process(&mgr->space, outL, outR, inL, inR, mgr->colorValue, sampleRate);
            break;
        case COLORFX_DUBECHO:
            DubEcho_Process(&mgr->dubEcho, outL, outR, inL, inR, mgr->colorValue, sampleRate);
            break;
        case COLORFX_SWEEP:
            Sweep_Process(&mgr->sweep, outL, outR, inL, inR, mgr->colorValue, sampleRate);
            break;
        case COLORFX_NOISE:
            Noise_Process(&mgr->noise, outL, outR, inL, inR, mgr->colorValue, sampleRate);
            break;
        case COLORFX_CRUSH:
            Crush_Process(&mgr->crush, outL, outR, inL, inR, mgr->colorValue, sampleRate);
            break;
        case COLORFX_FILTER:
            Filter_Process(&mgr->filter, outL, outR, inL, inR, mgr->colorValue, sampleRate);
            break;
        case COLORFX_JET:
            Jet_Process(&mgr->jet, outL, outR, inL, inR, mgr->colorValue, sampleRate);
            break;
        default:
            *outL = inL;
            *outR = inR;
            break;
    }
}
