#include "ui/player/waveform.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "logic/quantize.h"
#include <stdio.h>
#include <stdlib.h>

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

    Vector2 mouse = UIGetMousePosition();
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

    unsigned char *dynData = r->State->LoadedTrack->DynamicWaveform;
    float waveCenter = waveH / 2.0f;

    int aggZoom = (int)(effectiveZoom * r->dataDensity);
    if (aggZoom < 1) aggZoom = 1;

    // --- Boundary Setup ---
    // Scissor everything to prevent rendering outside deck area
    // BeginScissorMode expects raw screen coordinates, so we must add the UI offset
    BeginScissorMode((int)(wfLeft + UI_OffsetX), (int)(wfY + UI_OffsetY), (int)wfW, (int)waveH);

    DrawRectangle(wfLeft, wfY, wfW, waveH, ColorBlack);

    // Store previous amplitudes for continuous shape rendering
    float prevBAmp = -1.0f;
    float prevMAmp = -1.0f;
    float prevHAmp = -1.0f;
    float prevPX   = -100.0f; // Far left initialization

    if (dynData != NULL && r->dynWfmFrames > 0) {
        // Pre-calculate constants for the loop
        double baseDataPos = elapsedHalfFrames * (double)r->dataDensity;
        double zoomStep = (double)effectiveZoom * (double)r->dataDensity;
        float centerOffset = wfW / 2.0f;
        
        // Anti-flicker: Use stable fractional sampling
        for (float screenX = 0; screenX <= wfW + 1; screenX += 1.0f) {
            double dataPos = baseDataPos + (double)(screenX - centerOffset) * zoomStep;
            int dataIndex = (int)dataPos;

            if (dataIndex >= 0 && dataIndex < r->dynWfmFrames) {
                // Average window for smooth liquid shape
                int sumAmp = 0;
                int sumColor = 0;
                int count = 0;
                
                int loopSpan = aggZoom > 11 ? aggZoom : 11;
                int startK = dataIndex - (loopSpan / 2);
                
                for (int j = 0; j < loopSpan; j++) {
                    int chkX = startK + j;
                    if (chkX >= 0 && chkX < r->dynWfmFrames) {
                        sumAmp += (dynData[chkX] & 0x1F);
                        sumColor += (dynData[chkX] >> 5);
                        count++;
                    }
                }

                int amplitude = (count > 0) ? (sumAmp / count) : 0;
                int colorIdx = (count > 0) ? (sumColor / count) : (dynData[dataIndex] >> 5);

                // --- Calibration Parameters ---
                float vertScale = 1.9f; 
                float bAmp = (float)amplitude * vertScale;
                float mAmp = 0, hAmp = 0;

                // Frequency splitting
                if (colorIdx >= 6) { mAmp = bAmp * 0.85f; hAmp = bAmp * 0.60f; }
                else if (colorIdx >= 3) { mAmp = bAmp * 0.75f; hAmp = bAmp * 0.20f; }
                else { mAmp = bAmp * 0.40f; hAmp = bAmp * 0.05f; }

                if (bAmp > waveCenter - 1.0f) bAmp = waveCenter - 1.0f;
                if (mAmp > bAmp) mAmp = bAmp * 0.9f;
                if (hAmp > mAmp) hAmp = mAmp * 0.8f;

                float px = wfLeft + screenX;

                if (px >= wfLeft - 2.0f && px <= wfRight + 2.0f) {
                    if (prevPX > -90.0f) {
                        Color colorBass = {0, 100, 255, 255};
                        Color colorMid  = {150, 95, 20, 255};
                        Color colorHigh = {255, 250, 230, 255};

                        // Bass
                        DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevBAmp}, (Vector2){px, wfY + waveCenter + bAmp}, (Vector2){px, wfY + waveCenter - bAmp}, colorBass);
                        DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevBAmp}, (Vector2){prevPX, wfY + waveCenter + prevBAmp}, (Vector2){px, wfY + waveCenter + bAmp}, colorBass);
                        
                        // Mid
                        if (mAmp > 0 || prevMAmp > 0) {
                            DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevMAmp}, (Vector2){px, wfY + waveCenter + mAmp}, (Vector2){px, wfY + waveCenter - mAmp}, colorMid);
                            DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevMAmp}, (Vector2){prevPX, wfY + waveCenter + prevMAmp}, (Vector2){px, wfY + waveCenter + mAmp}, colorMid);
                        }

                        // High
                        if (hAmp > 0 || prevHAmp > 0) {
                            DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevHAmp}, (Vector2){px, wfY + waveCenter + hAmp}, (Vector2){px, wfY + waveCenter - hAmp}, colorHigh);
                            DrawTriangle((Vector2){prevPX, wfY + waveCenter - prevHAmp}, (Vector2){prevPX, wfY + waveCenter + prevHAmp}, (Vector2){px, wfY + waveCenter + hAmp}, colorHigh);
                        }
                    } else {
                        DrawRectangleV((Vector2){px, wfY + waveCenter - bAmp}, (Vector2){1, bAmp * 2}, (Color){0, 100, 255, 255});
                    }
                    prevBAmp = bAmp; prevMAmp = mAmp; prevHAmp = hAmp; prevPX = px;
                }
            }
        }
    }

    EndScissorMode();

    // Beat Grid
    if (r->State->LoadedTrack != NULL) {
        for (int i=0; i < 1024; i++) {
            unsigned int originalMs = r->State->LoadedTrack->BeatGrid[i].Time;
            uint16_t beatNum = r->State->LoadedTrack->BeatGrid[i].BeatNumber;
            if (originalMs == 0xFFFFFFFF || originalMs == 0) break;

            double beatPosHF = (double)originalMs * 0.15;
            float px = (float)((beatPosHF - elapsedHalfFrames) / (double)effectiveZoom);
            float bx = playheadX + px;

            if (bx >= wfLeft && bx <= wfRight) {
                Color clr = (Color){255, 255, 255, 50}; 
                float tw = 1.0f;

                if (beatNum == 1) {
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
            
            unsigned char cR = hc->Color[0];
            unsigned char cG = hc->Color[1];
            unsigned char cB = hc->Color[2];
            if (cR != 0 || cG != 0 || cB != 0) {
                clr = (Color){ cR, cG, cB, 255 };
            }
            
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
    
    // --- Phase Meter UI ---
    // Drawn above the main waveform block
    if (r->State->LoadedTrack && r->OtherDeck && r->OtherDeck->LoadedTrack) {
        float pmY = wfY + S(4);
        float pmW = S(160);
        float pmX = wfLeft + (wfW / 2.0f) - (pmW / 2.0f);
        float pmH = S(8);
        
        // Background track
        DrawRectangleLines(pmX, pmY, pmW, pmH, ColorShadow);
        DrawLine(pmX + pmW / 2, pmY, pmX + pmW / 2, pmY + pmH, ColorOrange); // Center mark
        
        // Calculate offset
        int32_t myPhase = Quantize_GetPhaseErrorMs(r->State->LoadedTrack, r->State->PositionMs);
        int32_t otherPhase = Quantize_GetPhaseErrorMs(r->OtherDeck->LoadedTrack, r->OtherDeck->PositionMs);
        
        // Difference: positive means I am ahead of the other deck
        int32_t phaseDiff = myPhase - otherPhase;
        
        // Max displayable drift in ms: let's say 200ms is full scale (edge of meter)
        float maxDrift = 150.0f;
        float driftRatio = (float)phaseDiff / maxDrift;
        if (driftRatio > 1.0f) driftRatio = 1.0f;
        if (driftRatio < -1.0f) driftRatio = -1.0f;
        
        // Size of the block itself
        float blockW = S(16);
        
        // Depending on Sync mode, Pioneer phase meters behavior:
        // A single active block usually represents the current deck's phase position relative to the master.
        // We will render a block that moves relative to the center.
        float blockX = (pmX + pmW / 2) + (driftRatio * (pmW / 2.0f)) - (blockW / 2.0f);
        
        Color blockColor = ColorWhite;
        if (r->State->IsMaster) blockColor = ColorOrange;
        else if (r->State->SyncMode == 2 && abs(phaseDiff) < 5) blockColor = (Color){0, 255, 255, 255}; // Locked
        
        DrawRectangle(blockX, pmY + S(1), blockW, pmH - S(2), blockColor);
    }
}

void WaveformRenderer_Init(WaveformRenderer *r, int id, DeckState *state, DeckState *otherDeck) {
    r->base.Update = Waveform_Update;
    r->base.Draw = Waveform_Draw;
    r->ID = id;
    r->State = state;
    r->OtherDeck = otherDeck;
    r->cachedTrack = NULL;
    r->dynWfmFrames = 480;
    r->lastMouseX = 0;
}
