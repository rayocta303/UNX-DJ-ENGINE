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
            
            // Calculate data density: how many data frames exist per UI frame (150Hz)
            // UI_Total_Frames = TrackLengthMs * 0.15
            float totalUIFrames = (float)r->State->TrackLengthMs * 0.15f;
            if (totalUIFrames > 0) {
                r->dataDensity = (float)r->dynWfmFrames / totalUIFrames;
            } else {
                r->dataDensity = 1.0f;
            }
        } else {
            r->dynWfmFrames = 480;
            r->dataDensity = 1.0f;
        }
    }

    float waveH = WAVE_AREA_H / 2.0f;
    float wfY = TOP_BAR_H + (r->ID * waveH);
    float wfLeft = SIDE_PANEL_W;
    float wfRight = BEAT_FX_X;

    Vector2 mouse = GetMousePosition();
    bool inWaveform = (mouse.x >= wfLeft && mouse.x <= wfRight && mouse.y >= wfY && mouse.y <= wfY + waveH);

    // Zoom & Jog Interaction Logic
    if (inWaveform) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            r->State->ZoomScale -= wheel * 0.5f; 
            if (r->State->ZoomScale < 0.25f) r->State->ZoomScale = 0.25f;
            if (r->State->ZoomScale > 32.0f) r->State->ZoomScale = 32.0f;
        }
    }

    if (inWaveform && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        r->State->IsTouching = true;
        r->lastMouseX = mouse.x;
    } 
    
    if (r->State->IsTouching) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mouse.x - r->lastMouseX;
            r->lastMouseX = mouse.x;

            float pitchRatio = 1.0f + (r->State->TempoPercent / 100.0f);
            float effectiveZoom = (float)r->State->ZoomScale * pitchRatio;
            
            float moveHF = -dx * effectiveZoom;
            
            if (r->State->VinylModeEnabled) {
                // Vinyl Mode (Scratch) movement (follows hand)
                r->State->JogDelta += (double)moveHF;
            } else {
                // CDJ Nudge logic (Pitch bend)
                // Scaling 1.0 is too sensitive for nudge, let's use a factor
                r->State->JogDelta += (double)(moveHF * 0.5); 
            }
        } else {
            r->State->IsTouching = false;
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

    if (r->State->LoadedTrack == NULL) {
        return;
    }

    float playheadX = wfLeft + wfW / 2.0f;
    float effectiveZoom = (float)r->State->ZoomScale;
    if (effectiveZoom < 0.1f) effectiveZoom = 0.1f;

    double elapsedHalfFrames = r->State->Position;

    int iPixel = (int)(elapsedHalfFrames / (double)effectiveZoom);
    float fracX = (float)((elapsedHalfFrames / (double)effectiveZoom) - (double)iPixel);

    int srcX0 = iPixel - (int)(wfW / 2.0f);
    unsigned char *dynData = r->State->LoadedTrack->DynamicWaveform;
    float waveCenter = waveH / 2.0f;

    int aggZoom = (int)(effectiveZoom * r->dataDensity);
    if (aggZoom < 1) aggZoom = 1;

    // --- Boundary Setup ---
    // Scissor everything to prevent rendering outside deck area
    BeginScissorMode((int)wfLeft, (int)wfY, (int)wfW, (int)waveH);

    DrawRectangle(wfLeft, wfY, wfW, waveH, ColorBlack);

    // Store previous amplitudes for continuous shape rendering
    float prevBAmp = -1.0f;
    float prevMAmp = -1.0f;
    float prevHAmp = -1.0f;
    float prevPX   = -100.0f; // Far left initialization

    if (dynData != NULL && r->dynWfmFrames > 0) {
        for (float screenX = 0; screenX <= wfW + 1; screenX++) {
            int xInt = (int)screenX;
            int dataX = (int)((double)(srcX0 + xInt) * (double)effectiveZoom * (double)r->dataDensity);

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

                // --- Waveform 3-Band (Preset) Calibration Parameters ---
                // Anda bisa merubah parameter di bawah ini untuk kalibrasi visual sendiri:
                
                // 1. Palet Warna (Pioneer DJ Standard)
                Color colorBass = (Color){0, 100, 255, 255};   // Biru Terang (Dasar)
                Color colorMid  = (Color){150, 95, 20, 255};   // Coklat/Emas (Tengah)
                Color colorHigh = (Color){255, 250, 230, 255}; // Putih/Krem (Puncak)
                
                // 2. Skala Vertikal (Berapa tinggi waveform mengisi area deck)
                float vertScale = 1.9f; 

                // 3. Rasio Layer (Seberapa besar layer Mid & High muncul relatif terhadap Bass)
                float bAmp = (float)amplitude * vertScale;
                float mAmp = 0;
                float hAmp = 0;

                // Logika pemisahan frekuensi berdasarkan 'colorIdx' Rekordbox (0-7)
                if (colorIdx >= 6) {        // Frekuensi Tinggi (Cymbal, Hi-hat, Vocal High)
                    mAmp = bAmp * 0.85f;    // Mid mengisi 85% area biru
                    hAmp = bAmp * 0.60f;    // High mengisi 60% area biru (Inti Putih)
                } else if (colorIdx >= 3) { // Frekuensi Menengah (Snare, Melodi, Vocal)
                    mAmp = bAmp * 0.75f;    // Mid mengisi 75% area biru
                    hAmp = bAmp * 0.20f;    // High muncul tipis (20%)
                } else {                    // Frekuensi Rendah (Kick, Sub-Bass)
                    mAmp = bAmp * 0.40f;    // Mid sangat sedikit (40%)
                    hAmp = bAmp * 0.05f;    // High hampir tidak ada
                }

                // 4. Clamping (Agar tidak keluar dari batas area visual)
                if (bAmp > waveCenter - 1.0f) bAmp = waveCenter - 1.0f;
                if (mAmp > bAmp) mAmp = bAmp * 0.9f;
                if (hAmp > mAmp) hAmp = mAmp * 0.8f;

                // --- Rendering Loop (Continuous Shape) ---
                float px = wfLeft + screenX - fracX;

                if (px >= wfLeft - 2.0f && px <= wfRight + 2.0f) {
                    if (prevPX > -90.0f) {
                        // CCW Order in Y-Down: (P1, P3, P2) & (P1, P4, P3)
                        
                        // 1. Bass (Blue)
                        DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevBAmp}, (Vector2){px, wfY + waveCenter + bAmp}, (Vector2){px, wfY + waveCenter - bAmp}, colorBass);
                        DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevBAmp}, (Vector2){prevPX, wfY + waveCenter + prevBAmp}, (Vector2){px, wfY + waveCenter + bAmp}, colorBass);
                        
                        // 2. Mid (Brown)
                        if (mAmp > 0 || prevMAmp > 0) {
                            DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevMAmp}, (Vector2){px, wfY + waveCenter + mAmp}, (Vector2){px, wfY + waveCenter - mAmp}, colorMid);
                            DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevMAmp}, (Vector2){prevPX, wfY + waveCenter + prevMAmp}, (Vector2){px, wfY + waveCenter + mAmp}, colorMid);
                        }

                        // 3. High (White)
                        if (hAmp > 0 || prevHAmp > 0) {
                            DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevHAmp}, (Vector2){px, wfY + waveCenter + hAmp}, (Vector2){px, wfY + waveCenter - hAmp}, colorHigh);
                            DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevHAmp}, (Vector2){prevPX, wfY + waveCenter + prevHAmp}, (Vector2){px, wfY + waveCenter + hAmp}, colorHigh);
                        }
                    } else {
                        // First pixel fallback
                        DrawRectangleV((Vector2){px, wfY + waveCenter - bAmp}, (Vector2){1, bAmp * 2}, colorBass);
                        if (mAmp > 0) DrawRectangleV((Vector2){px, wfY + waveCenter - mAmp}, (Vector2){1, mAmp * 2}, colorMid);
                        if (hAmp > 0) DrawRectangleV((Vector2){px, wfY + waveCenter - hAmp}, (Vector2){1, hAmp * 2}, colorHigh);
                    }

                    // Update previous values for next column
                    prevBAmp = bAmp;
                    prevMAmp = mAmp;
                    prevHAmp = hAmp;
                    prevPX   = px;
                }
            }
        }
    }

    EndScissorMode();

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
    
    EndScissorMode();
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
