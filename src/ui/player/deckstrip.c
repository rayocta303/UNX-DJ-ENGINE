#include "ui/player/deckstrip.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/waveform.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <rlgl.h>

static void drawLeftBadgeColumn(DeckStrip *d, float x, float y, float h) {
  float lColW = S(40);
  float lColX = x;

  Font faceXXS = UIFonts_GetFace(S(7));
  Font faceSm = UIFonts_GetFace(S(9));
  Font faceMd = UIFonts_GetFace(S(11));
  Font faceLg = UIFonts_GetFace(S(18));

  // 1. DECK Header
  float headH = 21;
  DrawRectangle(lColX, y, lColW, h, ColorDark3);
  DrawRectangle(lColX, y, lColW, S(headH), ColorBGUtil);
  DrawRectangle(lColX, y + S(headH) - S(1), lColW, S(1), ColorShadow);

  DrawCentredText("DECK", faceXXS, lColX, lColW, y + S(2), S(7), ColorPaper);

  float badgeY = y + S(9);
  DrawCentredText("((   ))", faceSm, lColX, lColW, badgeY, S(9), ColorRed);

  char idStr[16];
  sprintf(idStr, "%d", d->ID + 1);
  DrawCentredText(idStr, faceSm, lColX, lColW, badgeY, S(9), ColorWhite);

  // 2. TRACK Section
  float trackY = 24;
  DrawCentredText("TRACK", faceXXS, lColX, lColW, y + S(trackY), S(7),
                  ColorShadow);

  char trackStr[16] = "---";
  if (d->State->LoadedTrack != NULL) {
    sprintf(trackStr, "%02d", d->State->TrackNumber);
  }
  DrawCentredText(trackStr, faceLg, lColX, lColW, y + S(trackY + 5), S(18),
                  ColorWhite);

  // 3. SINGLE
  float singleY = 52;
  DrawCentredText("SINGLE", faceXXS, lColX, lColW, y + S(singleY), S(7),
                  ColorShadow);

  // 4. QUANTIZE
  float quantizeY = 68;
  Color quantizeColor = ColorShadow;
  const char *quantizeStr = "OFF";

  if (d->State->QuantizeEnabled) {
    quantizeColor = ColorRed;
    quantizeStr = "1";
  }

  DrawCentredText("QUANTIZE", faceXXS, lColX, lColW, y + S(quantizeY), S(7),
                  quantizeColor);
  DrawCentredText(quantizeStr, faceMd, lColX, lColW, y + S(quantizeY + 8.5f),
                  S(11), quantizeColor);

  DrawLine(lColX + lColW, y, lColX + lColW, y + h, ColorDark1);
}

static int DeckStrip_Update(Component *base) {
  DeckStrip *d = (DeckStrip *)base;
  float stripW = SCREEN_WIDTH / 2.0f;
  float x = d->ID * stripW;
  float y = TOP_BAR_H + WAVE_AREA_H + FX_BAR_H;
  float titleBgH = S(16);
  float midY = y + titleBgH + S(4);
  float lColW = S(40);
  float bpmBoxW = S(50);
  float bpmX = x + stripW - bpmBoxW - S(4);
  float tempoX = bpmX - S(64);

  float mtX = x + lColW + S(4);
  float mtY = midY + S(26);
  float mtW = S(40);
  float mtH = S(9);

  float bpmBoxH = S(28);
  float syncY = midY + bpmBoxH + S(2);
  float syncH = S(12);

  Vector2 mouse = GetMousePosition();
  bool isHoverTempoArea = (mouse.x >= tempoX && mouse.x <= bpmX + bpmBoxW &&
                           mouse.y >= midY && mouse.y <= midY + S(28));
  bool isHoverMT = (mouse.x >= mtX && mouse.x <= mtX + mtW && mouse.y >= mtY &&
                    mouse.y <= mtY + mtH);
  bool isHoverQuantize = (mouse.x >= x && mouse.x <= x + lColW &&
                          mouse.y >= y + S(68) && mouse.y <= y + S(88));
  bool isHoverSync = (mouse.x >= bpmX && mouse.x <= bpmX + bpmBoxW &&
                      mouse.y >= syncY && mouse.y <= syncY + syncH);

  float wx = x + lColW + S(4);
  float ww = stripW - lColW - S(10);
  float wy = TOP_BAR_H + WAVE_AREA_H + FX_BAR_H + DECK_STR_H - S(28);
  float wh = S(20);
  bool isHoverWaveform = (mouse.x >= wx && mouse.x <= wx + ww &&
                          mouse.y >= wy && mouse.y <= wy + wh);

  if (isHoverMT && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    d->State->MasterTempo = !d->State->MasterTempo;
  }

  if (isHoverQuantize && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    d->State->QuantizeEnabled = !d->State->QuantizeEnabled;
  }

  if (isHoverSync && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    d->State->SyncMode = (d->State->SyncMode + 1) % 3;
  }

  if (isHoverWaveform && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
      d->State->LoadedTrack != NULL) {
    float msRatio = (mouse.x - wx) / ww;
    if (msRatio < 0.0f)
      msRatio = 0.0f;
    if (msRatio > 1.0f)
      msRatio = 1.0f;

    long long newMs = (long long)(msRatio * d->State->TrackLengthMs);
    d->State->PositionMs = newMs;
    // Update playhead position in half-frames (150Hz = 0.15 * ms)
    d->State->Position = (double)newMs * 0.15;
  }

  if (isHoverTempoArea) {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      d->State->TempoRange = (d->State->TempoRange + 1) % 4;
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      float step = 0.0f;
      if (d->State->TempoRange == 0)
        step = 0.02f * wheel;
      else if (d->State->TempoRange == 1)
        step = 0.05f * wheel;
      else if (d->State->TempoRange == 2)
        step = 0.05f * wheel;
      else
        step = 0.5f * wheel;

      d->State->TempoPercent += step;

      float maxRange = 10.0f;
      if (d->State->TempoRange == 0)
        maxRange = 6.0f;
      else if (d->State->TempoRange == 1)
        maxRange = 10.0f;
      else if (d->State->TempoRange == 2)
        maxRange = 16.0f;
      else if (d->State->TempoRange == 3)
        maxRange = 100.0f;

      if (d->State->TempoPercent > maxRange)
        d->State->TempoPercent = maxRange;
      if (d->State->TempoPercent < -maxRange)
        d->State->TempoPercent = -maxRange;
    }
  }
  return 0;
}

// Static helper for decoding waveform bands
static void DecodeStaticWaveformBands(unsigned char byteVal, float hScale, float *b, float *m, float *h) {
  int amp = byteVal & 0x1F;
  int colorIdx = byteVal >> 5;
  float base = ((float)amp / 31.0f) * hScale;
  if (colorIdx >= 6) {
    *b = base * 0.95f; *m = base * 0.85f; *h = base * 0.60f;
  } else if (colorIdx >= 3) {
    *b = base * 0.90f; *m = base * 0.75f; *h = base * 0.20f;
  } else {
    *b = base * 0.85f; *m = base * 0.40f; *h = base * 0.05f;
  }
}

static unsigned char GetStaticWaveformPeak(unsigned char *data, int start, int end) {
  if (start >= end) return data[start];
  int maxAmp = -1;
  unsigned char best = 0;
  for (int i = start; i < end; i++) {
    int amp = data[i] & 0x1F;
    if (amp > maxAmp) {
      maxAmp = amp;
      best = data[i];
    }
  }
  return best;
}

static void DeckStrip_Draw(Component *base) {
  DeckStrip *d = (DeckStrip *)base;
  float stripW = SCREEN_WIDTH / 2.0f;
  float x = d->ID * stripW;
  float y = TOP_BAR_H + WAVE_AREA_H + FX_BAR_H;
  float h = DECK_STR_H;

  DrawRectangle(x, y, stripW, h, ColorBlack);
  DrawLine(x, y, x + stripW, y, ColorDark1);
  if (d->ID == 1) {
    DrawLine(x, y, x, y + h, ColorDark1);
  }

  Font faceXXS = UIFonts_GetFace(S(7));
  Font faceSm = UIFonts_GetFace(S(9));
  Font faceMd = UIFonts_GetFace(S(11));
  Font faceLg = UIFonts_GetFace(S(18));
  Font faceSub = UIFonts_GetFace(S(13));
  Font faceBPM = UIFonts_GetFace(S(20));

  float lColW = S(40);
  float lColX = x;
  drawLeftBadgeColumn(d, x, y, h);

  float mx = lColX + lColW + S(4);
  float titleBgH = S(16);
  DrawRectangle(lColX + lColW, y, stripW - lColW, titleBgH,
                (Color){0x10, 0x10, 0x10, 0xFF});

  char title[150] = "No Track";
  if (d->State->TrackTitle[0] != '\0') {
    strncpy(title, d->State->TrackTitle, sizeof(title) - 1);

    Font iconFace = UIFonts_GetIcon(S(9));
    UIDrawText("\xef\x80\x81", iconFace, mx, y + S(2), S(9),
               ColorWhite); // f001 music note
    UIDrawText(title, faceSm, mx + S(12), y + S(2), S(9), ColorWhite);
  } else {
    UIDrawText(title, faceSm, mx, y + S(2), S(9), ColorWhite);
  }

  float midY = y + titleBgH + S(4);
  float badgeX = mx;
  float badgeW = S(40);

  DrawRectangleLines(badgeX, midY + S(2), badgeW, S(9), ColorOrange);
  DrawCentredText("A.HOT CUE", faceXXS, badgeX, badgeW, midY + S(2.5f), S(7),
                  ColorOrange);

  DrawRectangleLines(badgeX, midY + S(14), badgeW, S(9), ColorShadow);
  DrawCentredText("AUTO CUE", faceXXS, badgeX, badgeW, midY + S(14.5f), S(7),
                  ColorPaper);

  Color mtClr = d->State->MasterTempo ? ColorRed : ColorShadow;
  DrawRectangleLines(badgeX, midY + S(26), badgeW, S(9), mtClr);
  DrawCentredText("MT", faceXXS, badgeX, badgeW, midY + S(26.5f), S(7), mtClr);

  float timeX = mx + badgeW + S(8);
  DrawCentredText("REMAIN / TIME", faceXXS, timeX, S(80), midY, S(7),
                  ColorShadow);

  long long playedMs = d->State->PositionMs;
  int minutes = (playedMs / 1000) / 60;
  int seconds = (playedMs / 1000) % 60;
  int subSec = playedMs % 1000;

  char timeStr[16];
  sprintf(timeStr, "%02d:%02d", minutes, seconds);
  float timeValY = midY + S(9);
  UIDrawText(timeStr, faceLg, timeX, timeValY, S(18), ColorWhite);

  float wM = MeasureTextEx(faceLg, timeStr, S(18), 1).x;
  char subStr[8];
  sprintf(subStr, ".%03d", subSec);
  UIDrawText(subStr, faceSub, timeX + wM + S(2), timeValY + S(4), S(13),
             ColorPaper);

  float bpmBoxW = S(50);
  float bpmX = x + stripW - bpmBoxW - S(4);
  float tempoX = bpmX - S(64);

  UIDrawText("TEMPO", faceXXS, tempoX, midY, S(7), ColorShadow);

  const char *rangeStr = "10%";
  if (d->State->TempoRange == 0)
    rangeStr = " 6%";
  else if (d->State->TempoRange == 2)
    rangeStr = "16%";
  else if (d->State->TempoRange == 3)
    rangeStr = "WIDE";

  float tBadgeW = 24.0f;
  float badgeY = midY - S(0.5f);
  DrawRectangle(tempoX + S(32), badgeY, S(tBadgeW), S(9), ColorOrange);
  DrawCentredText(rangeStr, faceXXS, tempoX + S(32), S(tBadgeW),
                  badgeY + S(0.5f), S(7), ColorBlack);

  char tempoStr[32];
  sprintf(tempoStr, "%+.2f%%", d->State->TempoPercent);
  if (d->State->TempoPercent == 0.0f)
    sprintf(tempoStr, " 0.00%%");

  float wT = MeasureTextEx(faceMd, tempoStr, S(11), 1).x;
  float valX = (tempoX + S(32) + S(tBadgeW)) - wT;
  UIDrawText(tempoStr, faceMd, valX, midY + S(11), S(11), ColorPaper);

  float bpmBoxH = S(28);
  float bpmBoxY = midY;
  DrawRectangleLines(bpmX, bpmBoxY, bpmBoxW, bpmBoxH, ColorOrange);
  UIDrawText("BPM", faceXXS, bpmX + S(2), bpmBoxY + S(0.5f), S(7), ColorShadow);

  if (d->State->IsMaster) {
    float masterW = S(30);
    float masterX = bpmX + bpmBoxW - masterW - S(1);
    DrawRectangle(masterX, bpmBoxY + S(1), masterW, S(8), ColorOrange);
    DrawCentredText("MASTER", faceXXS, masterX, masterW, bpmBoxY + S(1), S(7),
                    ColorBlack);
  }

  char bpmMain[16] = "--";
  char bpmDec[16] = ".-";

  if (d->State->CurrentBPM > 0.0f) {
    float bpmTarget = d->State->CurrentBPM;
    int bpmWhole = (int)bpmTarget;
    int bpmFraction = (int)((bpmTarget - bpmWhole) * 10);

    sprintf(bpmMain, "%d", bpmWhole);
    sprintf(bpmDec, ".%d", bpmFraction);
  }

  float wBmain = MeasureTextEx(faceBPM, bpmMain, S(20), 1).x;
  float wBdec = MeasureTextEx(faceMd, bpmDec, S(11), 1).x;

  float combinedX = bpmX + (bpmBoxW - (wBmain + wBdec)) / 2;
  float bpmValY = bpmBoxY + S(8);

  UIDrawText(bpmMain, faceBPM, combinedX, bpmValY, S(20), ColorOrange);
  UIDrawText(bpmDec, faceMd, combinedX + wBmain, bpmValY + S(3.5f), S(11),
             ColorOrange);

  // Sync Button
  float syncY = bpmBoxY + bpmBoxH + S(2);
  float syncH = S(12);

  Color syncColor = ColorShadow;
  const char *syncText = "SYNC";
  if (d->State->SyncMode == 1) {
    syncColor = ColorOrange;
    syncText = "BPM";
  } else if (d->State->SyncMode == 2) {
    syncColor = ColorOrange;
    syncText = "BEAT";
  }

  DrawRectangleLines(bpmX, syncY, bpmBoxW, syncH, syncColor);
  DrawCentredText(syncText, faceXXS, bpmX, bpmBoxW, syncY + S(2.5f), S(7),
                  syncColor);

  // Static Waveform rendering
  if (d->State->LoadedTrack != NULL) {
    float wx = lColX + lColW + S(4);
    float ww = stripW - lColW - S(10);
    float wy = TOP_BAR_H + WAVE_AREA_H + FX_BAR_H + DECK_STR_H - S(28);
    float wh = S(20);

    DrawRectangle(wx, wy, ww, wh, ColorDark3);
    DrawRectangleLines(wx, wy, ww, wh, ColorDark1);

    int totalFrames = d->State->LoadedTrack->StaticWaveformLen;
    float totalMs = d->State->TrackLengthMs;
    float playedRatio = 0;
    if (totalMs > 0)
      playedRatio = (float)playedMs / totalMs;

    // High-Res Static Mini Waveform (Quads)
    if (totalFrames > 1 && ww > 0) {
      rlBegin(RL_QUADS);
      for (int x = 0; x < (int)ww - 1; x++) {
        float r1 = (float)x / ww;
        float r2 = (float)(x + 1) / ww;
        float r3 = (float)(x + 2) / ww;

        int idx1 = (int)(r1 * totalFrames);
        int idx2 = (int)(r2 * totalFrames);
        int idx3 = (int)(r3 * totalFrames);

        if (idx1 >= totalFrames) idx1 = totalFrames - 1;
        if (idx2 >= totalFrames) idx2 = totalFrames - 1;
        if (idx3 >= totalFrames) idx3 = totalFrames - 1;
        if (idx1 < 0) idx1 = 0;

        unsigned char s1 = GetStaticWaveformPeak(d->State->LoadedTrack->StaticWaveform, idx1, idx2);
        unsigned char s2 = GetStaticWaveformPeak(d->State->LoadedTrack->StaticWaveform, idx2, idx3);

        float b1, m1, h1, b2, m2, h2;
        DecodeStaticWaveformBands(s1, wh, &b1, &m1, &h1);
        DecodeStaticWaveformBands(s2, wh, &b2, &m2, &h2);

        Color cB = {16, 105, 238, 255};
        Color cM = {16, 190, 82, 255};
        Color cH = {246, 251, 246, 255};

        if (r1 <= playedRatio) {
            cB = (Color){8, 52, 119, 255};
            cM = (Color){8, 95, 41, 255};
            cH = (Color){123, 125, 123, 255};
        }

        float cx1 = wx + x;
        float cx2 = wx + x + 1.0f;
        float yy = wy + wh;

        // Bottom Aligned Envelope Fill (Correct CCW Winding: TL, BL, BR, TR)
        // Bass
        rlColor4ub(cB.r, cB.g, cB.b, cB.a);
        rlVertex2f(cx1, yy - b1); // TL
        rlVertex2f(cx1, yy);      // BL
        rlVertex2f(cx2, yy);      // BR
        rlVertex2f(cx2, yy - b2); // TR

        // Mid
        rlColor4ub(cM.r, cM.g, cM.b, cM.a);
        rlVertex2f(cx1, yy - m1); // TL
        rlVertex2f(cx1, yy);      // BL
        rlVertex2f(cx2, yy);      // BR
        rlVertex2f(cx2, yy - m2); // TR

        // High
        rlColor4ub(cH.r, cH.g, cH.b, cH.a);
        rlVertex2f(cx1, yy - h1); // TL
        rlVertex2f(cx1, yy);      // BL
        rlVertex2f(cx2, yy);      // BR
        rlVertex2f(cx2, yy - h2); // TR
      }
      rlEnd();
    }

    // Cue Markers Rendering (Hot Cues & Memory Cues)
    if (totalMs > 0) {
      // Hot Cues
      for (int h = 0; h < d->State->LoadedTrack->HotCuesCount; h++) {
        float ratio = (float)d->State->LoadedTrack->HotCues[h].Start / totalMs;
        if (ratio >= 0.0f && ratio <= 1.0f) {
          float rx = wx + ratio * ww;
          Color hcClr = {d->State->LoadedTrack->HotCues[h].Color[0], d->State->LoadedTrack->HotCues[h].Color[1], d->State->LoadedTrack->HotCues[h].Color[2], 255};
          if (hcClr.r == 0 && hcClr.g == 0 && hcClr.b == 0) {
            Color palette[8] = {{0, 255, 0, 255}, {255, 0, 0, 255}, {255, 128, 0, 255}, {255, 255, 0, 255}, {0, 0, 255, 255}, {255, 0, 255, 255}, {0, 255, 255, 255}, {128, 0, 255, 255}};
            int idx = d->State->LoadedTrack->HotCues[h].ID - 1;
            if (idx < 0) idx = 0; if (idx > 7) idx = 7;
            hcClr = palette[idx];
          }
          DrawTriangle((Vector2){rx - 4, wy}, (Vector2){rx + 4, wy}, (Vector2){rx, wy + 6}, hcClr);
          DrawLineV((Vector2){rx, wy}, (Vector2){rx, wy + wh}, (Color){hcClr.r, hcClr.g, hcClr.b, 100});
        }
      }
      // Memory Cues
      for (int c = 0; c < d->State->LoadedTrack->CuesCount; c++) {
        float ratio = (float)d->State->LoadedTrack->Cues[c].Start / totalMs;
        if (ratio >= 0.0f && ratio <= 1.0f) {
          float rx = wx + ratio * ww;
          DrawTriangle((Vector2){rx - 3, wy + wh}, (Vector2){rx, wy + wh - 5}, (Vector2){rx + 3, wy + wh}, ColorOrange);
        }
      }
    }

    // Playhead Position Line
    if (totalMs > 0) {
      float progX = wx + playedRatio * ww;
      DrawRectangle(progX, wy, 2, wh, ColorRed);
    }
  }
}

void DeckStrip_Init(DeckStrip *d, int id, DeckState *state) {
  d->base.Update = DeckStrip_Update;
  d->base.Draw = DeckStrip_Draw;
  d->ID = id;
  d->State = state;
}
