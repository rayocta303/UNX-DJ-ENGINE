#include "ui/player/waveform.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>

static int Waveform_Update(Component *base) {
    WaveformRenderer *r = (WaveformRenderer *)base;

    Vector2 mouse = GetMousePosition();
    float waveH = WAVE_AREA_H / 2.0f;
    float wfY = TOP_BAR_H + (r->ID * waveH);
    float wfLeft = SIDE_PANEL_W;
    float wfRight = BEAT_FX_X;

    bool inWaveform = (mouse.x >= wfLeft && mouse.x <= wfRight && mouse.y >= wfY && mouse.y <= wfY + waveH);

    if (inWaveform) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            // Simplified zoom mock
        }
    }

    return 0;
}

static void Waveform_Draw(Component *base) {
    WaveformRenderer *r = (WaveformRenderer *)base;
    float waveH = WAVE_AREA_H / 2.0f;
    float wfY = TOP_BAR_H + (r->ID * waveH);
    float wfLeft = SIDE_PANEL_W;
    float wfRight = BEAT_FX_X;
    float wfW = wfRight - wfLeft;

    DrawRectangle(wfLeft, wfY, wfW, waveH, ColorBlack);

    float playheadX = wfLeft + wfW / 2.0f;

    // Simplified dynamic waveform mock
    float waveCenter = waveH / 2.0f;
    for (float screenX = 0; screenX <= wfW; screenX += 2.0f) {
        float px = wfLeft + screenX;
        float amp = 10.0f; // Mock amplitude
        DrawLine(px, wfY + waveCenter - amp, px, wfY + waveCenter + amp, (Color){0, 90, 255, 255});
    }

    // Playhead Line
    DrawLine(playheadX, wfY, playheadX, wfY + waveH, ColorRed);

    if (r->ID == 0) {
        DrawLine(wfLeft, wfY + waveH - 1, wfLeft + wfW, wfY + waveH - 1, ColorDark1);
    }
}

void WaveformRenderer_Init(WaveformRenderer *r, int id, DeckState *state) {
    r->base.Update = Waveform_Update;
    r->base.Draw = Waveform_Draw;
    r->ID = id;
    r->State = state;
    r->dynWfmFrames = 480;
    r->lastMouseX = 0;
}
