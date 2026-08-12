#include "ui/player/waveform.h"
#include "audio/engine.h"
#include "core/logger.h"
#include "core/logic/jog_config.h"
#include "core/logic/quantize.h"
#include "core/memory_guard.h"
#include "core/midi/midi_handler.h"
#include "input/input.h"
#include "rlgl.h"
#include "ui/components/assets_bundle.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// (Local GetCurrentBeat removed, now using Quantize_GetCurrentBeat from
// quantize.h)
void Waveform_AdjustZoom(DeckState *ds, int direction) {
  if (!ds)
    return;
  int currentIndex = 0;
  float minDiff = 99999.0f;
  for (int i = 0; i < NUM_ZOOM_LEVELS; i++) {
    float diff = fabsf(ds->ZoomScale - ZOOM_LEVELS[i]);
    if (diff < minDiff) {
      minDiff = diff;
      currentIndex = i;
    }
  }
  currentIndex += direction;
  if (currentIndex < 0)
    currentIndex = 0;
  int maxZoomIndex = NUM_ZOOM_LEVELS - 1;
  if (currentIndex > maxZoomIndex)
    currentIndex = maxZoomIndex;
  ds->ZoomScale = ZOOM_LEVELS[currentIndex];
}

static int Waveform_Update(Component *base) {
  WaveformRenderer *r = (WaveformRenderer *)base;

  // Refresh dynamic frame count when track changes
  // Recalculate data density if track length or waveform data changes
  if (r->State->LoadedTrack != r->cachedTrack || r->dataDensity <= 0.001f ||
      (r->State->TrackLengthMs > 0 &&
       r->State->TrackLengthMs != r->cachedTrackLength)) {
    r->cachedTrack = r->State->LoadedTrack;
    r->cachedTrackLength = r->State->TrackLengthMs;

    if (r->cachedTrack != NULL) {
      int bpf = (r->cachedTrack->Analysis.WaveformType == 3)   ? 3
                : (r->cachedTrack->Analysis.WaveformType == 2) ? 2
                                                               : 1;
      r->dynWfmFrames = r->cachedTrack->Analysis.DynamicWaveformLen / bpf;

      if (r->State->TrackLengthMs > 0 && r->dynWfmFrames > 0) {
        // Recordbox waveforms are typically 150 Hz (0.15 frames per ms)
        double totalUIFrames = (double)r->State->TrackLengthMs * 0.15;
        r->dataDensity = (float)((double)r->dynWfmFrames / totalUIFrames);
      } else {
        r->dataDensity = 1.0f;
      }
    } else {
      r->dynWfmFrames = 0;
      r->dataDensity = 1.0f;
    }
  }

  float waveH = WAVE_AREA_H / 2.0f;
  float wfY = TOP_BAR_H + (r->ID * waveH);
  float wfLeft = SIDE_PANEL_W;
  float wfRight = BEAT_FX_X;

  Vector2 mouse = Input_GetPointerPos();
  bool inWaveform = (mouse.x >= wfLeft && mouse.x <= wfRight &&
                     mouse.y >= wfY && mouse.y <= wfY + waveH);

  // AUTO RELOAD: If focused and buffer was evicted, reload it
  bool isInteracting =
      (inWaveform || r->State->IsPlaying || r->State->IsTouching);
  if (isInteracting)
    r->lastInteractionTime = GetTime();

  if (r->cachedTrack && r->cachedTrack->Analysis.DynamicWaveform == NULL &&
      r->cachedTrack->Analysis.DynamicWaveformLen > 0) {
    // Reload if we are interacting OR we are master
    if (isInteracting || r->State->IsMaster) {
      UNX_LOG_INFO("[WAVE] Reloading evicted buffer for Deck %c",
                   r->ID == 0 ? 'A' : 'B');
      RB_ReloadWaveform(r->cachedTrack->AnalyzePath,
                        &r->cachedTrack->Analysis.DynamicWaveform,
                        &r->cachedTrack->Analysis.DynamicWaveformLen,
                        &r->cachedTrack->Analysis.WaveformType);
    }
  }

  // AUTO EVICT: If idle and memory is low, clear buffer
  if (r->cachedTrack && r->cachedTrack->Analysis.DynamicWaveform != NULL) {
    if (!isInteracting && !r->State->IsPlaying && !r->State->IsMaster) {
      if (MemoryGuard_GetLevel() >= MEM_MODE_LITE &&
          (GetTime() - r->lastInteractionTime > 5.0)) {
        UNX_LOG_INFO("[WAVE] Evicting idle buffer for Deck %c to save RAM",
                     r->ID == 0 ? 'A' : 'B');
        free(r->cachedTrack->Analysis.DynamicWaveform);
        r->cachedTrack->Analysis.DynamicWaveform = NULL;
      }
    }
  }

  // Zoom & Jog Interaction Logic
  int gesture = GetGestureDetected();
  if (inWaveform) {
    float wheel = Mouse_GetWheel();

    // Support for Pinch Gesture (Touch)
    bool isPinch =
        (gesture == GESTURE_PINCH_IN || gesture == GESTURE_PINCH_OUT);

    if (wheel != 0.0f || isPinch) {
      int currentIndex = 0;
      float minDiff = 99999.0f;
      for (int i = 0; i < NUM_ZOOM_LEVELS; i++) {
        float diff = fabsf(r->State->ZoomScale - ZOOM_LEVELS[i]);
        if (diff < minDiff) {
          minDiff = diff;
          currentIndex = i;
        }
      }

      if (isPinch) {
        static float pinchTimer = 0;
        pinchTimer += GetFrameTime();
        if (pinchTimer > 0.1f) { // Rate limit pinch zoom
          if (gesture == GESTURE_PINCH_OUT)
            currentIndex++;
          else
            currentIndex--;
          pinchTimer = 0;
        }
      } else {
        float deadZone = 0.05f;
        if (wheel > deadZone) {
          currentIndex--; // Positive wheel decreases zoom (Zoom Out)
        } else if (wheel < -deadZone) {
          currentIndex++; // Negative wheel increases zoom (Zoom In)
        }
      }

      // Clamp the index
      if (currentIndex < 0)
        currentIndex = 0;

      int maxZoomIndex = NUM_ZOOM_LEVELS - 1;
      if (currentIndex > maxZoomIndex)
        currentIndex = maxZoomIndex;

      r->State->ZoomScale = ZOOM_LEVELS[currentIndex];

      if (isPinch)
        r->State->IsTouching = false; // Avoid scratching while pinching
    }
  }

  static bool isMouseTouchingWaveform[2] = {false, false};
  int dId = (r->State->ID >= 0 && r->State->ID < 2) ? r->State->ID : 0;
  bool midiConnected = MIDI_IsControllerConnected();
  bool touchAllowed = r->State->Waveform.WaveformTouchEnabled && !midiConnected;

  // Waveform touch: disabled if WaveformTouchEnabled=false OR MIDI controller
  // is connected
  if (touchAllowed && inWaveform && Input_IsPressed()) {
    r->State->IsTouching = true;
    isMouseTouchingWaveform[dId] = true;
    r->lastMouseX = mouse.x;
  }

  if (isMouseTouchingWaveform[dId]) {
    if (touchAllowed && Input_IsDown()) {
      float dx = mouse.x - r->lastMouseX;
      r->lastMouseX = mouse.x;

      float pitchRatio = 1.0f + (r->State->TempoPercent / 100.0f);
      float effectiveZoom = (float)r->State->ZoomScale * pitchRatio;

      float moveHF = -dx * effectiveZoom;

      if (r->State->LoopAdjustIn || r->State->LoopAdjustOut) {
        // Absolute fine sample drag during loop adjustment
        r->State->JogDelta += (double)moveHF;
      } else if (r->State->VinylModeEnabled) {
        // Vinyl Mode (Scratch) movement (follows hand)
        r->State->JogDelta += (double)moveHF;
      } else {
        // CDJ Nudge logic (Pitch bend)
        r->State->JogDelta += (double)(moveHF * g_JogConfig.WaveformNudgeScale);
      }
    } else {
      isMouseTouchingWaveform[dId] = false;
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
int PWV4_Decode(unsigned char *data, int64_t frame, int64_t maxFrames,
                Color *outColor) {
  if (frame < 0 || frame >= maxFrames) {
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
  int64_t step = 1;
  int64_t count = e - s;
  if (count > 16) {
    step = count / 16;
  }
  for (int64_t i = s; i < e; i += step) {
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

  extern AudioEngine *globalAudioEngine;
  if (globalAudioEngine != NULL && r->ID >= 0 && r->ID < 2) {
    DeckAudioState *audio = &globalAudioEngine->Decks[r->ID];
    double ratioHF =
        (audio->SampleRate > 0) ? ((double)audio->SampleRate / 150.0) : 294.0;
    if (r->State->LoopAdjustIn && audio->LoopStartPos > 0) {
      elapsedHalfFrames = audio->LoopStartPos / ratioHF;
    } else if (r->State->LoopAdjustOut && audio->LoopEndPos > 0) {
      elapsedHalfFrames = audio->LoopEndPos / ratioHF;
    }
  }

  float centerX = wfLeft + (wfW / 2.0f);
  float playheadX = centerX;

  // Position is updated in real-time by main.c (including during Loop In/Out
  // jog edit)

  float zoomDelta = effectiveZoom * r->dataDensity;

  // === Style from Settings ===
  // === Style & EQ Gains from Channel Mixer ===
  WaveformStyle userStyle = r->State->Waveform.Style;

  extern AudioEngine *globalAudioEngine;
  float eqLowMult = 1.0f, eqMidMult = 1.0f, eqHighMult = 1.0f;
  if (globalAudioEngine != NULL && r->ID >= 0 && r->ID < 2) {
    eqLowMult = globalAudioEngine->Decks[r->ID].EqLow * 2.0f;
    eqMidMult = globalAudioEngine->Decks[r->ID].EqMid * 2.0f;
    eqHighMult = globalAudioEngine->Decks[r->ID].EqHigh * 2.0f;
  }

  float gLow =
      ((r->State->Waveform.GainLow > 0) ? r->State->Waveform.GainLow : 1.0f) *
      eqLowMult;
  float gMid =
      ((r->State->Waveform.GainMid > 0) ? r->State->Waveform.GainMid : 1.0f) *
      eqMidMult;
  float gHigh =
      ((r->State->Waveform.GainHigh > 0) ? r->State->Waveform.GainHigh : 1.0f) *
      eqHighMult;

  // Scaling constants for 3-Band (Matched to Teensy/CDJ-1000)
  const float LOW_SCALE = 1.0f;
  const float MID_SCALE = 1.0f;
  const float HIGH_SCALE = 1.0f;
  const float NORM =
      2.0f / 255.0f; // Boosted to 2.0x to fill canvas and prevent empty space
  const float PWV2_HSCALE = waveCenter / 31.0f;
  const float ATK = 0.9f;  // Fast attack
  const float REL = 0.12f; // Smooth release for that 'tail' look

  // hardware-accurate 3-band palette (Blue/Green/White)
  Color BL_LOW = {16, 105, 238, 255};   // Teensy col_blue
  Color BL_MID = {16, 190, 82, 255};    // Teensy col_green
  Color BL_HIGH = {246, 251, 246, 255}; // Teensy col_white
  Color colorHigh = BL_HIGH;

  int wfType = r->State->LoadedTrack->Analysis
                   .WaveformType; // track data format (1, 2, or 3)

  unsigned char *wfData = r->State->LoadedTrack->Analysis.DynamicWaveform;
  if (wfData == NULL) {
    EndScissorMode();
    return;
  }
  int64_t wfFrames = r->dynWfmFrames;

  /*
    // 1. Static Waveform (Deckstrip)
    if (r->State->LoadedTrack != NULL &&
    r->State->LoadedTrack->Analysis.StaticWaveformLen > 0) { float dsY = wfY +
    waveH - S(10); float dsH = S(8);

        // Background for the strip
        DrawRectangleRec((Rectangle){ wfLeft, dsY, wfW, dsH }, (Color){ 20, 20,
    20, 200 });

        for (int i = 0; i < r->State->LoadedTrack->Analysis.StaticWaveformLen;
    i++) { float x = wfLeft + ((float)i /
    (float)r->State->LoadedTrack->Analysis.StaticWaveformLen) * wfW; unsigned
    char val = r->State->LoadedTrack->Analysis.StaticWaveform[i];

            float h = (float)val / 255.0f * dsH;
            Color c = ColorBlue;
            if (r->State->LoadedTrack->Analysis.StaticWaveformType == 2) c =
    ColorRed; // Placeholder for Color else if
    (r->State->LoadedTrack->Analysis.StaticWaveformType == 3) c = (Color){ 100,
    200, 255, 255 };

            DrawLineV((Vector2){x, dsY + dsH}, (Vector2){x, dsY + dsH - h}, c);
        }

        // Current position marker on deckstrip
        float playPos = (float)((double)r->State->PositionMs / (double)r->State->TrackLengthMs) * wfW;
        DrawRectangle(wfLeft + playPos - 1, dsY, 2, dsH, ColorWhite);
    }
  */

  float smLo = 0, smMi = 0, smHi = 0;
  Color smCol = {0, 0, 0, 0};
  float yy = wfY + waveCenter;

  double framesPerPixel = zoomDelta;
  if (framesPerPixel < 0.05) framesPerPixel = 0.05;

  // Re-sync effectiveZoom in case framesPerPixel was clamped at max zoom
  effectiveZoom = (float)framesPerPixel / r->dataDensity;

  double centerFrame = elapsedHalfFrames * r->dataDensity;
  double centerBinExact = centerFrame / framesPerPixel;
  
  // Calculate rendering bounds in terms of absolute bins
  int binsLeft = (int)ceilf(playheadX - wfLeft) + 2;
  int binsRight = (int)ceilf(wfRight - playheadX) + 2;
  
  int startBinIndex = (int)floor(centerBinExact) - binsLeft;
  int endBinIndex = (int)floor(centerBinExact) + binsRight;
  if (startBinIndex < 0) startBinIndex = 0;
  
  int numBins = endBinIndex - startBinIndex + 1;
  #define MAX_WAVE_BINS 3000
  if (numBins > MAX_WAVE_BINS) numBins = MAX_WAVE_BINS;

  int64_t startFrame = (int64_t)(startBinIndex * framesPerPixel);
  int64_t endFrame = (int64_t)((endBinIndex + 1) * framesPerPixel);
  if (endFrame > wfFrames) endFrame = wfFrames;

  // Warm-up IIR Filter to ensure stable smoothing phase
  int64_t preRollStart = startFrame - (int64_t)(32.0 * framesPerPixel);
  if (preRollStart < 0) preRollStart = 0;
  
  for (int64_t k = preRollStart; k < startFrame; k++) {
    float rL = 0, rM = 0, rH = 0;
    Color colRaw = {0, 0, 0, 255};

    if (wfType == 3) {
      rM = (float)wfData[k * 3] * MID_SCALE * NORM * waveCenter * gMid;
      rH = (float)wfData[k * 3 + 1] * HIGH_SCALE * NORM * waveCenter * gHigh;
      rL = (float)wfData[k * 3 + 2] * LOW_SCALE * NORM * waveCenter * gLow;
    } else {
      int h;
      if (wfType == 2)
        h = PWV4_Decode(wfData, k, wfFrames, &colRaw);
      else
        h = PWV2_Decode(wfData[k], &colRaw);

      float baseH = h * PWV2_HSCALE;
      if (wfType == 2) {
        rL = baseH * ((float)colRaw.r / 255.0f) * gLow;
        rM = baseH * ((float)colRaw.g / 255.0f) * gMid;
        rH = baseH * ((float)colRaw.b / 255.0f) * gHigh;
      } else {
        if (colRaw.r > colRaw.b && colRaw.r > colRaw.g) {
          rL = baseH * 0.4f * gLow; rM = baseH * 0.9f * gMid; rH = baseH * 0.2f * gHigh;
        } else if (colRaw.b > colRaw.r && colRaw.b > colRaw.g) {
          rL = baseH * 0.95f * gLow; rM = baseH * 0.6f * gMid; rH = baseH * 0.1f * gHigh;
        } else {
          rL = baseH * 0.8f * gLow; rM = baseH * 0.8f * gMid; rH = baseH * 0.6f * gHigh;
        }
      }
    }

    smLo += (rL - smLo) * ((rL > smLo) ? ATK : REL);
    smMi += (rM - smMi) * ((rM > smMi) ? ATK : REL);
    smHi += (rH - smHi) * ((rH > smHi) ? ATK : REL);

    if (smLo > waveCenter) smLo = waveCenter;
    if (smMi > waveCenter) smMi = waveCenter;
    if (smHi > waveCenter) smHi = waveCenter;
    if (smLo < 0.5f && rL > 0) smLo = 0.5f;
    if (smMi < 0.5f && rM > 0) smMi = 0.5f;
    if (smHi < 0.5f && rH > 0) smHi = 0.5f;

    if (wfType != 3) {
      smCol.r = (unsigned char)(smCol.r + (colRaw.r - smCol.r) * ATK);
      smCol.g = (unsigned char)(smCol.g + (colRaw.g - smCol.g) * ATK);
      smCol.b = (unsigned char)(smCol.b + (colRaw.b - smCol.b) * ATK);
      smCol.a = 255;
    }
  }

  static float pixLo[MAX_WAVE_BINS];
  static float pixMi[MAX_WAVE_BINS];
  static float pixHi[MAX_WAVE_BINS];
  static Color pixCol[MAX_WAVE_BINS];

  for (int b = 0; b < numBins; b++) {
      pixLo[b] = 0.0f; pixMi[b] = 0.0f; pixHi[b] = 0.0f;
      pixCol[b] = (Color){0, 0, 0, 0};
  }

  for (int64_t i = startFrame; i < endFrame; i++) {
    float rL = 0, rM = 0, rH = 0;
    Color colRaw = {0, 0, 0, 255};

    if (wfType == 3) {
      rM = (float)wfData[i * 3] * MID_SCALE * NORM * waveCenter * gMid;
      rH = (float)wfData[i * 3 + 1] * HIGH_SCALE * NORM * waveCenter * gHigh;
      rL = (float)wfData[i * 3 + 2] * LOW_SCALE * NORM * waveCenter * gLow;
    } else {
      int h;
      if (wfType == 2)
        h = PWV4_Decode(wfData, i, wfFrames, &colRaw);
      else
        h = PWV2_Decode(wfData[i], &colRaw);

      float baseH = h * PWV2_HSCALE;
      if (wfType == 2) {
        rL = baseH * ((float)colRaw.r / 255.0f) * gLow;
        rM = baseH * ((float)colRaw.g / 255.0f) * gMid;
        rH = baseH * ((float)colRaw.b / 255.0f) * gHigh;
      } else {
        if (colRaw.r > colRaw.b && colRaw.r > colRaw.g) {
          rL = baseH * 0.4f * gLow; rM = baseH * 0.9f * gMid; rH = baseH * 0.2f * gHigh;
        } else if (colRaw.b > colRaw.r && colRaw.b > colRaw.g) {
          rL = baseH * 0.95f * gLow; rM = baseH * 0.6f * gMid; rH = baseH * 0.1f * gHigh;
        } else {
          rL = baseH * 0.8f * gLow; rM = baseH * 0.8f * gMid; rH = baseH * 0.6f * gHigh;
        }
      }
    }

    smLo += (rL - smLo) * ((rL > smLo) ? ATK : REL);
    smMi += (rM - smMi) * ((rM > smMi) ? ATK : REL);
    smHi += (rH - smHi) * ((rH > smHi) ? ATK : REL);

    if (smLo > waveCenter) smLo = waveCenter;
    if (smMi > waveCenter) smMi = waveCenter;
    if (smHi > waveCenter) smHi = waveCenter;
    if (smLo < 0.5f && rL > 0) smLo = 0.5f;
    if (smMi < 0.5f && rM > 0) smMi = 0.5f;
    if (smHi < 0.5f && rH > 0) smHi = 0.5f;

    if (wfType != 3) {
      smCol.r = (unsigned char)(smCol.r + (colRaw.r - smCol.r) * ATK);
      smCol.g = (unsigned char)(smCol.g + (colRaw.g - smCol.g) * ATK);
      smCol.b = (unsigned char)(smCol.b + (colRaw.b - smCol.b) * ATK);
      smCol.a = 255;
    }

    // Sub-Pixel Bin Spreading: ensures no gaps when zoomed in (framesPerPixel < 1.0)
    int trackBinStart = (int)(i / framesPerPixel);
    int trackBinEnd = (int)((i + 0.999) / framesPerPixel);
    
    for (int k = trackBinStart; k <= trackBinEnd; k++) {
        int b = k - startBinIndex;
        if (b >= 0 && b < numBins) {
            if (smLo > pixLo[b]) pixLo[b] = smLo;
            if (smMi > pixMi[b]) pixMi[b] = smMi;
            if (smHi > pixHi[b]) pixHi[b] = smHi;
            pixCol[b] = smCol;
        }
    }
  }

  // Draw smooth continuous polygon (RL_QUADS) using fractional scrolling
  // Single-pass gradient-alpha rendering: transparent at edges fades to opaque at core.
  // This halves vertex count while achieving soft anti-aliased slopes.
  rlBegin(RL_QUADS);
  for (int b = 0; b < numBins - 1; b++) {
      int binIdx = startBinIndex + b;
      
      // Exact fractional scrolling offset — always 1.0px apart, no wobble
      float px0 = playheadX + (float)((double)binIdx - centerBinExact);
      float px1 = px0 + 1.0f;
      
      // Frustum culling — skip bins outside canvas
      if (px1 < wfLeft - 2.0f || px0 > wfRight + 2.0f) continue;
      
      if (userStyle == WAVEFORM_STYLE_BLUE || userStyle == WAVEFORM_STYLE_RGB) {
          // Use raw float heights (no roundf) for smooth sub-pixel slopes
          float h0 = fmaxf(pixLo[b],   fmaxf(pixMi[b],   pixHi[b]));
          float h1 = fmaxf(pixLo[b+1], fmaxf(pixMi[b+1], pixHi[b+1]));
          
          if (h0 > 0.1f || h1 > 0.1f) {
              Color c = pixCol[b];
              if (userStyle == WAVEFORM_STYLE_RGB) {
                  c.r = (unsigned char)fminf(255.0f, (float)c.r * eqLowMult);
                  c.g = (unsigned char)fminf(255.0f, (float)c.g * eqMidMult);
                  c.b = (unsigned char)fminf(255.0f, (float)c.b * eqHighMult);
              }
              // Outer transparent edge (AA fringe)
              rlColor4ub(c.r, c.g, c.b, 0);
              rlVertex2f(px0, yy - h0 - 1.0f);
              rlColor4ub(c.r, c.g, c.b, 0);
              rlVertex2f(px0, yy + h0 + 1.0f);
              rlColor4ub(c.r, c.g, c.b, 0);
              rlVertex2f(px1, yy + h1 + 1.0f);
              rlColor4ub(c.r, c.g, c.b, 0);
              rlVertex2f(px1, yy - h1 - 1.0f);
              
              // Inner solid core
              rlColor4ub(c.r, c.g, c.b, 255);
              rlVertex2f(px0, yy - h0 + 0.5f);
              rlColor4ub(c.r, c.g, c.b, 255);
              rlVertex2f(px0, yy + h0 - 0.5f);
              rlColor4ub(c.r, c.g, c.b, 255);
              rlVertex2f(px1, yy + h1 - 0.5f);
              rlColor4ub(c.r, c.g, c.b, 255);
              rlVertex2f(px1, yy - h1 + 0.5f);
          }
      } else {
          float pL0 = pixLo[b];   float pL1 = pixLo[b+1];
          float pM0 = pixMi[b];   float pM1 = pixMi[b+1];
          float pH0 = pixHi[b];   float pH1 = pixHi[b+1];
          
          if (pL0 > 0.1f || pL1 > 0.1f) {
              rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 0);
              rlVertex2f(px0, yy - pL0 - 1.0f);
              rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 0);
              rlVertex2f(px0, yy + pL0 + 1.0f);
              rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 0);
              rlVertex2f(px1, yy + pL1 + 1.0f);
              rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 0);
              rlVertex2f(px1, yy - pL1 - 1.0f);
              
              rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 255);
              rlVertex2f(px0, yy - pL0 + 0.5f);
              rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 255);
              rlVertex2f(px0, yy + pL0 - 0.5f);
              rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 255);
              rlVertex2f(px1, yy + pL1 - 0.5f);
              rlColor4ub(BL_LOW.r, BL_LOW.g, BL_LOW.b, 255);
              rlVertex2f(px1, yy - pL1 + 0.5f);
          }
          if (pM0 > 0.1f || pM1 > 0.1f) {
              rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 0);
              rlVertex2f(px0, yy - pM0 - 1.0f);
              rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 0);
              rlVertex2f(px0, yy + pM0 + 1.0f);
              rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 0);
              rlVertex2f(px1, yy + pM1 + 1.0f);
              rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 0);
              rlVertex2f(px1, yy - pM1 - 1.0f);
              
              rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 255);
              rlVertex2f(px0, yy - pM0 + 0.5f);
              rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 255);
              rlVertex2f(px0, yy + pM0 - 0.5f);
              rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 255);
              rlVertex2f(px1, yy + pM1 - 0.5f);
              rlColor4ub(BL_MID.r, BL_MID.g, BL_MID.b, 255);
              rlVertex2f(px1, yy - pM1 + 0.5f);
          }
          if (pH0 > 0.1f || pH1 > 0.1f) {
              rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 0);
              rlVertex2f(px0, yy - pH0 - 1.0f);
              rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 0);
              rlVertex2f(px0, yy + pH0 + 1.0f);
              rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 0);
              rlVertex2f(px1, yy + pH1 + 1.0f);
              rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 0);
              rlVertex2f(px1, yy - pH1 - 1.0f);
              
              rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 255);
              rlVertex2f(px0, yy - pH0 + 0.5f);
              rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 255);
              rlVertex2f(px0, yy + pH0 - 0.5f);
              rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 255);
              rlVertex2f(px1, yy + pH1 - 0.5f);
              rlColor4ub(BL_HIGH.r, BL_HIGH.g, BL_HIGH.b, 255);
              rlVertex2f(px1, yy - pH1 + 0.5f);
          }
      }
  }
  rlEnd();

  // Beat Grid — ticks use semi-transparent overlay to preserve waveform pixels
  // beneath
  if (r->State->LoadedTrack != NULL) {
    double minVisibleMs =
        ((elapsedHalfFrames + (wfLeft - playheadX) * effectiveZoom) / 0.15) -
        500.0;
    double maxVisibleMs =
        ((elapsedHalfFrames + (wfRight - playheadX) * effectiveZoom) / 0.15) +
        500.0;

    for (int i = 0; i < r->State->LoadedTrack->Analysis.BeatGridCount; i++) {
      unsigned int originalMs =
          r->State->LoadedTrack->Analysis.BeatGrid[i].Time;
      if (originalMs == 0xFFFFFFFF)
        break;
      if ((double)originalMs < minVisibleMs)
        continue;
      if ((double)originalMs > maxVisibleMs)
        break;

      uint16_t beatNum = r->State->LoadedTrack->Analysis.BeatGrid[i].BeatNumber;
      double beatPosHF = (double)originalMs * 0.15;
      float px =
          (float)((beatPosHF - elapsedHalfFrames) / (double)effectiveZoom);
      float bx = playheadX + px;

      if (bx >= wfLeft && bx <= wfRight) {
        bool isBar = (beatNum == 1);
        bool isLastBeat =
            (i == r->State->LoadedTrack->Analysis.BeatGridCount - 1);

        Color capColor = isBar ? Fade(ColorRed, 0.85f) : Fade(colorHigh, 0.55f);
        if (isLastBeat)
          capColor = ColorRed;

        DrawRectangleV((Vector2){bx - 1.0f, wfY}, (Vector2){3.0f, S(7)},
                       capColor);
        DrawRectangleV((Vector2){bx - 1.0f, wfY + waveH - S(7)},
                       (Vector2){3.0f, S(7)}, capColor);

        Color lineColor =
            isBar ? Fade(ColorRed, 0.20f) : Fade(colorHigh, 0.12f);
        if (isLastBeat) {
          DrawRectangleV((Vector2){bx - 1.0f, wfY + S(7)},
                         (Vector2){3.0f, waveH - S(14)}, Fade(ColorRed, 0.8f));
        } else {
          DrawRectangleV((Vector2){bx, wfY + S(7)},
                         (Vector2){1.0f, waveH - S(14)}, lineColor);
        }
      }
    }
  }

  // Main Cue Marker
  if (r->State->LoadedTrack != NULL) {
    double cuePosHF = (double)r->State->MainCueMs * 0.15;
    float px = (float)((cuePosHF - elapsedHalfFrames) / (double)effectiveZoom);
    float bx = playheadX + px;

    if (bx >= wfLeft && bx <= wfRight) {
      // Orange triangle at the bottom pointing up
      DrawTriangle((Vector2){bx - S(6), wfY + waveH},
                   (Vector2){bx + S(6), wfY + waveH},
                   (Vector2){bx, wfY + waveH - S(8)}, ColorOrange);
      // Vertical white line
      DrawLineEx((Vector2){bx, wfY}, (Vector2){bx, wfY + waveH}, 1.5f,
                 ColorWhite);
    }
  }

  // Memory Cues
  if (r->State->LoadedTrack != NULL) {
    for (int i = 0; i < r->State->LoadedTrack->CuesCount; i++) {
      HotCue mc = r->State->LoadedTrack->Cues[i];
      double mcPosHF = (double)mc.Start * 0.15;
      float px = (float)((mcPosHF - elapsedHalfFrames) / (double)effectiveZoom);
      float bx = playheadX + px;

      if (bx >= wfLeft - S(2) && bx <= wfRight + S(2)) {
        // Vertical orange line
        DrawLineEx((Vector2){bx, wfY}, (Vector2){bx, wfY + waveH}, 1.0f,
                   ColorOrange);
        // Small marker at bottom
        DrawRectangleV((Vector2){bx - 1.0f, wfY + waveH - S(5)},
                       (Vector2){3.0f, S(5)}, ColorOrange);
      }
    }
  }

  // Hot Cues — scrolling colored triangles with letter labels
  if (r->State->LoadedTrack != NULL) {
    static const Color hcPalette[8] = {{0, 255, 0, 255},   {255, 0, 0, 255},
                                       {255, 128, 0, 255}, {255, 255, 0, 255},
                                       {0, 0, 255, 255},   {255, 0, 255, 255},
                                       {0, 255, 255, 255}, {128, 0, 255, 255}};

    for (int i = 0; i < r->State->LoadedTrack->HotCuesCount; i++) {
      HotCue hc = r->State->LoadedTrack->HotCues[i];
      double hcPosHF = (double)hc.Start * 0.15;
      float px = (float)((hcPosHF - elapsedHalfFrames) / (double)effectiveZoom);
      float bx = playheadX + px;

      if (bx >= wfLeft - S(10) && bx <= wfRight + S(10)) {
        int idx = hc.ID - 1;
        if (idx < 0)
          idx = 0;
        if (idx > 7)
          idx = 7;
        Color hcClr = GetCueColor(hc, hcPalette[idx]);
        // Triangle pointing down at top
        DrawTriangle((Vector2){bx - S(6), wfY}, (Vector2){bx + S(6), wfY},
                     (Vector2){bx, wfY + S(10)}, hcClr);

        // Letter indicator with background
        char hcLabel[2] = {(char)('A' + hc.ID - 1), 0};
        Font hcFont = UIFonts_GetBoldFace(S(12));
        float txtW = MeasureTextEx(hcFont, hcLabel, S(12), 1).x;
        DrawRectangleRec((Rectangle){bx + S(6), wfY, txtW + S(4), S(14)},
                         (Color){0, 0, 0, 220});
        UIDrawText(hcLabel, hcFont, bx + S(8), wfY + S(1), S(12), hcClr);

        // Bold vertical line through waveform
        DrawRectangleV((Vector2){bx - 1.0f, wfY + S(10)},
                       (Vector2){3.0f, waveH - S(10)}, Fade(hcClr, 0.85f));
      }
    }
  }

  // --- LOOP HIGHLIGHT ---
  // --- LOOP HIGHLIGHT & EDIT BOUNDARIES ---
  extern AudioEngine *globalAudioEngine;
  if (globalAudioEngine && r->ID >= 0 && r->ID < 2) {
    DeckAudioState *audio = &globalAudioEngine->Decks[r->ID];
    if (audio->IsLooping || r->State->LoopAdjustIn || r->State->LoopAdjustOut) {
      double ratioHF =
          (audio->SampleRate > 0) ? ((double)audio->SampleRate / 150.0) : 294.0;
      double loopStartHF = audio->LoopStartPos / ratioHF;
      double loopEndHF = audio->LoopEndPos / ratioHF;

      float xStart =
          (float)((loopStartHF - elapsedHalfFrames) / (double)effectiveZoom);
      float xEnd =
          (float)((loopEndHF - elapsedHalfFrames) / (double)effectiveZoom);

      float bxStart = playheadX + xStart;
      float bxEnd = playheadX + xEnd;

      // Draw loop shaded region
      if (bxEnd >= wfLeft && bxStart <= wfRight) {
        float drawLeft = fmaxf(bxStart, wfLeft);
        float drawRight = fminf(bxEnd, wfRight);
        Color loopCol = (Color){255, 165, 0, 255}; // Amber DJ loop color
        DrawRectangleRec(
            (Rectangle){drawLeft, wfY, drawRight - drawLeft, waveH},
            Fade(loopCol, 0.25f));

        // Draw In boundary
        if (bxStart >= wfLeft && bxStart <= wfRight) {
          bool adjIn = r->State->LoopAdjustIn;
          Color inCol = adjIn ? (Color){255, 230, 0, 255} : loopCol;
          float lineThick = adjIn ? 4.0f : 2.5f;
          DrawLineEx((Vector2){bxStart, wfY}, (Vector2){bxStart, wfY + waveH},
                     lineThick, inCol);
          DrawRectangleRec((Rectangle){bxStart, wfY + 2, 24, 16},
                           Fade(inCol, adjIn ? 0.95f : 0.7f));
          DrawText("IN", (int)bxStart + 4, (int)wfY + 4, 10, BLACK);
        }
        // Draw Out boundary
        if (bxEnd >= wfLeft && bxEnd <= wfRight) {
          bool adjOut = r->State->LoopAdjustOut;
          Color outCol = adjOut ? (Color){255, 230, 0, 255} : loopCol;
          float lineThick = adjOut ? 4.0f : 2.5f;
          DrawLineEx((Vector2){bxEnd, wfY}, (Vector2){bxEnd, wfY + waveH},
                     lineThick, outCol);
          DrawRectangleRec((Rectangle){bxEnd - 28, wfY + 2, 26, 16},
                           Fade(outCol, adjOut ? 0.95f : 0.7f));
          DrawText("OUT", (int)bxEnd - 24, (int)wfY + 4, 10, BLACK);
        }
      }

      // Active Loop Adjust Mode Badge & Running Live Playhead
      if (r->State->LoopAdjustIn || r->State->LoopAdjustOut) {
        if (r->State->LoopAdjustIn) {
          DrawRectangleRec((Rectangle){wfLeft + 10, wfY + 6, 106, 20},
                           Fade((Color){255, 220, 0, 255}, 0.9f));
          DrawText("LOOP IN ADJ", (int)wfLeft + 16, (int)wfY + 10, 11, BLACK);
        } else {
          DrawRectangleRec((Rectangle){wfRight - 116, wfY + 6, 106, 20},
                           Fade((Color){255, 220, 0, 255}, 0.9f));
          DrawText("LOOP OUT ADJ", (int)wfRight - 110, (int)wfY + 10, 11,
                   BLACK);
        }

        // Draw animated live playhead sweeping through loop area
        double livePosHF = audio->Position / ratioHF;
        float xLive =
            (float)((livePosHF - elapsedHalfFrames) / (double)effectiveZoom);
        float bxLive = playheadX + xLive;

        if (bxLive >= wfLeft && bxLive <= wfRight) {
          DrawLineEx((Vector2){bxLive, wfY}, (Vector2){bxLive, wfY + waveH},
                     2.5f, ColorRed);
          DrawTriangle((Vector2){bxLive - S(4), wfY},
                       (Vector2){bxLive + S(4), wfY},
                       (Vector2){bxLive, wfY + S(6)}, ColorRed);
          DrawTriangle((Vector2){bxLive - S(4), wfY + waveH},
                       (Vector2){bxLive + S(4), wfY + waveH},
                       (Vector2){bxLive, wfY + waveH - S(6)}, ColorRed);
        }
      }
    }
  }

  // --- GHOST PLAYHEAD (SLIP MODE) ---
  if (globalAudioEngine) {
    DeckAudioState *audio = &globalAudioEngine->Decks[r->ID];
    if (audio->SlipActive) {
      double ratioHF = (double)audio->SampleRate / 150.0;
      double slipPosHF = audio->SlipPosition / ratioHF;
      float xSlip =
          (float)((slipPosHF - elapsedHalfFrames) / (double)effectiveZoom);
      float bxSlip = playheadX + xSlip;

      if (bxSlip >= wfLeft && bxSlip <= wfRight) {
        // Draw a dimmed/ghost playhead line
        DrawLineEx((Vector2){bxSlip, wfY}, (Vector2){bxSlip, wfY + waveH}, 1.0f,
                   Fade(ColorWhite, 0.4f));
        // Tiny arrow at top
        DrawTriangle((Vector2){bxSlip - S(3), wfY},
                     (Vector2){bxSlip + S(3), wfY},
                     (Vector2){bxSlip, wfY + S(5)}, Fade(ColorWhite, 0.6f));
      }
    }
  }

  // Playhead — solid bright line with subtle glow shadow behind it (only when
  // not editing loop)
  if (!r->State->LoopAdjustIn && !r->State->LoopAdjustOut) {
    Color playheadColor = colorHigh;
    // Shadow (slightly wider, low alpha)
    DrawLineEx((Vector2){playheadX, wfY}, (Vector2){playheadX, wfY + waveH},
               3.0f, Fade(colorHigh, 0.18f));
    // Main hairline
    DrawLineEx((Vector2){playheadX, wfY}, (Vector2){playheadX, wfY + waveH},
               1.0f, playheadColor);
  }

  if (r->ID == 0) {
    DrawLineEx((Vector2){wfLeft, wfY + waveH - 1},
               (Vector2){wfLeft + wfW, wfY + waveH - 1}, 1.0f, ColorDark1);
  }

  EndScissorMode();

  // --- Phase Meter UI ---
  if (r->State->LoadedTrack) {
    float pmW = S(140); // Total width of the 4 blocks
    float pmH = S(10);  // Height of main blocks
    float pmX = wfLeft + (wfW / 2.0f) - (pmW / 2.0f);
    float pmY = wfY + S(4);

    // Get current beat (1-4) and map it to a 0-3 index for the blocks
    int myBeat =
        Quantize_GetCurrentBeat(r->State->LoadedTrack, r->State->PositionMs);
    int myDisplayBeat = ((myBeat - 1) % 4);

    float blockSpacing = S(4);
    float blockW = (pmW - (3 * blockSpacing)) / 4.0f;

    // Draw Main Deck (This Deck) - Large Blocks
    for (int i = 0; i < 4; i++) {
      float bx = pmX + i * (blockW + blockSpacing);

      // Draw empty box outline
      DrawRectangleLines(bx, pmY, blockW, pmH, ColorShadow);

      // Fill the box if it's the current beat
      if (i == myDisplayBeat) {
        Color c = r->State->IsMaster ? ColorOrange : ColorWhite;
        DrawRectangle(bx, pmY, blockW, pmH, c);
      }
    }

    // Draw Other Deck - Small blocks underneath (for visual beat matching)
    if (r->OtherDeck && r->OtherDeck->LoadedTrack) {
      int otherBeat = Quantize_GetCurrentBeat(r->OtherDeck->LoadedTrack,
                                              r->OtherDeck->PositionMs);
      int otherDisplayBeat = ((otherBeat - 1) % 4);

      float otherY = pmY + pmH + S(2);
      float otherH = S(4); // Smaller height for the secondary track

      for (int i = 0; i < 4; i++) {
        float bx = pmX + i * (blockW + blockSpacing);

        DrawRectangleLines(bx, otherY, blockW, otherH, ColorShadow);

        if (i == otherDisplayBeat) {
          // Dim the other deck's color slightly unless it's the master
          Color c =
              r->OtherDeck->IsMaster ? ColorOrange : Fade(ColorWhite, 0.6f);
          DrawRectangle(bx, otherY, blockW, otherH, c);
        }
      }
    }
  }

  // --- LOADING OVERLAY ---
  if (r->State->IsLoading) {
    float pulse = (sinf(GetTime() * 10.0f) * 0.5f + 0.5f); // 0.0 to 1.0
    DrawRectangle(wfLeft, wfY, wfRight - wfLeft, waveH,
                  Fade(ColorOrange, 0.1f + pulse * 0.2f));

    Font faceBPM = UIFonts_GetFace(S(20));
    DrawCentredText("LOADING TRACK...", faceBPM, wfLeft, wfW,
                    wfY + waveCenter - S(10), S(20),
                    Fade(ColorWhite, 0.6f + pulse * 0.4f));
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
  r->lastInteractionTime = GetTime();
}
