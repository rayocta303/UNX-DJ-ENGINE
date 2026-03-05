#include "ui/player/waveform.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>

static int Waveform_Update(Component *base) {
    WaveformRenderer *r = (WaveformRenderer *)base;

    // Refresh dynamic frame count when track changes
    if (r->State->LoadedTrack != r->cachedTrack) {
        r->cachedTrack = r->State->LoadedTrack;
        if (r->cachedTrack) {
            r->dynWfmFrames = r->cachedTrack->DynamicWaveformLen;
        } else {
            r->dynWfmFrames = 480;
        }
    }

    float waveH = WAVE_AREA_H / 2.0f;
    float wfY = TOP_BAR_H + (r->ID * waveH);
    float wfLeft = SIDE_PANEL_W;
    float wfRight = BEAT_FX_X;

    Vector2 mouse = GetMousePosition();
    bool inWaveform = (mouse.x >= wfLeft && mouse.x <= wfRight && mouse.y >= wfY && mouse.y <= wfY + waveH);

    // Zoom Logic
    if (inWaveform) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            r->State->ZoomScale -= (int)wheel;
            if (r->State->ZoomScale < 1) r->State->ZoomScale = 1;
            if (r->State->ZoomScale > 32) r->State->ZoomScale = 32;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            r->State->IsScratching = true;
            r->lastMouseX = mouse.x;
        } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && r->State->IsScratching) {
            float dx = mouse.x - r->lastMouseX;
            r->lastMouseX = mouse.x;

            float pitchRatio = 1.0f + (r->State->TempoPercent / 100.0f);
            float effectiveZoom = (float)r->State->ZoomScale * pitchRatio;
            
            float moveHF = -dx * effectiveZoom;
            r->State->ScratchDelta += (double)moveHF;
            r->State->IsScratching = true;
        } else {
            r->State->IsScratching = false;
        }
    } else {
        r->State->IsScratching = false;
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

    if (r->State->LoadedTrack == NULL) {
        return;
    }

    float playheadX = wfLeft + wfW / 2.0f;
    float pitchRatio = 1.0f + (r->State->TempoPercent / 100.0f);
    float effectiveZoom = (float)r->State->ZoomScale * pitchRatio;
    if (effectiveZoom < 0.1f) effectiveZoom = 0.1f;

    double elapsedHalfFrames = r->State->Position;

    int iPixel = (int)(elapsedHalfFrames / (double)effectiveZoom);
    float fracX = (float)((elapsedHalfFrames / (double)effectiveZoom) - (double)iPixel);

    int srcX0 = iPixel - (int)(wfW / 2.0f);
    unsigned char *dynData = r->State->LoadedTrack->DynamicWaveform;
    float waveCenter = waveH / 2.0f;

    int aggZoom = (int)effectiveZoom;
    if (aggZoom < 1) aggZoom = 1;

    if (dynData != NULL && r->dynWfmFrames > 0) {
        for (float screenX = 0; screenX <= wfW + 1; screenX++) {
            int xInt = (int)screenX;
            int dataX = (int)((double)(srcX0 + xInt) * (double)effectiveZoom);

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
                    if (ampB > 0) DrawRectangleV((Vector2){px, wfY + waveCenter - ampB}, (Vector2){1, ampB * 2}, (Color){0, 90, 255, 255});
                    if (ampY > 0) DrawRectangleV((Vector2){px, wfY + waveCenter - ampY}, (Vector2){1, ampY * 2}, (Color){210, 130, 20, 255});
                    if (ampW > 0) DrawRectangleV((Vector2){px, wfY + waveCenter - ampW}, (Vector2){1, ampW * 2}, (Color){255, 250, 220, 255});
                }
            }
        }
    }

    // Beat Grid
    if (r->State->LoadedTrack != NULL) {
        for (int i=0; i < 1024; i++) {
            unsigned int originalMs = r->State->LoadedTrack->BeatGrid[i];
            if (originalMs == 0xFFFFFFFF || originalMs == 0) break;

            double beatPosHF = (double)originalMs * 0.15;
            float px = (float)((beatPosHF - elapsedHalfFrames) / (double)effectiveZoom);
            float bx = playheadX + px;

            if (bx >= wfLeft && bx <= wfRight) {
                Color clr = (Color){255, 255, 255, 50}; 
                float tw = 1.0f;

                int downbeatTarget = (1 - r->State->LoadedTrack->GridOffset) & 3;
                if (i % 4 == downbeatTarget) {
                    clr = (Color){255, 0, 0, 100}; 
                    tw = 2.0f;
                }

                float segmentH = waveH * 0.12f;
                DrawRectangleV((Vector2){bx - (tw/2.0f), wfY}, (Vector2){tw, segmentH}, clr);
                DrawRectangleV((Vector2){bx - (tw/2.0f), wfY + waveH - segmentH}, (Vector2){tw, segmentH}, clr);
            }
        }
    }

    // HotCue Markers
    Color hcColors[] = {
        {0, 255, 0, 255}, {255, 0, 0, 255}, {255, 128, 0, 255}, {255, 255, 0, 255},
        {0, 0, 255, 255}, {255, 0, 255, 255}, {0, 255, 255, 255}, {128, 0, 255, 255}
    };
    const char* hcLabels[] = {"A", "B", "C", "D", "E", "F", "G", "H"};

    // 1. Draw Memory Cues (white lines)
    for (int i=0; i < r->State->LoadedTrack->CuesCount; i++) {
        HotCue *mc = &r->State->LoadedTrack->Cues[i];
        double mcHF = (double)mc->Start * 0.15;
        float hx = playheadX + (float)((mcHF - elapsedHalfFrames) / (double)effectiveZoom);
        if (hx >= wfLeft && hx <= wfRight) {
            DrawLineEx((Vector2){hx, wfY}, (Vector2){hx, wfY + waveH}, 1.2f, ColorWhite);
        }
    }

    // 2. Draw HotCues (flags)
    for (int i = 0; i < r->State->LoadedTrack->HotCuesCount; i++) {
        HotCue *hc = &r->State->LoadedTrack->HotCues[i];
        int hcIdx = hc->ID - 1;
        if (hcIdx < 0 || hcIdx >= 8) continue;

        double hcHF = (double)hc->Start * 0.15;
        float hx = playheadX + (float)((hcHF - elapsedHalfFrames) / (double)effectiveZoom);

        if (hx >= wfLeft && hx <= wfRight) {
            Color clr = hcColors[hcIdx];
            DrawLineEx((Vector2){hx, wfY}, (Vector2){hx, wfY + waveH}, 1.5f, clr);

            // Flag
            float fw = S(14);
            DrawRectangle(hx, wfY, fw, fw, clr);
            UIDrawText(hcLabels[hcIdx], UIFonts_GetFace(S(10)), hx + S(3), wfY + S(1), S(10), ColorWhite);
        }
    }

    DrawLineEx((Vector2){playheadX, wfY}, (Vector2){playheadX, wfY + waveH}, 1.5f, ColorRed);
    if (r->ID == 0) {
        DrawLineEx((Vector2){wfLeft, wfY + waveH - 1}, (Vector2){wfLeft + wfW, wfY + waveH - 1}, 1.5f, ColorDark1);
    }
}

void WaveformRenderer_Init(WaveformRenderer *r, int id, DeckState *state) {
    r->base.Update = Waveform_Update;
    r->base.Draw = Waveform_Draw;
    r->ID = id;
    r->State = state;
    r->cachedTrack = NULL;
    r->dynWfmFrames = 480;
    r->lastMouseX = 0;
}
