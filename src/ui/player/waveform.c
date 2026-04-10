#include "ui/player/waveform.h"
#include "logic/quantize.h"
#include "rlgl.h"
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

// Removed waveform rendering functions per user request.


#include <rlgl.h>

static void SpliteWaveformBands(unsigned char byteVal, float waveCenter, float gLow, float gMid, float gHigh, int *bAmp, int *mAmp, int *hAmp) {
  int amplitude = byteVal & 0x1F;
  int colorIdx = byteVal >> 5;

  float vertScale = 1.9f;
  float baseAmp = ((float)amplitude / 31.0f) * waveCenter * vertScale;
  
  float b = 0, m = 0, h = 0;

  // Emulate split logic based on color density
  if (colorIdx >= 6) {
    b = baseAmp * 0.95f;
    m = baseAmp * 0.85f;
    h = baseAmp * 0.60f;
  } else if (colorIdx >= 3) {
    b = baseAmp * 0.90f;
    m = baseAmp * 0.75f;
    h = baseAmp * 0.20f;
  } else {
    b = baseAmp * 0.85f;
    m = baseAmp * 0.40f;
    h = baseAmp * 0.05f;
  }

  b *= gLow; m *= gMid; h *= gHigh;

  if (b > waveCenter) b = waveCenter;
  if (m > b) m = b * 0.9f;
  if (h > m) h = m * 0.8f;

  *bAmp = (int)(b * 2.0f);
  *mAmp = (int)(m * 2.0f);
  *hAmp = (int)(h * 2.0f);
}

static unsigned char GetDynamicWaveformPeak(unsigned char *data, int64_t maxFrames, int64_t start, int64_t end) {
    if (start < 0) start = 0;
    if (end > maxFrames) end = maxFrames;
    if (start >= end) {
        if (start >= 0 && start < maxFrames) return data[start];
        return 0;
    }
    int maxAmp = -1;
    unsigned char best = 0;
    for (int64_t i = start; i < end; i++) {
        int amp = data[i] & 0x1F;
        if (amp > maxAmp) {
            maxAmp = amp;
            best = data[i];
        }
    }
    return best;
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

  // Exact background color from Teensy dj_screen.cpp
  Color bgTeensy = {26, 26, 26, 255}; 
  DrawRectangle(wfLeft, wfY, wfW, waveH, bgTeensy);

  float effectiveZoom = (float)r->State->ZoomScale;
  if (effectiveZoom < 0.1f) effectiveZoom = 0.1f;
  double elapsedHalfFrames = r->State->Position;

  float playheadX = wfLeft + wfW / 2.0f;

  BeginScissorMode((int)(wfLeft + UI_OffsetX), (int)(wfY + UI_OffsetY),
                   (int)wfW, (int)waveH);

  float waveCenter = waveH / 2.0f;
  float gLow = (r->State->Waveform.GainLow > 0) ? r->State->Waveform.GainLow : 1.0f;
  float gMid = (r->State->Waveform.GainMid > 0) ? r->State->Waveform.GainMid : 1.0f;
  float gHigh = (r->State->Waveform.GainHigh > 0) ? r->State->Waveform.GainHigh : 1.0f;

  Color colorBass = {16, 105, 238, 255};  // Teensy Blue
  Color colorMid = {16, 190, 82, 255};    // Teensy Green
  Color colorHigh = {246, 251, 246, 255}; // Teensy White

  // Continuous floating point step to maintain perfect synced alignment with Raylib BeatGrid.
  // Drawn using RL_QUADS for high-res anti-aliased seamless continuous overlapping strips.
  rlBegin(RL_QUADS);
  for (int x = 0; x < (int)wfW - 1; x++) {
    int64_t index1 = (int64_t)(elapsedHalfFrames * r->dataDensity + (x - (wfW / 2.0f)) * (effectiveZoom * r->dataDensity));
    int64_t index2 = (int64_t)(elapsedHalfFrames * r->dataDensity + ((x + 1) - (wfW / 2.0f)) * (effectiveZoom * r->dataDensity));
    int64_t index3 = (int64_t)(elapsedHalfFrames * r->dataDensity + ((x + 2) - (wfW / 2.0f)) * (effectiveZoom * r->dataDensity));
    
    if (index1 >= 0 || index2 < r->dynWfmFrames) {
      unsigned char sVal1 = GetDynamicWaveformPeak(r->State->LoadedTrack->DynamicWaveform, r->dynWfmFrames, index1, index2);
      unsigned char sVal2 = GetDynamicWaveformPeak(r->State->LoadedTrack->DynamicWaveform, r->dynWfmFrames, index2, index3);
      
      int b1, m1, h1, b2, m2, h2;
      SpliteWaveformBands(sVal1, waveCenter, gLow, gMid, gHigh, &b1, &m1, &h1);
      SpliteWaveformBands(sVal2, waveCenter, gLow, gMid, gHigh, &b2, &m2, &h2);

      float cx1 = wfLeft + x;
      float cx2 = wfLeft + x + 1.0f;
      float yy = wfY + waveCenter;

      if (b1 > 0 || b2 > 0) {
        rlColor4ub(colorBass.r, colorBass.g, colorBass.b, colorBass.a);
        rlVertex2f(cx1, yy - (b1 / 2.0f)); rlVertex2f(cx1, yy + (b1 / 2.0f));
        rlVertex2f(cx2, yy + (b2 / 2.0f)); rlVertex2f(cx2, yy - (b2 / 2.0f));
      }
      if (m1 > 0 || m2 > 0) {
        rlColor4ub(colorMid.r, colorMid.g, colorMid.b, colorMid.a);
        rlVertex2f(cx1, yy - (m1 / 2.0f)); rlVertex2f(cx1, yy + (m1 / 2.0f));
        rlVertex2f(cx2, yy + (m2 / 2.0f)); rlVertex2f(cx2, yy - (m2 / 2.0f));
      }
      if (h1 > 0 || h2 > 0) {
        rlColor4ub(colorHigh.r, colorHigh.g, colorHigh.b, colorHigh.a);
        rlVertex2f(cx1, yy - (h1 / 2.0f)); rlVertex2f(cx1, yy + (h1 / 2.0f));
        rlVertex2f(cx2, yy + (h2 / 2.0f)); rlVertex2f(cx2, yy - (h2 / 2.0f));
      }
    }
  }
  rlEnd();

  // Draw Horizontal mid-canvas line (Exact col_white from Teensy)
  DrawLineEx((Vector2){wfLeft, wfY + waveCenter}, (Vector2){wfRight, wfY + waveCenter}, 1.0f, colorHigh);

  // Beat Grid
  if (r->State->LoadedTrack != NULL) {
    for (int i = 0; i < 1024; i++) {
      unsigned int originalMs = r->State->LoadedTrack->BeatGrid[i].Time;
      uint16_t beatNum = r->State->LoadedTrack->BeatGrid[i].BeatNumber;
      if (originalMs == 0xFFFFFFFF || originalMs == 0) break;

      double beatPosHF = (double)originalMs * 0.105;
      float px = (float)((beatPosHF - elapsedHalfFrames) / (double)effectiveZoom);
      float bx = playheadX + px;

      if (bx >= wfLeft && bx <= wfRight) {
        // Teensy beatgrid exact port
        Color white_190 = {189, 190, 189, 255}; 
        Color tickColor = (beatNum == 1) ? ColorRed : white_190;
        
        float tickHeight = 10.0f;
        
        // Top tick (3 pixels wide)
        DrawRectangleV((Vector2){bx - 1.0f, wfY}, (Vector2){3.0f, tickHeight}, tickColor);
        
        // Middle section (1 pixel wide, always white_190)
        DrawRectangleV((Vector2){bx, wfY + tickHeight}, (Vector2){1.0f, waveH - (2.0f * tickHeight)}, white_190);
        
        // Bottom tick (3 pixels wide)
        float tickBottomStart = waveH - tickHeight;
        DrawRectangleV((Vector2){bx - 1.0f, wfY + tickBottomStart}, (Vector2){3.0f, tickHeight}, tickColor);
      }
    }
  }

  // Draw vertical mid-canvas play head line
  DrawLineEx((Vector2){playheadX, wfY}, (Vector2){playheadX, wfY + waveH}, 1.5f, colorHigh);
  
  if (r->ID == 0) {
    DrawLineEx((Vector2){wfLeft, wfY + waveH - 1}, (Vector2){wfLeft + wfW, wfY + waveH - 1}, 1.5f, ColorDark1);
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

    int32_t myPhase = Quantize_GetPhaseErrorMs(r->State->LoadedTrack, r->State->PositionMs);
    int32_t otherPhase = Quantize_GetPhaseErrorMs(r->OtherDeck->LoadedTrack, r->OtherDeck->PositionMs);

    int32_t phaseDiff = myPhase - otherPhase;

    float maxDrift = 150.0f;
    float driftRatio = (float)phaseDiff / maxDrift;
    if (driftRatio > 1.0f) driftRatio = 1.0f;
    if (driftRatio < -1.0f) driftRatio = -1.0f;

    float blockW = S(16);
    float blockX = (pmX + pmW / 2) + (driftRatio * (pmW / 2.0f)) - (blockW / 2.0f);

    Color blockColor = ColorWhite;
    if (r->State->IsMaster) blockColor = ColorOrange;
    else if (r->State->SyncMode == 2 && abs(phaseDiff) < 5) blockColor = (Color){0, 255, 255, 255};

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
