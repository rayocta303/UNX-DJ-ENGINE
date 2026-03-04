#include "ui/player/waveform.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>

static int Waveform_Update(Component *base) {
    WaveformRenderer *r = (WaveformRenderer *)base;

    float waveH = WAVE_AREA_H / 2.0f;
    float wfY = TOP_BAR_H + (r->ID * waveH);
    float wfLeft = SIDE_PANEL_W;
    float wfRight = BEAT_FX_X;

    Vector2 mouse = GetMousePosition();
    bool inWaveform = (mouse.x >= wfLeft && mouse.x <= wfRight && mouse.y >= wfY && mouse.y <= wfY + waveH);

    // Zoom Logic
    if (inWaveform) {
        float wy = GetMouseWheelMove();
        if (wy != 0.0f) {
            // Placeholder: Handle wheel zoom change
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            r->State->IsScratching = true;
            r->lastMouseX = mouse.x;
        } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && r->State->IsScratching) {
            float dx = mouse.x - r->lastMouseX;
            r->lastMouseX = mouse.x;

            float pitchRatio = 1.0f + (r->State->TempoPercent / 100.0f);
            float effectiveZoom = 4.0f * pitchRatio; // hardcoded zoom 4
            
            float moveHF = -dx * effectiveZoom;
            (void)moveHF;
            // Send to audio engine later
        } else {
            r->State->IsScratching = false;
        }
    } else {
        r->State->IsScratching = false;
    }

    if (r->State->LoadedTrack != NULL) {
        r->dynWfmFrames = r->State->LoadedTrack->DynamicWaveformLen;
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

    if (r->State->LoadedTrack == NULL || r->State->LoadedTrack->DynamicWaveformLen == 0) {
        return;
    }

    float playheadX = wfLeft + wfW / 2.0f;
    float pitchRatio = 1.0f + (r->State->TempoPercent / 100.0f);
    float effectiveZoom = 4.0f * pitchRatio; // Hardcoded ZoomScale = 4

    double elapsedHalfFrames = (double)r->State->PositionMs * 0.15;

    int iPixel = (int)(elapsedHalfFrames / effectiveZoom);
    float fracX = (float)((elapsedHalfFrames / effectiveZoom) - iPixel);

    int srcX0 = iPixel - (int)(wfW / 2.0f);
    unsigned char *dynData = r->State->LoadedTrack->DynamicWaveform;
    float waveCenter = waveH / 2.0f;

    int aggZoom = (int)effectiveZoom;
    if (aggZoom < 1) aggZoom = 1;

    for (float screenX = 0; screenX <= wfW + 1; screenX++) {
        int dataX = (int)((srcX0 + (int)screenX) * effectiveZoom);

        if (dataX >= 0 && dataX < r->dynWfmFrames) {
            int amplitude = dynData[dataX] & 0x1F;
            int colorIdx = dynData[dataX] >> 5;

            if (aggZoom > 1) {
                for (int j = 0; j < aggZoom - 1; j++) {
                    int chkX = dataX + j + 1;
                    if (chkX >= 0 && chkX < r->dynWfmFrames) {
                        int ampChk = dynData[chkX] & 0x1F;
                        if (ampChk > amplitude) {
                            amplitude = ampChk;
                            if (amplitude > 13) colorIdx = dynData[chkX] >> 5;
                        }
                    }
                }
            }

            if (amplitude > (int)waveCenter - 1) amplitude = (int)waveCenter - 1;

            float ampB = 0, ampY = 0, ampW = 0;
            if (colorIdx >= 6) {
                ampW = (float)amplitude;
            } else if (colorIdx >= 3) {
                ampY = (float)amplitude;
                ampW = (float)amplitude * 0.5f;
            } else {
                ampB = (float)amplitude;
                ampY = (float)amplitude * 0.7f;
                ampW = (float)amplitude * 0.35f;
            }

            float px = wfLeft + screenX - fracX;

            if (px >= wfLeft && px <= wfRight) {
                if (ampB > 0) DrawLine(px, wfY + waveCenter - ampB, px, wfY + waveCenter + ampB, (Color){0, 90, 255, 255});
                if (ampY > 0) DrawLine(px, wfY + waveCenter - ampY, px, wfY + waveCenter + ampY, (Color){210, 130, 20, 255});
                if (ampW > 0) DrawLine(px, wfY + waveCenter - ampW, px, wfY + waveCenter + ampW, (Color){255, 250, 220, 255});
            }
        }
    }

    // Beat Grid
    for (int i=0; i < 1024; i++) {
        unsigned int originalMs = r->State->LoadedTrack->BeatGrid[i];
        if (originalMs == 0xFFFFFFFF || originalMs == 0) break;

        double beatPosHF = (double)originalMs * 0.15;
        float px = (float)((beatPosHF - elapsedHalfFrames) / effectiveZoom);
        float bx = playheadX + px;

        if (bx >= wfLeft && bx <= wfRight) {
            Color clr = (Color){255, 255, 255, 80}; 
            float w = 1.0f;

            int downbeatTarget = (1 - r->State->LoadedTrack->GridOffset) & 3;
            if (i % 4 == downbeatTarget) {
                clr = (Color){255, 0, 0, 80}; 
                w = 2.0f;
            }

            DrawRectangle(bx - (w/2.0f), wfY, w, waveH, clr);
        }
    }

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
