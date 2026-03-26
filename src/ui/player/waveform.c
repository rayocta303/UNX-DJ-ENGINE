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

void Waveform_DrawGeneric(
    Rectangle bounds, unsigned char *data, int dataLen,
    double positionHalfFrames, // For scrolling: playhead pos. For static: unused/0
    float zoomStep,         // For scrolling: samples per pixel. For static: totalLen/width
    bool isStatic,          // True for deckstrip overview, False for scrolling track
    WaveformStyle style, float gLow, float gMid, float gHigh,
    float playedRatio,      // 0.0-1.0 to dim played area (only for static)
    float startScreenX,     // Usually 0
    float endScreenX        // Usually bounds.width
) {
  if (data == NULL || dataLen <= 0)
    return;

  float waveCenterY = bounds.y + (bounds.height / 2.0f);

  BeginScissorMode((int)(bounds.x + UI_OffsetX), (int)(bounds.y + UI_OffsetY),
                   (int)bounds.width, (int)bounds.height);

  if (isStatic) {
    // Static base line
    DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height - 1), (int)bounds.width, 1, (Color){22, 110, 240, 255});

    for (float screenX = startScreenX; screenX <= endScreenX; screenX += 1.0f) {
      float px = bounds.x + screenX;

      double binStart = (double)(screenX / bounds.width) * (double)dataLen;
      double binEnd = (double)((screenX + 1.0f) / bounds.width) * (double)dataLen;

      int startIdx = (int)floor(binStart);
      int endIdx = (int)ceil(binEnd);
      if (endIdx <= startIdx) endIdx = startIdx + 1;

      if (startIdx < 0) startIdx = 0;
      if (endIdx > dataLen) endIdx = dataLen;

      if (startIdx < dataLen) {
        float maxAmp = 0;
        unsigned char firstColor = data[startIdx] >> 5;

        for (int i = startIdx; i < endIdx; i++) {
          float amp = (float)(data[i] & 0x1F);
          if (amp > maxAmp) maxAmp = amp;
        }

        float totalGain = (gLow + gMid + gHigh) / 3.0f;
        if (totalGain <= 0.01f) totalGain = 1.0f;

        float amplitudeZ = maxAmp * totalGain;
        float maxBarH = bounds.height;
        float ampBarH = (amplitudeZ / 31.0f) * maxBarH;
        if (ampBarH > maxBarH) ampBarH = maxBarH;

        Color drawColor = (Color){22, 110, 240, 255}; 
        if (style == WAVEFORM_STYLE_RGB) {
          if (firstColor >= 6) drawColor = (Color){255, 100, 100, 255};
          else if (firstColor >= 3) drawColor = (Color){100, 255, 100, 255};
        } else if (style == WAVEFORM_STYLE_3BAND || style == WAVEFORM_STYLE_SHAPE) {
          float highlight = amplitudeZ / 31.0f;
          if (highlight > 1.0f) highlight = 1.0f;
          drawColor.r += (unsigned char)(highlight * 100);
          drawColor.g += (unsigned char)(highlight * 80);
        }

        bool isPlayed = (screenX / bounds.width) <= playedRatio;
        if (isPlayed) {
          drawColor.r /= 2; drawColor.g /= 2; drawColor.b /= 2; drawColor.a = 150;
        }

        DrawRectangle((int)px, (int)(bounds.y + bounds.height - ampBarH), 1, (int)ampBarH, drawColor);
      }
    }
  } else {
    // Dynamic smooth-scrolling view aligned to data to prevent horizontal flickering/gaps
    float centerScreenOffset = (endScreenX - startScreenX) / 2.0f;
    
    // Base line for entire area
    DrawRectangle((int)(bounds.x + startScreenX), (int)(waveCenterY - 1), (int)(endScreenX - startScreenX), 2, (Color){22, 110, 240, 255});

    double firstVisibleData = positionHalfFrames - (centerScreenOffset * zoomStep);
    double lastVisibleData = positionHalfFrames + (centerScreenOffset * zoomStep);

    int startDataIdx = (int)floor(firstVisibleData);
    if (startDataIdx < 0) startDataIdx = 0;
    
    int endDataIdx = (int)ceil(lastVisibleData);
    if (endDataIdx > dataLen) endDataIdx = dataLen;

    float zCountF = zoomStep;
    if (zCountF < 1.0f) zCountF = 1.0f;
    int zCount = (int)zCountF;

    startDataIdx = (startDataIdx / zCount) * zCount;

    for (int i = startDataIdx; i < endDataIdx; i += zCount) {
      int endI = i + zCount;
      if (endI > dataLen) endI = dataLen;

      float maxAmp = 0;
      float sumAmp = 0;
      int count = 0;
      unsigned char firstColor = (i >= 0 && i < dataLen) ? (data[i] >> 5) : 0;

      for (int j = i; j < endI; j++) {
        float amp = (float)(data[j] & 0x1F);
        if (amp > maxAmp) maxAmp = amp;
        sumAmp += amp;
        count++;
      }

      float avgAmp = count > 0 ? (sumAmp / count) : 0;
      float rmsVal = count > 1 ? avgAmp : (maxAmp * 0.7f);

      float totalGain = (gLow + gMid + gHigh) / 3.0f;
      if (totalGain <= 0.01f) totalGain = 1.0f;

      float amplitudeZ = maxAmp * totalGain;
      float rmsZ = rmsVal * totalGain;

      float maxBarH = (bounds.height / 2.0f) - 1.0f;
      float ampBarH = (amplitudeZ / 31.0f) * maxBarH;
      float rmsBarH = (rmsZ / 31.0f) * maxBarH;

      if (ampBarH > maxBarH) ampBarH = maxBarH;
      if (rmsBarH > maxBarH) rmsBarH = maxBarH;

      Color drawColor = (Color){22, 110, 240, 255}; 
      Color rmsColor = ColorWhite;

      if (style == WAVEFORM_STYLE_RGB) {
        if (firstColor >= 6) drawColor = (Color){255, 100, 100, 255};
        else if (firstColor >= 3) drawColor = (Color){100, 255, 100, 255};
      } else if (style == WAVEFORM_STYLE_3BAND || style == WAVEFORM_STYLE_SHAPE) {
        float highlight = amplitudeZ / 31.0f;
        if (highlight > 1.0f) highlight = 1.0f;
        drawColor.r += (unsigned char)(highlight * 100);
        drawColor.g += (unsigned char)(highlight * 80);
      }

      float screenX1 = (float)((i - positionHalfFrames) / zoomStep) + centerScreenOffset;
      float screenX2 = (float)((endI - positionHalfFrames) / zoomStep) + centerScreenOffset;

      if(screenX2 < 0) continue;
      if(screenX1 > endScreenX - startScreenX) continue;

      float barW = screenX2 - screenX1;
      barW = ceilf(barW);
      if (barW < 1.0f) barW = 1.0f; 

      Rectangle ampRect = {
          bounds.x + startScreenX + screenX1,
          waveCenterY - ampBarH - 1.0f,
          barW,
          2.0f + 2.0f * ampBarH
      };
      DrawRectangleRec(ampRect, drawColor);

      Rectangle rmsRect = {
          bounds.x + startScreenX + screenX1,
          waveCenterY - rmsBarH - 1.0f,
          barW,
          2.0f + 2.0f * rmsBarH
      };
      DrawRectangleRec(rmsRect, rmsColor);
    }
  }

  EndScissorMode();
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

  DrawRectangle(wfLeft, wfY, wfW, waveH, ColorBlack);

  float effectiveZoom = (float)r->State->ZoomScale;
  if (effectiveZoom < 0.1f)
    effectiveZoom = 0.1f;
  double elapsedHalfFrames = r->State->Position;

  float fAggZoom = effectiveZoom * r->dataDensity;
  if (fAggZoom < 1.0f)
    fAggZoom = 1.0f;

  float gLow =
      (r->State->Waveform.GainLow > 0) ? r->State->Waveform.GainLow : 1.0f;
  float gMid =
      (r->State->Waveform.GainMid > 0) ? r->State->Waveform.GainMid : 1.0f;
  float gHigh =
      (r->State->Waveform.GainHigh > 0) ? r->State->Waveform.GainHigh : 1.0f;

  Rectangle bounds = {wfLeft, wfY, wfW, waveH};

  // 1. Draw dynamic scrolling waveform via generic function
  Waveform_DrawGeneric(
      bounds, r->State->LoadedTrack->DynamicWaveform, r->dynWfmFrames,
      elapsedHalfFrames * (double)r->dataDensity,
      (float)(effectiveZoom * r->dataDensity),
      false, // scrolling mode
      r->State->Waveform.Style, gLow, gMid, gHigh, 0.0f, 0.0f, wfW + 1.0f);

  float playheadX = wfLeft + wfW / 2.0f;

  BeginScissorMode((int)(wfLeft + UI_OffsetX), (int)(wfY + UI_OffsetY),
                   (int)wfW, (int)waveH);

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
  if (r->State->LoadedTrack && r->OtherDeck && r->OtherDeck->LoadedTrack) {
    float pmY = wfY + S(4);
    float pmW = S(160);
    float pmX = wfLeft + (wfW / 2.0f) - (pmW / 2.0f);
    float pmH = S(8);

    DrawRectangleLines(pmX, pmY, pmW, pmH, ColorShadow);
    DrawLine(pmX + pmW / 2, pmY, pmX + pmW / 2, pmY + pmH, ColorOrange);

    int32_t myPhase =
        Quantize_GetPhaseErrorMs(r->State->LoadedTrack, r->State->PositionMs);
    int32_t otherPhase = Quantize_GetPhaseErrorMs(r->OtherDeck->LoadedTrack,
                                                  r->OtherDeck->PositionMs);

    int32_t phaseDiff = myPhase - otherPhase;

    float maxDrift = 150.0f;
    float driftRatio = (float)phaseDiff / maxDrift;
    if (driftRatio > 1.0f)
      driftRatio = 1.0f;
    if (driftRatio < -1.0f)
      driftRatio = -1.0f;

    float blockW = S(16);
    float blockX =
        (pmX + pmW / 2) + (driftRatio * (pmW / 2.0f)) - (blockW / 2.0f);

    Color blockColor = ColorWhite;
    if (r->State->IsMaster)
      blockColor = ColorOrange;
    else if (r->State->SyncMode == 2 && abs(phaseDiff) < 5)
      blockColor = (Color){0, 255, 255, 255};

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
