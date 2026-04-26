#include "ui/player/waveform.h"
#include "core/logic/quantize.h"
#include "rlgl.h"
#include "ui/components/assets_bundle.h"
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
      // Frame count depends on bytes-per-frame for each waveform type:
      //   Type 1 (PWV2 Blue):          1 byte  per frame
      //   Type 2 (PWV4 RGB Color):     2 bytes per frame (16-bit big-endian
      //   word) Type 3 (PWV7 3-Band):        3 bytes per frame
      int bpf = (r->cachedTrack->WaveformType == 3)   ? 3
                : (r->cachedTrack->WaveformType == 2) ? 2
                                                      : 1;
      r->dynWfmFrames = r->cachedTrack->DynamicWaveformLen / bpf;

      // Calculate data density: how many data frames exist per UI frame (150Hz)
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
  bool inWaveform = (mouse.x >= wfLeft && mouse.x <= wfRight &&
                     mouse.y >= wfY && mouse.y <= wfY + waveH);

  // Zoom & Jog Interaction Logic
  if (inWaveform) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      int currentIndex = 0;
      float minDiff = 99999.0f;
      for (int i = 0; i < NUM_ZOOM_LEVELS; i++) {
        float diff = fabsf(r->State->ZoomScale - ZOOM_LEVELS[i]);
        if (diff < minDiff) {
          minDiff = diff;
          currentIndex = i;
        }
      }

      float deadZone = 0.05f;
      // Use a small deadzone to ignore microscopic trackpad jitters
      if (wheel > deadZone) {
        currentIndex--; // Positive wheel decreases zoom
      } else if (wheel < -deadZone) {
        currentIndex++; // Negative wheel increases zoom
      }

      // Clamp the index
      if (currentIndex < 0) currentIndex = 0;
      if (currentIndex >= NUM_ZOOM_LEVELS) currentIndex = NUM_ZOOM_LEVELS - 1;

      r->State->ZoomScale = ZOOM_LEVELS[currentIndex];
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

// ─────────────────────────────────────────────────────────────────────────────
// PWV2 BLUE color lookup table (from python-prodj-link / dysentery research)
// Each entry is the color for whiteness index 0-7.
// whiteness = bits[7:5] of the raw byte (0=darkest, 7=lightest/brightest)
// color_index used for lookup = 7 - whiteness
// ─────────────────────────────────────────────────────────────────────────────
const unsigned char PWV2_BLUE_TABLE[8][3] = {
    {200, 224, 232}, // index 0 → whiteness 7 (brightest, most white)
    {136, 192, 232}, // index 1 → whiteness 6
    {136, 192, 232}, // index 2 → whiteness 5
    {120, 184, 216}, // index 3 → whiteness 4
    {0, 184, 216},   // index 4 → whiteness 3
    {0, 168, 232},   // index 5 → whiteness 2
    {0, 136, 176},   // index 6 → whiteness 1
    {0, 104, 144},   // index 7 → whiteness 0 (darkest)
};

// Decode 1-byte PWV2 Blue sample.
// Returns: height (0..31), color RGB via out params.
int PWV2_Decode(unsigned char v, Color *outColor) {
  int height = v & 0x1F;          // bits 4-0
  int whiteness = (v >> 5) & 0x7; // bits 7-5
  int ci = 7 - whiteness;         // 0=brightest, 7=darkest
  outColor->r = PWV2_BLUE_TABLE[ci][0];
  outColor->g = PWV2_BLUE_TABLE[ci][1];
  outColor->b = PWV2_BLUE_TABLE[ci][2];
  outColor->a = 255;
  return height;
}

// ─────────────────────────────────────────────────────────────────────────────
// PWV4 RGB COLOR decoder (from python-prodj-link, dysentery issue #9)
// Data: 2 bytes per frame stored big-endian as a 16-bit word.
//   v = (data[i*2] << 8) | data[i*2+1]  (big-endian)
//   height = (v >> 2) & 0x1F   → bits 6-2
//   Blue   = (v >> 7) & 0x07   → bits 9-7
//   Green  = (v >> 10) & 0x07  → bits 12-10
//   Red    = (v >> 13) & 0x07  → bits 15-13
//   Color intensities are scaled: component = (intensity / 7.0) * 255
// ─────────────────────────────────────────────────────────────────────────────
int PWV4_Decode(unsigned char *data, int64_t frame, Color *outColor) {
  if (frame < 0) {
    *outColor = (Color){0, 0, 0, 0};
    return 0;
  }
  uint16_t v = ((uint16_t)data[frame * 2] << 8) | data[frame * 2 + 1];
  int height = (v >> 2) & 0x1F;
  int ri = (v >> 13) & 0x07;
  int gi = (v >> 10) & 0x07;
  int bi = (v >> 7) & 0x07;
  outColor->r = (unsigned char)((ri * 255) / 7);
  outColor->g = (unsigned char)((gi * 255) / 7);
  outColor->b = (unsigned char)((bi * 255) / 7);
  outColor->a = 255;
  return height;
}



// PWV7 (3-Band Scrolling Waveform) byte layout per Pioneer reverse engineering
// (dysentery):
//   Byte 0 = Mid frequency amplitude  (0-255)
//   Byte 1 = High frequency amplitude (0-255)
//   Byte 2 = Low frequency amplitude  (0-255)
// Lerp variant for sub-frame precision when zoomed in
static void Get3BandPeakLerp(unsigned char *data, int64_t maxFrames, double pos,
                             float *outL, float *outM, float *outH) {
  if (pos < 0 || pos >= maxFrames) {
    *outL = 0;
    *outM = 0;
    *outH = 0;
    return;
  }
  if (pos >= maxFrames - 1) {
    int64_t idx = (int64_t)pos;
    // Byte layout: [Mid, High, Low]
    *outM = (float)data[idx * 3];
    *outH = (float)data[idx * 3 + 1];
    *outL = (float)data[idx * 3 + 2];
    return;
  }

  int64_t i1 = (int64_t)floor(pos);
  int64_t i2 = i1 + 1;
  float fr = (float)(pos - (double)i1);

  // Byte layout: [Mid, High, Low]
  *outM = (float)data[i1 * 3] * (1.0f - fr) + (float)data[i2 * 3] * fr;
  *outH = (float)data[i1 * 3 + 1] * (1.0f - fr) + (float)data[i2 * 3 + 1] * fr;
  *outL = (float)data[i1 * 3 + 2] * (1.0f - fr) + (float)data[i2 * 3 + 2] * fr;
}

// Peak extraction over a range of frames, falling back to lerp at high zoom
void Get3BandPeak(unsigned char *data, int64_t maxFrames, double start,
                  double end, float *outL, float *outM, float *outH) {
  // If the entire window is outside valid data, return zero
  if (end <= 0 || start >= maxFrames) {
    *outL = 0;
    *outM = 0;
    *outH = 0;
    return;
  }

  // Preserve original window center for interpolation high zoom
  if (end - start <= 1.0) {
    Get3BandPeakLerp(data, maxFrames, (start + end) * 0.5, outL, outM, outH);
    return;
  }

  // Clamp actual loop boundaries to data limits
  int64_t s = (int64_t)floor(start);
  if (s < 0)
    s = 0;
  int64_t e = (int64_t)ceil(end);
  if (e > maxFrames)
    e = maxFrames;

  float maxL = 0, maxM = 0, maxH = 0;
  for (int64_t i = s; i < e; i++) {
    // Byte layout: [Mid, High, Low]
    if ((float)data[i * 3] > maxM)
      maxM = (float)data[i * 3];
    if ((float)data[i * 3 + 1] > maxH)
      maxH = (float)data[i * 3 + 1];
    if ((float)data[i * 3 + 2] > maxL)
      maxL = (float)data[i * 3 + 2];
  }
  *outL = maxL;
  *outM = maxM;
  *outH = maxH;
}

static void Waveform_Draw(Component *base) {
  WaveformRenderer *r = (WaveformRenderer *)base;

  // Canvas dimensions
  float waveH = WAVE_AREA_H / 2.0f;
  float wfY = TOP_BAR_H + (r->ID * waveH);
  float wfLeft = SIDE_PANEL_W;
  float wfRight = BEAT_FX_X;
  float wfW = wfRight - wfLeft;
  float waveCenter = waveH / 2.0f;

  // === ONE scissor region for the entire canvas — no outside draws ===
  // This eliminates all flickering caused by partial-frame compositing.
  // Everything (background, waveform, overlays) is drawn atomically inside
  // the same scissor pass before raylib flushes to the framebuffer.
  int sx = (int)(wfLeft + UI_OffsetX);
  int sy = (int)(wfY + UI_OffsetY);
  int sw = (int)wfW;
  int sh = (int)waveH;
  BeginScissorMode(sx, sy, sw, sh);

  // Background — Skip filling to allow global logo to show through
  // Subtle center guide — drawn once here
  DrawRectangle(wfLeft, (int)(wfY + waveCenter), (int)wfW, 1,
                (Color){35, 35, 35, 255});

  if (r->State->LoadedTrack == NULL) {
    EndScissorMode();
    return;
  }

  Font faceXS = UIFonts_GetFace(S(8));

  float pitchRatio = 1.0f + (r->State->TempoPercent / 100.0f);
  
  // Apply the pitch ratio to the zoom scale.
  // Faster BPM (ratio > 1.0) increases effective zoom -> squishes waveform
  // Slower BPM (ratio < 1.0) decreases effective zoom -> stretches waveform
  float effectiveZoom = (float)r->State->ZoomScale * pitchRatio;
  if (effectiveZoom < 0.1f)
    effectiveZoom = 0.1f;
  double elapsedHalfFrames = r->State->Position;
  float centerX = SCREEN_WIDTH / 2.0f;
  float playheadX = centerX;
  float zoomDelta = effectiveZoom * r->dataDensity;

  // === Style from Settings ===
  WaveformStyle userStyle = r->State->Waveform.Style;

  float gLow =
      (r->State->Waveform.GainLow > 0) ? r->State->Waveform.GainLow : 1.0f;
  float gMid =
      (r->State->Waveform.GainMid > 0) ? r->State->Waveform.GainMid : 1.0f;
  float gHigh =
      (r->State->Waveform.GainHigh > 0) ? r->State->Waveform.GainHigh : 1.0f;

  // Scaling constants for 3-Band
  const float LOW_SCALE = 0.4f;
  const float MID_SCALE = 0.3f;
  const float HIGH_SCALE = 0.06f;
  const float NORM = 1.0f / (255.0f * LOW_SCALE);
  const float PWV2_HSCALE = waveCenter / 31.0f;
  const float ATK = 0.45f;
  const float REL = 0.10f;

  // beat-link 3-band palette
  Color BL_LOW = {32, 83, 217, 255};
  Color BL_MID = {242, 170, 60, 255};
  Color BL_HIGH = {255, 255, 255, 255};
  Color colorHigh = BL_HIGH;

  int wfType =
      r->State->LoadedTrack->WaveformType; // track data format (1, 2, or 3)

  unsigned char *wfData = r->State->LoadedTrack->DynamicWaveform;
  int64_t wfFrames = r->dynWfmFrames;

  float smLo = 0, smMi = 0, smHi = 0;
  Color smCol = {0, 0, 0, 0};
  float yy = wfY + waveCenter;

  // Warm-up: seed smoothed state by simulating 100 frames of motion BEFORE the
  // x=0 edge. This is CRITICAL for consistency during scrolling. An
  // Attack/Release filter is state-dependent; by giving it a deep "history"
  // (pre-roll), its value at x=0 will be the same regardless of sub-pixel
  // scrolling offsets.
  {
    double preRollWidth = 100.0;
    for (int i = 0; i <= (int)preRollWidth; i++) {
      double px = (double)i - preRollWidth;
      double pA =
          elapsedHalfFrames * r->dataDensity + (px - centerX) * zoomDelta;
      double pB = pA + zoomDelta;

      if (wfType == 3) {
        float iL, iM, iH;
        Get3BandPeak(wfData, wfFrames, pA, pB, &iL, &iM, &iH);
        float rL = iL * LOW_SCALE * NORM * waveCenter * gLow;
        float rM = iM * MID_SCALE * NORM * waveCenter * gMid;
        float rH = iH * HIGH_SCALE * NORM * waveCenter * gHigh;
        smLo += (rL - smLo) * ((rL > smLo) ? ATK : REL);
        smMi += (rM - smMi) * ((rM > smMi) ? ATK : REL);
        smHi += (rH - smHi) * ((rH > smHi) ? ATK : REL);
      } else {
        Color col;
        int h;
        if (wfType == 2)
          h = PWV4_Decode(wfData, (int64_t)floor(pA + 0.5), &col);
        else
          h = PWV2_Decode(wfData[(int64_t)floor(pA + 0.5)], &col);

        float rL, rM, rH;
        if (userStyle == WAVEFORM_STYLE_3BAND) {
          float baseH = h * PWV2_HSCALE;
          if (col.r > col.b && col.r > col.g) {
            rL = baseH * 0.4f;
            rM = baseH * 0.9f;
            rH = baseH * 0.2f;
          } else if (col.b > col.r && col.b > col.g) {
            rL = baseH * 0.95f;
            rM = baseH * 0.6f;
            rH = baseH * 0.1f;
          } else {
            rL = baseH * 0.8f;
            rM = baseH * 0.8f;
            rH = baseH * 0.6f;
          }
          rL *= gLow;
          rM *= gMid;
          rH *= gHigh;
        } else {
          rL = h * PWV2_HSCALE * gLow;
          rM = 0;
          rH = 0;
        }

        smLo += (rL - smLo) * ((rL > smLo) ? ATK : REL);
        smMi += (rM - smMi) * ((rM > smMi) ? ATK : REL);
        smHi += (rH - smHi) * ((rH > smHi) ? ATK : REL);

        float coef = (rL > smLo) ? ATK : REL; // Use low for color smoothing
        smCol.r = (unsigned char)(smCol.r + (col.r - smCol.r) * coef);
        smCol.g = (unsigned char)(smCol.g + (col.g - smCol.g) * coef);
        smCol.b = (unsigned char)(smCol.b + (col.b - smCol.b) * coef);
        smCol.a = 255;
      }
    }
  }

#define DRAW_TRAP(pA, cA)                                                      \
  do {                                                                         \
    rlVertex2f(cx0, yy - (pA));                                                \
    rlVertex2f(cx0, yy + (pA));                                                \
    rlVertex2f(cx1, yy + (cA));                                                \
    rlVertex2f(cx0, yy - (pA));                                                \
    rlVertex2f(cx1, yy + (cA));                                                \
    rlVertex2f(cx1, yy - (cA));                                                \
  } while (0)

  // --- DATA-DRIVEN RENDERING LOOP ---
  // Instead of iterating over screen pixels, we iterate over the waveform data
  // samples themselves. This ensures that every peak is consistently shaped
  // and height-stable during scrolling.
  double framesPerPixel = zoomDelta;
  double halfVisibleFrames = (centerX / 1.0f) * framesPerPixel;
  int64_t startFrame =
      (int64_t)floor(elapsedHalfFrames * r->dataDensity - halfVisibleFrames) -
      1;
  int64_t endFrame =
      (int64_t)ceil(elapsedHalfFrames * r->dataDensity + halfVisibleFrames) + 1;

  if (startFrame < 0)
    startFrame = 0;
  if (endFrame > wfFrames)
    endFrame = wfFrames;

  // Optimized drawing loop: Skip frames if zoomed out too much to reduce GPU load
  int step = 1;
#if defined(PLATFORM_IOS)
  if (zoomDelta < 1.0f) step = 2; // Simple LOD for older iOS devices
#endif

  rlBegin(RL_TRIANGLES);
  for (int64_t i = startFrame; i < endFrame - step; i += step) {
    float x0 = (float)(((double)i - elapsedHalfFrames * r->dataDensity) /
                           framesPerPixel +
                       centerX);
    float x1 = (float)(((double)(i + 1) - elapsedHalfFrames * r->dataDensity) /
                           framesPerPixel +
                       centerX);

    // Safety skip for off-screen data
    if (x1 < wfLeft - 2 || x0 > wfRight + 2)
      continue;

    float rL = 0, rM = 0, rH = 0;
    Color colRaw = {0, 0, 0, 255};

    // Sample raw data
    if (wfType == 3) {
      rM = (float)wfData[i * 3] * MID_SCALE * NORM * waveCenter * gMid;
      rH = (float)wfData[i * 3 + 1] * HIGH_SCALE * NORM * waveCenter * gHigh;
      rL = (float)wfData[i * 3 + 2] * LOW_SCALE * NORM * waveCenter * gLow;
    } else {
      int h;
      if (wfType == 2)
        h = PWV4_Decode(wfData, i, &colRaw);
      else
        h = PWV2_Decode(wfData[i], &colRaw);

      if (userStyle == WAVEFORM_STYLE_3BAND) {
        float baseH = h * PWV2_HSCALE;
        if (colRaw.r > colRaw.b && colRaw.r > colRaw.g) {
          rL = baseH * 0.4f;
          rM = baseH * 0.9f;
          rH = baseH * 0.2f;
        } else if (colRaw.b > colRaw.r && colRaw.b > colRaw.g) {
          rL = baseH * 0.95f;
          rM = baseH * 0.6f;
          rH = baseH * 0.1f;
        } else {
          rL = baseH * 0.8f;
          rM = baseH * 0.8f;
          rH = baseH * 0.6f;
        }
        rL *= gLow;
        rM *= gMid;
        rH *= gHigh;
      } else {
        rL = h * PWV2_HSCALE * gLow;
      }
    }

    // Update smoothed state
    float pLo = smLo, pMi = smMi, pHi = smHi;
    Color pCol = smCol;

    smLo += (rL - smLo) * ((rL > smLo) ? ATK : REL);
    smMi += (rM - smMi) * ((rM > smMi) ? ATK : REL);
    smHi += (rH - smHi) * ((rH > smHi) ? ATK : REL);
    if (wfType != 3) {
      smCol.r = (unsigned char)(smCol.r + (colRaw.r - smCol.r) * ATK);
      smCol.g = (unsigned char)(smCol.g + (colRaw.g - smCol.g) * ATK);
      smCol.b = (unsigned char)(smCol.b + (colRaw.b - smCol.b) * ATK);
      smCol.a = 255;
    }

    // DRAW
    if (userStyle == WAVEFORM_STYLE_BLUE || userStyle == WAVEFORM_STYLE_RGB) {
      if (pLo > 0.1f || smLo > 0.1f) {
        float cx0 = x0, cx1 = x1;
        rlColor4ub(pCol.r, pCol.g, pCol.b, 255);
        rlVertex2f(cx0, yy - pLo);
        rlVertex2f(cx0, yy + pLo);
        rlColor4ub(smCol.r, smCol.g, smCol.b, 255);
        rlVertex2f(cx1, yy + smLo);
        rlColor4ub(pCol.r, pCol.g, pCol.b, 255);
        rlVertex2f(cx0, yy - pLo);
        rlColor4ub(smCol.r, smCol.g, smCol.b, 255);
        rlVertex2f(cx1, yy + smLo);
        rlVertex2f(cx1, yy - smLo);
      }
    } else {
      // 3-BAND
      float cx0 = x0, cx1 = x1;
      if (pLo > 0.1f || smLo > 0.1f) {
        rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 255);
        DRAW_TRAP(pLo, smLo);
      }
      if (pMi > 0.1f || smMi > 0.1f) {
        rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 255);
        DRAW_TRAP(pMi, smMi);
      }
      if (pHi > 0.1f || smHi > 0.1f) {
        rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 255);
        DRAW_TRAP(pHi, smHi);
      }
    }
  }
  rlEnd();
#undef DRAW_TRAP

  // Beat Grid — ticks use semi-transparent overlay to preserve waveform pixels
  // beneath
  if (r->State->LoadedTrack != NULL) {
    for (int i = 0; i < r->State->LoadedTrack->BeatGridCount; i++) {
      unsigned int originalMs = r->State->LoadedTrack->BeatGrid[i].Time;
      uint16_t beatNum = r->State->LoadedTrack->BeatGrid[i].BeatNumber;
      if (originalMs == 0xFFFFFFFF || originalMs == 0)
        break;

      double beatPosHF = (double)originalMs * 0.15;
      float px =
          (float)((beatPosHF - elapsedHalfFrames) / (double)effectiveZoom);
      float bx = playheadX + px;

      if (bx >= wfLeft && bx <= wfRight) {
        bool isBar = (beatNum == 1);
        bool isLastBeat = (i == r->State->LoadedTrack->BeatGridCount - 1);

        // Top cap tick (3px wide) — solid but semi-transparent
        Color capColor = isBar ? Fade(ColorRed, 0.85f) : Fade(colorHigh, 0.55f);
        if (isLastBeat) capColor = ColorRed; // Bright red for end marker

        DrawRectangleV((Vector2){bx - 1.0f, wfY}, (Vector2){3.0f, S(7)},
                       capColor);
        DrawRectangleV((Vector2){bx - 1.0f, wfY + waveH - S(7)},
                       (Vector2){3.0f, S(7)}, capColor);

        // Hairline body — nearly invisible so waveform shows through
        Color lineColor =
            isBar ? Fade(ColorRed, 0.20f) : Fade(colorHigh, 0.12f);
        
        if (isLastBeat) {
            // Stronger boundary at the end
            DrawRectangleV((Vector2){bx - 1.0f, wfY + S(7)}, (Vector2){3.0f, waveH - S(14)}, Fade(ColorRed, 0.8f));
        } else {
            DrawRectangleV((Vector2){bx, wfY + S(7)},
                           (Vector2){1.0f, waveH - S(14)}, lineColor);
        }
      }
    }
  }

  // Hot Cues — scrolling colored triangles with letter labels
  if (r->State->LoadedTrack != NULL) {
    for (int i = 0; i < r->State->LoadedTrack->HotCuesCount; i++) {
      HotCue hc = r->State->LoadedTrack->HotCues[i];
      double hcPosHF = (double)hc.Start * 0.15;
      float px = (float)((hcPosHF - elapsedHalfFrames) / (double)effectiveZoom);
      float bx = playheadX + px;

      if (bx >= wfLeft - S(10) && bx <= wfRight + S(10)) {
        Color hcClr = GetCueColor(hc, ColorOrange);
        // Triangle pointing down at top
        DrawTriangle((Vector2){bx - S(5), wfY}, (Vector2){bx + S(5), wfY},
                     (Vector2){bx, wfY + S(8)}, hcClr);
        
        // Letter indicator
        char hcLabel[2] = {(char)('A' + hc.ID - 1), 0};
        UIDrawText(hcLabel, faceXS, bx + S(6), wfY + S(1), S(8), hcClr);
        
        // Subtle vertical line through waveform
        DrawRectangleV((Vector2){bx, wfY + S(8)}, (Vector2){1.0f, waveH - S(8)}, Fade(hcClr, 0.3f));
      }
    }
  }

  // Playhead — solid bright line with subtle glow shadow behind it
  Color playheadColor = colorHigh;
  // Shadow (slightly wider, low alpha)
  DrawLineEx((Vector2){playheadX, wfY}, (Vector2){playheadX, wfY + waveH}, 3.0f,
             Fade(colorHigh, 0.18f));
  // Main hairline
  DrawLineEx((Vector2){playheadX, wfY}, (Vector2){playheadX, wfY + waveH}, 1.0f,
             playheadColor);

  if (r->ID == 0) {
    DrawLineEx((Vector2){wfLeft, wfY + waveH - 1},
               (Vector2){wfLeft + wfW, wfY + waveH - 1}, 1.0f, ColorDark1);
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
