#include "ui/player/waveform.h"
#include "logic/quantize.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int Waveform_Update(Component *base) {
  WaveformRenderer *r = (WaveformRenderer *)base;

  // Refresh dynamic frame count when track changes
  if (r->State->LoadedTrack != r->cachedTrack) {
    r->cachedTrack = r->State->LoadedTrack;
    if (r->cachedTrack) {
      r->dynWfmFrames = r->cachedTrack->DynamicWaveformLen;

      // Calculate data density: how many data frames exist per UI frame (105Hz)
      // UI_Total_Frames = TrackLengthMs * 0.105
      float totalUIFrames = (float)r->State->TrackLengthMs * 0.105f;
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
  bool inWaveform = (mouse.x >= wfLeft && mouse.x <= wfRight &&
                     mouse.y >= wfY && mouse.y <= wfY + waveH);

  // Zoom & Jog Interaction Logic
  if (inWaveform) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      r->State->ZoomScale -= wheel * 0.5f;
      if (r->State->ZoomScale < 0.25f)
        r->State->ZoomScale = 0.25f;
      if (r->State->ZoomScale > 32.0f)
        r->State->ZoomScale = 32.0f;
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
  if (effectiveZoom < 0.1f)
    effectiveZoom = 0.1f;

  double elapsedHalfFrames = r->State->Position;

  unsigned char *dynData = r->State->LoadedTrack->DynamicWaveform;
  float waveCenter = waveH / 2.0f;

  float fAggZoom = effectiveZoom * r->dataDensity;
  if (fAggZoom < 1.0f)
    fAggZoom = 1.0f;

  // --- Boundary Setup ---
  // Scissor everything to prevent rendering outside deck area
  // BeginScissorMode expects raw screen coordinates, so we must add the UI
  // offset
  BeginScissorMode((int)(wfLeft + UI_OffsetX), (int)(wfY + UI_OffsetY),
                   (int)wfW, (int)waveH);

  DrawRectangle(wfLeft, wfY, wfW, waveH, ColorBlack);

  // Store previous amplitudes for continuous shape rendering

  if (dynData != NULL && r->dynWfmFrames > 0) {
    // Pre-calculate constants for the loop
    double baseDataPos = elapsedHalfFrames * (double)r->dataDensity;
    double zoomStep = (double)effectiveZoom * (double)r->dataDensity;
    float centerOffset = wfW / 2.0f;

    // Anti-flicker: Use stable fractional sampling with Pixel-Binning
    for (float screenX = 0; screenX <= wfW + 1; screenX += 1.0f) {
      double binCenter =
          baseDataPos + (double)(screenX - centerOffset) * zoomStep;
      double binStart = binCenter - zoomStep * 0.5;
      double binEnd = binCenter + zoomStep * 0.5;

      if (binEnd >= 0 && binStart < (double)r->dynWfmFrames) {
        float maxAmp = 0;
        float fSumColor = 0;
        float totalWeight = 0;

        // Iterate through all samples that overlap with this pixel's bin
        int startIdx = (int)floor(binStart);
        int endIdx = (int)floor(binEnd);

        if (startIdx < 0)
          startIdx = 0;
        if (endIdx >= r->dynWfmFrames)
          endIdx = r->dynWfmFrames - 1;

        for (int i = startIdx; i <= endIdx; i++) {
          float sampleWeight = 1.0f;

          // Fractional weight for the boundary samples
          if (i == startIdx) {
            sampleWeight = (float)(1.0 - (binStart - (double)startIdx));
            if (sampleWeight > 1.0f)
              sampleWeight = 1.0f;
            if (sampleWeight < 0.0f)
              sampleWeight = 0.0f;
          } else if (i == endIdx) {
            sampleWeight = (float)(binEnd - (double)endIdx);
            if (sampleWeight > 1.0f)
              sampleWeight = 1.0f;
            if (sampleWeight < 0.0f)
              sampleWeight = 0.0f;
          }

          if (sampleWeight <= 0)
            continue;

          float amp = (float)(dynData[i] & 0x1F);
          float col = (float)(dynData[i] >> 5);

          // PEAK HOLD: We take the sample-weighted peak
          // This keeps the vertical scale stable as samples move in/out
          if (amp > maxAmp)
            maxAmp = amp;

          fSumColor += col * sampleWeight;
          totalWeight += sampleWeight;
        }

        // Adjust amplitude
        int amplitude = (int)(maxAmp);
        int colorIdx =
            (totalWeight > 0) ? (int)(fSumColor / totalWeight) : 0;

        // Apply Gains
        float gLow = (r->State->Waveform.GainLow > 0)
                         ? r->State->Waveform.GainLow
                         : 1.0f;
        float gMid = (r->State->Waveform.GainMid > 0)
                         ? r->State->Waveform.GainMid
                         : 1.0f;
        float gHigh = (r->State->Waveform.GainHigh > 0)
                          ? r->State->Waveform.GainHigh
                          : 1.0f;

        // --- Calibration Parameters ---
        float vertScale = 1.9f;
        float baseAmp = (float)amplitude * vertScale;
        float bAmp = 0, mAmp = 0, hAmp = 0;

        // Frequency splitting (Heuristic)
        if (colorIdx >= 6) {
          bAmp = baseAmp * 0.95f;
          mAmp = baseAmp * 0.85f;
          hAmp = baseAmp * 0.60f;
        } else if (colorIdx >= 3) {
          bAmp = baseAmp * 0.90f;
          mAmp = baseAmp * 0.75f;
          hAmp = baseAmp * 0.20f;
        } else {
          bAmp = baseAmp * 0.85f;
          mAmp = baseAmp * 0.40f;
          hAmp = baseAmp * 0.05f;
        }

        // Apply user gains
        bAmp *= gLow;
        mAmp *= gMid;
        hAmp *= gHigh;

        // Ensure visibility for 3BAND even if heuristic is weak
        if (r->State->Waveform.Style == WAVEFORM_STYLE_3BAND) {
          if (mAmp < bAmp * 0.3f)
            mAmp = bAmp * 0.3f;
          if (hAmp < mAmp * 0.3f)
            hAmp = mAmp * 0.3f;
        }

        if (bAmp > waveCenter - 1.0f)
          bAmp = waveCenter - 1.0f;
        if (mAmp > bAmp)
          mAmp = bAmp * 0.9f;
        if (hAmp > mAmp)
          hAmp = mAmp * 0.8f;

        float px = wfLeft + screenX;

        if (px >= wfLeft - 2.0f && px <= wfRight + 2.0f) {
          WaveformStyle style = r->State->Waveform.Style;

          if (style == WAVEFORM_STYLE_3BAND || style == WAVEFORM_STYLE_SHAPE) {
            // Layered Vector Style (Teensy Look)
            Color colorBass = {16, 105, 238, 255};  // Teensy Blue
            Color colorMid = {16, 190, 82, 255};    // Teensy Green
            Color colorHigh = {246, 251, 246, 255}; // Teensy White

            if (style == WAVEFORM_STYLE_SHAPE) {
              // Solid colors for Teensy shape match
              colorBass.a = 255;
              colorMid.a = 255;
              colorHigh.a = 255;
            }

            // Bass (Blue) - Outermost
            DrawRectangle(px, wfY + waveCenter - bAmp, 1, (int)(bAmp * 2),
                          colorBass);

            // Mid (Green)
            if (mAmp > 0) {
              DrawRectangle(px, wfY + waveCenter - mAmp, 1, (int)(mAmp * 2),
                            colorMid);
            }

            // High (White) - Innermost
            if (hAmp > 0) {
              DrawRectangle(px, wfY + waveCenter - hAmp, 1, (int)(hAmp * 2),
                            colorHigh);
            }

            // Horizontal Axis (Subtle white line)
            DrawLineEx((Vector2){px, wfY + waveCenter},
                       (Vector2){px + 1, wfY + waveCenter}, 1.0f,
                       (Color){255, 255, 255, 60});
          } else if (style == WAVEFORM_STYLE_RGB) {
            // Mixxx style RGB (Amplitude-based mixing)
            float total = bAmp + mAmp + hAmp + 0.001f;
            Color mix;
            mix.r =
                (unsigned char)((bAmp * 0 + mAmp * 255 + hAmp * 255) / total);
            mix.g =
                (unsigned char)((bAmp * 120 + mAmp * 255 + hAmp * 50) / total);
            mix.b =
                (unsigned char)((bAmp * 255 + mAmp * 20 + hAmp * 50) / total);
            mix.a = 255;

            DrawRectangle(px, wfY + waveCenter - bAmp, 1, (int)(bAmp * 2), mix);
          } else {
            // WAVEFORM_STYLE_BLUE (Classic Pioneer)
            Color colorBlue = {0, 100, 255, 255};
            float highlight = hAmp / (waveCenter + 0.001f);
            if (highlight > 1)
              highlight = 1;
            Color mix = colorBlue;
            mix.r += (unsigned char)(highlight * 150);
            mix.g += (unsigned char)(highlight * 100);

            DrawRectangle(px, wfY + waveCenter - bAmp, 1, (int)(bAmp * 2), mix);
          }
        }
      }
    }
  }

  EndScissorMode();

  // Beat Grid
  if (r->State->LoadedTrack != NULL) {
    for (int i = 0; i < 1024; i++) {
      unsigned int originalMs = r->State->LoadedTrack->BeatGrid[i].Time;
      uint16_t beatNum = r->State->LoadedTrack->BeatGrid[i].BeatNumber;
      if (originalMs == 0xFFFFFFFF || originalMs == 0)
        break;

      double beatPosHF = (double)originalMs * 0.105;
      float px =
          (float)((beatPosHF - elapsedHalfFrames) / (double)effectiveZoom);
      float bx = playheadX + px;

      if (bx >= wfLeft && bx <= wfRight) {
        Color clr = (Color){255, 255, 255, 50};
        float tw = 1.0f;

        if (beatNum == 1) {
          clr = (Color){255, 0, 0, 100};
          tw = 2.0f;
        }

        float segmentH = waveH * 0.12f;
        DrawRectangleV((Vector2){bx - (tw / 2.0f), wfY},
                       (Vector2){tw, segmentH}, clr);
        DrawRectangleV((Vector2){bx - (tw / 2.0f), wfY + waveH - segmentH},
                       (Vector2){tw, segmentH}, clr);
      }
    }
  }

  // HotCue Markers
  Color hcColors[] = {{0, 255, 0, 255},   {255, 0, 0, 255},  {255, 128, 0, 255},
                      {255, 255, 0, 255}, {0, 0, 255, 255},  {255, 0, 255, 255},
                      {0, 255, 255, 255}, {128, 0, 255, 255}};
  const char *hcLabels[] = {"A", "B", "C", "D", "E", "F", "G", "H"};

  // 1. Draw Memory Cues (white lines)
  for (int i = 0; i < r->State->LoadedTrack->CuesCount; i++) {
    HotCue *mc = &r->State->LoadedTrack->Cues[i];
    double mcHF = (double)mc->Start * 0.105;
    float hx =
        playheadX + (float)((mcHF - elapsedHalfFrames) / (double)effectiveZoom);
    if (hx >= wfLeft && hx <= wfRight) {
      DrawLineEx((Vector2){hx, wfY}, (Vector2){hx, wfY + waveH}, 1.2f,
                 ColorWhite);
    }
  }

  // 2. Draw HotCues (flags)
  for (int i = 0; i < r->State->LoadedTrack->HotCuesCount; i++) {
    HotCue *hc = &r->State->LoadedTrack->HotCues[i];
    int hcIdx = hc->ID - 1;
    if (hcIdx < 0 || hcIdx >= 8)
      continue;

    double hcHF = (double)hc->Start * 0.105;
    float hx =
        playheadX + (float)((hcHF - elapsedHalfFrames) / (double)effectiveZoom);

    if (hx >= wfLeft && hx <= wfRight) {
      Color clr = hcColors[hcIdx];

      unsigned char cR = hc->Color[0];
      unsigned char cG = hc->Color[1];
      unsigned char cB = hc->Color[2];
      if (cR != 0 || cG != 0 || cB != 0) {
        clr = (Color){cR, cG, cB, 255};
      }

      DrawLineEx((Vector2){hx, wfY}, (Vector2){hx, wfY + waveH}, 1.5f, clr);

      // Flag
      float fw = S(14);
      DrawRectangle(hx, wfY, fw, fw, clr);
      UIDrawText(hcLabels[hcIdx], UIFonts_GetFace(S(10)), hx + S(3), wfY + S(1),
                 S(10), ColorWhite);
    }
  }

  DrawLineEx((Vector2){playheadX, wfY}, (Vector2){playheadX, wfY + waveH}, 1.5f,
             ColorRed);
  if (r->ID == 0) {
    DrawLineEx((Vector2){wfLeft, wfY + waveH - 1},
               (Vector2){wfLeft + wfW, wfY + waveH - 1}, 1.5f, ColorDark1);
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
    DrawLine(pmX + pmW / 2, pmY, pmX + pmW / 2, pmY + pmH,
             ColorOrange); // Center mark

    // Calculate offset
    int32_t myPhase =
        Quantize_GetPhaseErrorMs(r->State->LoadedTrack, r->State->PositionMs);
    int32_t otherPhase = Quantize_GetPhaseErrorMs(r->OtherDeck->LoadedTrack,
                                                  r->OtherDeck->PositionMs);

    // Difference: positive means I am ahead of the other deck
    int32_t phaseDiff = myPhase - otherPhase;

    // Max displayable drift in ms: let's say 200ms is full scale (edge of
    // meter)
    float maxDrift = 150.0f;
    float driftRatio = (float)phaseDiff / maxDrift;
    if (driftRatio > 1.0f)
      driftRatio = 1.0f;
    if (driftRatio < -1.0f)
      driftRatio = -1.0f;

    // Size of the block itself
    float blockW = S(16);

    // Depending on Sync mode, Pioneer phase meters behavior:
    // A single active block usually represents the current deck's phase
    // position relative to the master. We will render a block that moves
    // relative to the center.
    float blockX =
        (pmX + pmW / 2) + (driftRatio * (pmW / 2.0f)) - (blockW / 2.0f);

    Color blockColor = ColorWhite;
    if (r->State->IsMaster)
      blockColor = ColorOrange;
    else if (r->State->SyncMode == 2 && abs(phaseDiff) < 5)
      blockColor = (Color){0, 255, 255, 255}; // Locked

    DrawRectangle(blockX, pmY + S(1), blockW, pmH - S(2), blockColor);
  }
}

void WaveformRenderer_Init(WaveformRenderer *r, int id, DeckState *state,
                           DeckState *otherDeck) {
  r->base.Update = Waveform_Update;
  r->base.Draw = Waveform_Draw;
  r->ID = id;
  r->State = state;
  r->OtherDeck = otherDeck;
  r->cachedTrack = NULL;
  r->dynWfmFrames = 480;
  r->lastMouseX = 0;
}
