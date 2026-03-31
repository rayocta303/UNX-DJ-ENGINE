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

static Color GetFreqColor(float r, float g, float b) {
  // Determine dominant hue based on RGB proportions
  float maxVal = fmaxf(r, fmaxf(g, b));
  float minVal = fminf(r, fminf(g, b));
  float h = 0;
  if (maxVal > 0 && maxVal != minVal) {
    float d = maxVal - minVal;
    if (maxVal == r)
      h = (g - b) / d + (g < b ? 6 : 0);
    else if (maxVal == g)
      h = (b - r) / d + 2;
    else
      h = (r - g) / d + 4;
    h /= 6.0f;
  }
  // High saturation and medium lightness for vibrant "gelombang" look
  return ColorFromHSV(h * 360.0f, 0.70f, 0.50f);
}

// Samples amplitude and color, using linear interpolation if zoomed in enough
// This guarantees a smooth continuous spline (bentuk sinewave) instead of
// discrete boxes (garis dijajarkan)
static float SampleMaxAmplitude(unsigned char *data, int dataLen,
                                double exactIdx, float width, int *outColor) {
  if (exactIdx < 0.0 || exactIdx >= (double)dataLen) {
    if (outColor)
      *outColor = 0;
    return 0.0f;
  }

  int idx1 = (int)exactIdx;

  if (width <= 1.0f) {
    float frac = (float)(exactIdx - idx1);
    int idx2 = (idx1 + 1 < dataLen) ? idx1 + 1 : idx1;
    float a1 = (float)(data[idx1] & 0x1F);
    float a2 = (float)(data[idx2] & 0x1F);
    if (outColor)
      *outColor = data[idx1] >> 5;
    return a1 * (1.0f - frac) + a2 * frac;
  } else {
    int idx2 = (int)(exactIdx + width);
    if (idx2 >= dataLen)
      idx2 = dataLen - 1;
    float maxA = 0;
    int maxColor = data[idx1] >> 5;
    for (int i = idx1; i <= idx2; i++) {
      float a = (float)(data[i] & 0x1F);
      if (a > maxA) {
        maxA = a;
        maxColor = data[i] >> 5;
      }
    }
    if (outColor)
      *outColor = maxColor;
    return maxA;
  }
}

static void Waveform_DrawBlue(Rectangle bounds, float centerOffset,
                              unsigned char *data, int dataLen, double position,
                              float zoomStep, bool isStatic, float playedRatio,
                              float startX, float endX) {
  float centerY = bounds.y + (bounds.height / 2.0f);
  Color waveColor = (Color){22, 110, 240, 255};
  Color rmsColor = (Color){100, 180, 255, 255};
  float z = isStatic ? ((float)dataLen / bounds.width) : zoomStep;

  for (float x = startX; x < endX; x += 1.0f) {
    float nextX = x + 1.0f;
    double d1 = isStatic ? (double)(x / bounds.width * dataLen)
                         : (position + (x - centerOffset) * z);
    double d2 = isStatic ? (double)(nextX / bounds.width * dataLen)
                         : (position + (nextX - centerOffset) * z);

    float a1 = SampleMaxAmplitude(data, dataLen, d1, z, NULL);
    float a2 = SampleMaxAmplitude(data, dataLen, d2, z, NULL);

    float h1 = (a1 / 31.0f) * (bounds.height / 2.0f),
          h2 = (a2 / 31.0f) * (bounds.height / 2.0f);
    // RMS overlay approximated for smooth gelombang shape.
    float rh1 = h1 * 0.7f, rh2 = h2 * 0.7f;

    Color cW = waveColor, cR = rmsColor;
    if (isStatic && x / bounds.width <= playedRatio) {
      cW.r >>= 1;
      cW.g >>= 1;
      cW.b >>= 1;
      cW.a = 150;
      cR.r >>= 1;
      cR.g >>= 1;
      cR.b >>= 1;
      cR.a = 150;
    }

    rlBegin(RL_QUADS);
    rlColor4ub(cW.r, cW.g, cW.b, cW.a);
    rlVertex2f(bounds.x + x, centerY - h1);
    rlVertex2f(bounds.x + x, centerY + h1);
    rlVertex2f(bounds.x + nextX, centerY + h2);
    rlVertex2f(bounds.x + nextX, centerY - h2);

    rlColor4ub(cR.r, cR.g, cR.b, cR.a);
    rlVertex2f(bounds.x + x, centerY - rh1);
    rlVertex2f(bounds.x + x, centerY + rh1);
    rlVertex2f(bounds.x + nextX, centerY + rh2);
    rlVertex2f(bounds.x + nextX, centerY - rh2);
    rlEnd();
  }
}

static void Waveform_DrawRGB(Rectangle bounds, float centerOffset,
                             unsigned char *data, int dataLen, double position,
                             float zoomStep, bool isStatic, float playedRatio,
                             float startX, float endX) {
  float centerY = bounds.y + (bounds.height / 2.0f);
  float z = isStatic ? ((float)dataLen / bounds.width) : zoomStep;

  for (float x = startX; x < endX; x += 1.0f) {
    float nextX = x + 1.0f;
    double d1 = isStatic ? (double)(x / bounds.width * dataLen)
                         : (position + (x - centerOffset) * z);
    double d2 = isStatic ? (double)(nextX / bounds.width * dataLen)
                         : (position + (nextX - centerOffset) * z);

    int c1, c2;
    float a1 = SampleMaxAmplitude(data, dataLen, d1, z, &c1);
    float a2 = SampleMaxAmplitude(data, dataLen, d2, z, &c2);

    float iL1 = (c1 < 3) ? a1 : a1 * 0.4f,
          iM1 = (c1 >= 3 && c1 < 6) ? a1 : (c1 >= 6 ? a1 * 0.6f : a1 * 0.2f),
          iH1 = (c1 >= 6) ? a1 : a1 * 0.2f;
    float iL2 = (c2 < 3) ? a2 : a2 * 0.4f,
          iM2 = (c2 >= 3 && c2 < 6) ? a2 : (c2 >= 6 ? a2 * 0.6f : a2 * 0.2f),
          iH2 = (c2 >= 6) ? a2 : a2 * 0.2f;

    Color clr1 = GetFreqColor(iH1, iM1, iL1),
          clr2 = GetFreqColor(iH2, iM2, iL2);
    if (isStatic && x / bounds.width <= playedRatio) {
      clr1.r >>= 1;
      clr1.g >>= 1;
      clr1.b >>= 1;
      clr1.a = 150;
      clr2.r >>= 1;
      clr2.g >>= 1;
      clr2.b >>= 1;
      clr2.a = 150;
    }

    float h1 = (a1 / 31.0f) * (bounds.height / 2.0f);
    float h2 = (a2 / 31.0f) * (bounds.height / 2.0f);

    rlBegin(RL_QUADS);
    rlColor4ub(
        clr1.r, clr1.g, clr1.b,
        clr1.a); // Assuming gentle gradient isn't strictly required per-vertex
                 // for exact copy, but we can interpolate clr1
    rlVertex2f(bounds.x + x, centerY - h1);
    rlVertex2f(bounds.x + x, centerY + h1);
    rlColor4ub(clr2.r, clr2.g, clr2.b, clr2.a);
    rlVertex2f(bounds.x + nextX, centerY + h2);
    rlVertex2f(bounds.x + nextX, centerY - h2);

    // Highlight Overlay for "gelombang" depth
    rlColor4ub(255, 255, 255, 40);
    rlVertex2f(bounds.x + x, centerY - h1 * 0.7f);
    rlVertex2f(bounds.x + x, centerY + h1 * 0.7f);
    rlVertex2f(bounds.x + nextX, centerY + h2 * 0.7f);
    rlVertex2f(bounds.x + nextX, centerY - h2 * 0.7f);
    rlEnd();
  }
}

static void Waveform_Draw3Band(Rectangle bounds, float centerOffset,
                               unsigned char *data, int dataLen,
                               double position, float zoomStep, bool isStatic,
                               float playedRatio, float gL, float gM, float gH,
                               float startX, float endX) {
  float centerY = bounds.y + (bounds.height / 2.0f);
  Color colL = (Color){0, 85, 225, 255};    // Blue #0055e1
  Color colM = (Color){255, 166, 0, 255};   // Orange #ffa600
  Color colH = (Color){255, 255, 255, 255}; // White #ffffff

  const float pL = 0.1665f, pM = 0.333f, pH = 0.666f;
  const float sL = 0.7f, sM = 0.4f, sH = 0.2f;

  float z = isStatic ? ((float)dataLen / bounds.width) : zoomStep;

  for (float x = startX; x < endX; x += 1.0f) {
    float nextX = x + 1.0f;
    double d1 = isStatic ? (double)(x / bounds.width * dataLen)
                         : (position + (x - centerOffset) * z);
    double d2 = isStatic ? (double)(nextX / bounds.width * dataLen)
                         : (position + (nextX - centerOffset) * z);

    int c1, c2;
    float r1 = SampleMaxAmplitude(data, dataLen, d1, z, &c1);
    float r2 = SampleMaxAmplitude(data, dataLen, d2, z, &c2);

    // Scale amplitude (0-31 to 0.0-1.0 range)
    float n1 = r1 / 31.0f;
    float n2 = r2 / 31.0f;

    // Filter simulation using color index (0=Low, 7=High)
    // Low drops to 40% at high freq, Mid peaks at 100% in center, High rises to
    // 100% at high freq.
    float m1L = 1.0f - (c1 / 7.0f) * 0.6f;
    float m1M = 1.0f - fabsf((float)c1 - 3.5f) / 3.5f * 0.7f;
    float m1H = 0.2f + (c1 / 7.0f) * 0.8f;

    float m2L = 1.0f - (c2 / 7.0f) * 0.6f;
    float m2M = 1.0f - fabsf((float)c2 - 3.5f) / 3.5f * 0.7f;
    float m2H = 0.2f + (c2 / 7.0f) * 0.8f;

    float halfH = bounds.height / 2.0f;

    // Ported from three_band.py standard scaling (power, scale) mapped with
    // artificial filtered frequency
    float h1L = powf(n1 * m1L, pL) * sL * gL * halfH;
    float h2L = powf(n2 * m2L, pL) * sL * gL * halfH;

    float h1M = powf(n1 * m1M, pM) * sM * gM * halfH;
    float h2M = powf(n2 * m2M, pM) * sM * gM * halfH;

    float h1H = powf(n1 * m1H, pH) * sH * gH * halfH;
    float h2H = powf(n2 * m2H, pH) * sH * gH * halfH;

    Color cLow = colL, cMid = colM, cHigh = colH;
    if (isStatic && x / bounds.width <= playedRatio) {
      cLow.r >>= 1;
      cLow.g >>= 1;
      cLow.b >>= 1;
      cLow.a = 150;
      cMid.r >>= 1;
      cMid.g >>= 1;
      cMid.b >>= 1;
      cMid.a = 150;
      cHigh.r >>= 1;
      cHigh.g >>= 1;
      cHigh.b >>= 1;
      cHigh.a = 150;
    }

    // Draw using solid un-mixed colors
    rlBegin(RL_QUADS);

    // Draw Low Band Background (Blue)
    rlColor4ub(cLow.r, cLow.g, cLow.b, cLow.a);
    rlVertex2f(bounds.x + x, centerY - h1L);
    rlVertex2f(bounds.x + x, centerY + h1L);
    rlVertex2f(bounds.x + nextX, centerY + h2L);
    rlVertex2f(bounds.x + nextX, centerY - h2L);

    // Draw Mid Band Overlap (Orange)
    rlColor4ub(cMid.r, cMid.g, cMid.b, cMid.a);
    rlVertex2f(bounds.x + x, centerY - h1M);
    rlVertex2f(bounds.x + x, centerY + h1M);
    rlVertex2f(bounds.x + nextX, centerY + h2M);
    rlVertex2f(bounds.x + nextX, centerY - h2M);

    // Draw High Band Overlap (White)
    rlColor4ub(cHigh.r, cHigh.g, cHigh.b, cHigh.a);
    rlVertex2f(bounds.x + x, centerY - h1H);
    rlVertex2f(bounds.x + x, centerY + h1H);
    rlVertex2f(bounds.x + nextX, centerY + h2H);
    rlVertex2f(bounds.x + nextX, centerY - h2H);

    rlEnd();
  }
}

void Waveform_DrawGeneric(Rectangle bounds, unsigned char *data, int dataLen,
                          double positionHalfFrames, float zoomStep,
                          bool isStatic, WaveformStyle style, float gLow,
                          float gMid, float gHigh, float playedRatio,
                          float startX, float endX) {
  if (data == NULL || dataLen <= 0)
    return;

  BeginScissorMode((int)(bounds.x + UI_OffsetX), (int)(bounds.y + UI_OffsetY),
                   (int)bounds.width, (int)bounds.height);

  float centerOffset = bounds.width / 2.0f;
  switch (style) {
  case WAVEFORM_STYLE_BLUE:
    Waveform_DrawBlue(bounds, centerOffset, data, dataLen, positionHalfFrames,
                      zoomStep, isStatic, playedRatio, startX, endX);
    break;
  case WAVEFORM_STYLE_RGB:
    Waveform_DrawRGB(bounds, centerOffset, data, dataLen, positionHalfFrames,
                     zoomStep, isStatic, playedRatio, startX, endX);
    break;
  case WAVEFORM_STYLE_3BAND:
    Waveform_Draw3Band(bounds, centerOffset, data, dataLen, positionHalfFrames,
                       zoomStep, isStatic, playedRatio, gLow, gMid, gHigh,
                       startX, endX);
    break;
  default:
    Waveform_DrawBlue(bounds, centerOffset, data, dataLen, positionHalfFrames,
                      zoomStep, isStatic, playedRatio, startX, endX);
    break;
  }

  if (!isStatic)
    DrawRectangle((int)(bounds.x + startX),
                  (int)(bounds.y + bounds.height / 2.0f), (int)(endX - startX),
                  1, (Color){255, 255, 255, 40});
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
