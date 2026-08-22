#include "ui/player/deckstrip.h"
#include "audio/engine.h"
#include "input/input.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/waveform.h"
#include <math.h>
#include <rlgl.h>
#include <stdio.h>
#include <string.h>

static void drawLeftBadgeColumn(DeckStrip *d, float x, float y, float h) {
  float lColW = S(40);
  float lColX = x;

  Font faceXXS = UIFonts_GetFace(S(7));
  Font faceSm = UIFonts_GetFace(S(9));
  Font faceMd = UIFonts_GetFace(S(11));
  Font faceLg = UIFonts_GetFace(S(18));

  // 1. DECK Header
  float headH = 21;
  Color headColor = Theme.BgMain;
  if (d->State->IsLoading && ((int)(GetTime() * 4.0f) % 2 == 0)) {
    headColor = Theme.AccentOrange;
  }

  DrawRectangle(lColX, y, lColW, h, Theme.BgPanelAlt);

  DrawRectangle(lColX, y, lColW, S(headH), headColor);
  DrawRectangle(lColX, y + S(headH) - S(1), lColW, S(1), Theme.BorderDefault);

  DrawCentredText("DECK", faceXXS, lColX, lColW, y + S(2), S(7), Theme.TextPrimary);

  float badgeY = y + S(9);
  DrawCentredText("((   ))", faceSm, lColX, lColW, badgeY, S(9), Theme.AccentRed);

  char idStr[16];
  sprintf(idStr, "%d", d->ID + 1);
  DrawCentredText(idStr, faceSm, lColX, lColW, badgeY, S(9), Theme.TextPrimary);

  // 2. TRACK Section
  float trackY = 24;
  DrawCentredText("TRACK", faceXXS, lColX, lColW, y + S(trackY), S(7),
                  Theme.BorderDefault);

  char trackStr[16] = "---";
  if (d->State->LoadedTrack != NULL) {
    sprintf(trackStr, "%02d", d->State->TrackNumber);
  }
  DrawCentredText(trackStr, faceLg, lColX, lColW, y + S(trackY + 5), S(18),
                  Theme.TextPrimary);

  // 3. PLAY MODE (SINGLE / CONTINUE)
  float singleY = 52;
  const char *playModeStr = (d->State->PlayMode == 0) ? "CONTINUE" : "SINGLE";
  Color playModeColor = (d->State->PlayMode == 0) ? Theme.AccentOrange : Theme.BorderDefault;
  DrawCentredText(playModeStr, faceXXS, lColX, lColW, y + S(singleY), S(7),
                  playModeColor);

  // 4. QUANTIZE
  float quantizeY = 68;
  Color quantizeColor = Theme.BorderDefault;
  const char *quantizeStr = "OFF";

  if (d->State->QuantizeEnabled) {
    quantizeColor = Theme.AccentRed;
    quantizeStr = "1";
  }

  DrawCentredText("QUANTIZE", faceXXS, lColX, lColW, y + S(quantizeY), S(7),
                  quantizeColor);
  DrawCentredText(quantizeStr, faceMd, lColX, lColW, y + S(quantizeY + 8.5f),
                  S(11), quantizeColor);

  DrawLine(lColX + lColW, y, lColX + lColW, y + h, Theme.BorderDefault);
}

static int DeckStrip_Update(Component *base) {
  DeckStrip *d = (DeckStrip *)base;
  float stripW = SCREEN_WIDTH / 2.0f;
  float x = d->ID * stripW;
  float y = TOP_BAR_H + WAVE_AREA_H + FX_BAR_H;
  float titleBgH = S(16);
  float midY = y + titleBgH + S(4);
  float lColW = S(40);
  float bpmBoxW = S(56);
  float bpmX = x + stripW - bpmBoxW - S(4);
  float tempoX = bpmX - S(86);

  float mx = x + lColW + S(4);
  float mtX = mx;
  float mtY = midY + S(29);
  float mtW = S(48);
  float mtH = S(12);

  float bpmBoxH = S(28);
  float syncY = midY + bpmBoxH + S(2);
  float syncH = S(12);

  Vector2 mouse = Input_GetPointerPos();
  bool isHoverTempoArea = (mouse.x >= tempoX && mouse.x <= bpmX + bpmBoxW &&
                           mouse.y >= midY && mouse.y <= midY + S(28));

  // Time Mode Toggle
  float tx = mtX + mtW + S(6);

  if (Touch_CheckClick((Rectangle){x, y + S(50), lColW, S(16)}, S(2.0f))) {
    d->State->PlayMode = (d->State->PlayMode == 0) ? 1 : 0;
  }
  if (Touch_CheckClick((Rectangle){x, y + S(68), lColW, S(20)}, S(2.0f))) {
    d->State->QuantizeEnabled = !d->State->QuantizeEnabled;
  }

  float jogY = midY + S(1);
  if (Touch_CheckClick((Rectangle){mx, jogY, S(48), S(12)}, S(2.0f))) {
    d->State->VinylModeEnabled = !d->State->VinylModeEnabled;
  }

  if (Touch_CheckClick((Rectangle){mtX, mtY, mtW, mtH}, S(2.0f))) {
    d->State->MasterTempo = !d->State->MasterTempo;
  }
  if (Touch_CheckClick((Rectangle){bpmX, syncY, bpmBoxW, syncH}, S(2.0f))) {
    d->State->SyncMode = (d->State->SyncMode + 1) % 3;
  }
  if (Touch_CheckClick((Rectangle){tx, midY, S(80), S(28)}, S(2.0f))) {
    d->State->TimeMode = (d->State->TimeMode + 1) % 2;
  }
  if (Touch_CheckClick((Rectangle){tempoX, midY, S(86), S(28)}, S(2.0f))) {
    d->State->TempoRange = (d->State->TempoRange + 1) % 4;
  }

  // Mouse Wheel for Tempo
  if (isHoverTempoArea) {
    float wheel = Mouse_GetWheel();
    if (wheel != 0.0f) {
      float step = 0.02f;
      if (d->State->TempoRange == 1)
        step = 0.05f;
      else if (d->State->TempoRange == 2)
        step = 0.05f;
      else if (d->State->TempoRange == 3)
        step = 0.5f;

      d->State->TempoPercent += (step * wheel);

      float maxRange = 6.0f;
      if (d->State->TempoRange == 1)
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

  // Handle Seeks via waveform touch
  static bool uiTouching[2] = {false, false};
  static bool wasPlayingBeforeSeek[2] = {false, false};
  if (d->State->LoadedTrack != NULL && d->State->TrackLengthMs > 0) {
    float wx = x + lColW + S(4);
    float ww = stripW - lColW - S(8);
    float wy = y + DECK_STR_H - S(30);
    float wh = S(26);
    Rectangle wfmRect = {wx, wy, ww, wh};

    if (Input_IsPressed() && CheckCollisionPointRec(mouse, wfmRect)) {
      if (!uiTouching[d->ID]) {
        uiTouching[d->ID] = true;
        wasPlayingBeforeSeek[d->ID] = d->State->IsPlaying;
        if (d->State->IsPlaying) {
          d->State->IsPreviewing = true; // Start preview mode
        }
      }
    }

    if (uiTouching[d->ID]) {
      if (Input_IsDown()) {
        float ratio = (mouse.x - wx) / ww;
        if (ratio < 0.0f)
          ratio = 0.0f;
        if (ratio > 1.0f)
          ratio = 1.0f;
        long long targetMs =
            (long long)(ratio * (double)d->State->TrackLengthMs);

        if (wasPlayingBeforeSeek[d->ID]) {
          // Just previewing, do not seek audio engine
          d->State->PositionMs = targetMs;
          d->State->Position = (targetMs / 1000.0) * 150.0;
        } else {
          // Paused, so we actually seek
          d->State->SeekMs = targetMs;
          d->State->PositionMs = targetMs;
          d->State->Position = (targetMs / 1000.0) * 150.0;
          d->State->HasSeekRequest = true;
        }
      } else {
        uiTouching[d->ID] = false;
        if (wasPlayingBeforeSeek[d->ID]) {
          d->State->IsPreviewing =
              false; // Stop preview mode, snap back to playing pos
        }
      }
    }
  }

  return 0;
}

static void DeckStrip_Draw(Component *base) {
  DeckStrip *d = (DeckStrip *)base;
  float stripW = SCREEN_WIDTH / 2.0f;
  float x = d->ID * stripW;
  float y = TOP_BAR_H + WAVE_AREA_H + FX_BAR_H;

  DrawRectangle(x, y, stripW, DECK_STR_H, Theme.BgMain);
  DrawLine(x, y, x + stripW, y, Theme.BorderDefault);
  if (d->ID == 0)
    DrawLine(stripW, y, stripW, y + DECK_STR_H, Theme.BorderDefault);

  // 1. LEFT Badge
  drawLeftBadgeColumn(d, x, y, DECK_STR_H);

  Font faceXXS = UIFonts_GetFace(S(7));
  Font faceXXSBold = UIFonts_GetBoldFace(S(7));
  Font faceMd = UIFonts_GetFace(S(11));
  Font faceBPM = UIFonts_GetFace(S(20));

  float lColW = S(40);
  float lColX = x;

  float mx = lColX + lColW + S(4);
  float titleBgH = S(16);
  DrawRectangle(lColX + lColW, y, stripW - lColW, titleBgH,
                Theme.BgMain);

  Font titleFont = UIFonts_GetBoldFace(S(12.0f));
  Font iconFace = UIFonts_GetIcon(S(11.0f));

  char title[150] = "No Track";
  if (d->State->TrackTitle[0] != '\0') {
    strncpy(title, d->State->TrackTitle, sizeof(title) - 1);

    UIDrawText("\xef\x80\x81", iconFace, mx, y + S(2.5f), S(11),
               Theme.TextPrimary); // f001 music note
    float maxW = stripW - (mx - x) - S(20);
    Rectangle titleRect = {mx + S(15), y + S(1.5f), maxW, S(16)};
    static float autoTimer[2] = {0, 0};
    static const TrackState *lastTrack[2] = {NULL, NULL};
    if (d->State->LoadedTrack != lastTrack[d->ID]) {
      lastTrack[d->ID] = d->State->LoadedTrack;
      autoTimer[d->ID] = 0.0f;
    }
    autoTimer[d->ID] += GetFrameTime();

    UIDrawScrollingText(title, titleFont, titleRect, S(12.0f), Theme.TextPrimary,
                        autoTimer[d->ID]);
  } else {
    UIDrawText(title, titleFont, mx, y + S(1.5f), S(12.0f), Theme.TextPrimary);
  }

  float midY = y + titleBgH + S(4);
  float badgeX = mx;
  float badgeW = S(48);
  float badgeH = S(12);

  bool vinylOn = d->State->VinylModeEnabled;
  Color jogClr = vinylOn ? Theme.AccentBlue : Theme.AccentRed;
  const char *jogStr = vinylOn ? "VINYL" : "CDJ";
  DrawRectangleLines(badgeX, midY + S(1), badgeW, badgeH, jogClr);
  DrawCentredText(jogStr, faceXXS, badgeX, badgeW, midY + S(3.0f), S(7),
                  jogClr);

  DrawRectangleLines(badgeX, midY + S(15), badgeW, badgeH, Theme.BorderDefault);
  DrawCentredText("AUTO CUE", faceXXS, badgeX, badgeW, midY + S(17.0f), S(7),
                  Theme.TextPrimary);

  Color mtClr = d->State->MasterTempo ? Theme.AccentRed : Theme.BorderDefault;
  DrawRectangleLines(badgeX, midY + S(29), badgeW, badgeH, mtClr);
  DrawCentredText("MT", faceXXS, badgeX, badgeW, midY + S(31.0f), S(7), mtClr);

  float timeX = mx + badgeW + S(6);

  // REMAIN / TIME Indicators
  Font elFace = (d->State->TimeMode == 0) ? UIFonts_GetBoldFace(S(7)) : faceXXS;
  Font rmFace = (d->State->TimeMode == 1) ? UIFonts_GetBoldFace(S(7)) : faceXXS;
  Color elClr = (d->State->TimeMode == 0) ? Theme.TextPrimary : Theme.BorderDefault;
  Color rmClr = (d->State->TimeMode == 1) ? Theme.TextPrimary : Theme.BorderDefault;

  UIDrawText("TIME", elFace, timeX + S(50), midY, S(7), elClr);
  UIDrawText("/", faceXXS, timeX + S(43), midY, S(7), Theme.BorderDefault);
  UIDrawText("REMAIN", rmFace, timeX + S(7), midY, S(7), rmClr);

  long long displayMs = d->State->PositionMs;
  if (d->State->TimeMode == 1 && d->State->LoadedTrack != NULL) {
    displayMs = (long long)d->State->TrackLengthMs - d->State->PositionMs;
  }

  bool isNeg = displayMs < 0;
  long long absMs = isNeg ? -displayMs : displayMs;

  int minutes = (int)((absMs / 1000) / 60);
  int seconds = (int)((absMs / 1000) % 60);
  int subSec = (int)(absMs % 1000);

  char timeStr[16];
  if (isNeg) {
    sprintf(timeStr, "-%02d:%02d", minutes, seconds);
  } else {
    sprintf(timeStr, "%02d:%02d", minutes, seconds);
  }

  Font faceTime = UIFonts_GetTimeFace(S(18));
  Font faceTimeSub = UIFonts_GetTimeFace(S(13));

  float timeValY = midY + S(9);
  UIDrawText(timeStr, faceTime, timeX, timeValY, S(18), Theme.TextPrimary);

  float wM = MeasureTextEx(faceTime, timeStr, S(18), 1).x;
  char subStr[8];
  sprintf(subStr, ".%03d", subSec);
  UIDrawText(subStr, faceTimeSub, timeX + wM + S(2), timeValY + S(4), S(13),
             Theme.TextPrimary);

  float bpmBoxW = S(56);
  float bpmX = x + stripW - bpmBoxW - S(4);
  float tempoX = bpmX - S(86);

  UIDrawText("TEMPO RANGE", faceXXS, tempoX, midY, S(7), Theme.BorderDefault);

  const char *rangeStr = "10%";
  Color rangeBgCol = Theme.AccentOrange; // Dark Orange
  if (d->State->TempoRange == 0) {
    rangeStr = " 6%";
    rangeBgCol = Theme.AccentGreen; // Dark Green
  } else if (d->State->TempoRange == 2) {
    rangeStr = "16%";
    rangeBgCol = Theme.AccentBlue; // Dark Blue
  } else if (d->State->TempoRange == 3) {
    rangeStr = "WIDE";
    rangeBgCol = Theme.AccentRed; // Dark Red
  }

  float tBadgeW = 24.0f;
  float tBadgeY = midY - S(0.5f);
  float tempoBadgeX = bpmX - S(8) - S(tBadgeW);
  DrawRectangle(tempoBadgeX, tBadgeY, S(tBadgeW), S(9), rangeBgCol);
  DrawCentredText(rangeStr, faceXXSBold, tempoBadgeX, S(tBadgeW),
                  tBadgeY + S(0.5f), S(7), Theme.TextPrimary);

  char tempoStr[32];
  sprintf(tempoStr, "%+.2f%%", d->State->TempoPercent);
  if (d->State->TempoPercent == 0.0f)
    sprintf(tempoStr, " 0.00%%");

  float wT = MeasureTextEx(faceTime, tempoStr, S(18), 1).x;
  float valX = (bpmX - S(8)) - wT;
  UIDrawText(tempoStr, faceTime, valX, midY + S(9), S(18), Theme.TextPrimary);

  float bpmBoxH = S(28);
  float bpmBoxY = midY;
  DrawRectangleLines(bpmX, bpmBoxY, bpmBoxW, bpmBoxH, Theme.AccentOrange);
  UIDrawText("BPM", faceXXS, bpmX + S(2), bpmBoxY + S(0.5f), S(7), Theme.BorderDefault);

  if (d->State->IsMaster) {
    float masterW = S(36);
    float masterX = bpmX + bpmBoxW - masterW - S(1);
    DrawRectangle(masterX, bpmBoxY + S(1), masterW, S(8), Theme.AccentOrange);
    DrawCentredText("MASTER", faceXXS, masterX, masterW, bpmBoxY + S(0.5f),
                    S(7), Theme.BgMain);
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

  UIDrawText(bpmMain, faceBPM, combinedX, bpmValY, S(20), Theme.AccentOrange);
  UIDrawText(bpmDec, faceMd, combinedX + wBmain, bpmValY + S(3.5f), S(11),
             Theme.AccentOrange);

  // Sync Button
  float syncY = bpmBoxY + bpmBoxH + S(2);
  float syncH = S(12);

  Color syncColor = Theme.BorderDefault;
  const char *syncText = "SYNC";
  if (d->State->SyncMode == 1) {
    syncColor = Theme.TextPrimary;
    syncText = "BPM";
  } else if (d->State->SyncMode == 2) {
    syncColor = Theme.AccentBlue;
    syncText = "BEAT";
  }

  DrawRectangleLines(bpmX, syncY, bpmBoxW, syncH, syncColor);
  DrawCentredText(syncText, faceXXS, bpmX, bpmBoxW, syncY + S(2.5f), S(7),
                  syncColor);

  // === New Preview Waveform rendering ===
  if (d->State->LoadedTrack != NULL) {
    float wx = lColX + lColW + S(4);
    float ww = stripW - lColW - S(8);
    float wy = y + DECK_STR_H - S(30);
    float wh = S(26); // Increased height for S(4) bottom margin

    // Background box
    DrawRectangle(wx, wy, ww, wh, Theme.BgMain);
    DrawRectangleLinesEx((Rectangle){wx, wy, ww, wh}, 1.0f, Theme.BorderDefault);

    // Center guide line
    DrawLine(wx, wy + wh * 0.5f, wx + ww, wy + wh * 0.5f,
             Theme.BorderDefault);

    int type = d->State->LoadedTrack->Analysis.StaticWaveformType;
    unsigned char *data = d->State->LoadedTrack->Analysis.StaticWaveform;
    int dataLen = d->State->LoadedTrack->Analysis.StaticWaveformLen;
    float totalMs = (float)d->State->TrackLengthMs;
    float playedRatio =
        (totalMs > 0) ? (float)d->State->PositionMs / totalMs : 0;

    if (dataLen > 0 && ww > 0) {
      rlDrawRenderBatchActive();
      rlBegin(RL_TRIANGLES);

      extern AudioEngine *globalAudioEngine;
      float eqLowMult = 1.0f, eqMidMult = 1.0f, eqHighMult = 1.0f;
      if (globalAudioEngine != NULL && d->ID >= 0 && d->ID < 2) {
        eqLowMult = globalAudioEngine->Decks[d->ID].EqLow * 2.0f;
        eqMidMult = globalAudioEngine->Decks[d->ID].EqMid * 2.0f;
        eqHighMult = globalAudioEngine->Decks[d->ID].EqHigh * 2.0f;
      }

      WaveformStyle style = d->State->Waveform.Style;
      float gLow = d->State->Waveform.GainLow * eqLowMult;
      float gMid = d->State->Waveform.GainMid * eqMidMult;
      float gHigh = d->State->Waveform.GainHigh * eqHighMult;

      // Decide which data source to use (High-res Dynamic or Low-res Static)
      // We prefer dynamic for 3-Band because it looks much better
      bool useDyn = (d->State->LoadedTrack->Analysis.DynamicWaveformLen > 0);
      unsigned char *renderData =
          useDyn ? d->State->LoadedTrack->Analysis.DynamicWaveform : data;
      int renderType =
          useDyn ? d->State->LoadedTrack->Analysis.WaveformType : type;

      int bpf = 1;
      if (renderType == 3)
        bpf = 3; // PWV7
      else if (renderType == 2)
        bpf = 2; // PWV4
      else
        bpf = 1; // PWV2

      int totalFrames =
          (useDyn ? d->State->LoadedTrack->Analysis.DynamicWaveformLen
                  : dataLen) /
          bpf;
      float yy = wy + wh * 0.5f;

      // hardware-accurate 3-band palette
      Color BL_LOW = {16, 105, 238, 255};   // col_blue
      Color BL_MID = {16, 190, 82, 255};    // col_green
      Color BL_HIGH = {246, 251, 246, 255}; // col_white

      float smLo = 0, smMi = 0, smHi = 0;
      const float ATK = 0.9f;
      const float REL = 0.12f;

      for (int xi = 0; xi < (int)ww; xi++) {
        float r0 = (float)xi / ww;
        float r1 = (float)(xi + 1) / ww;

        double p0 = r0 * totalFrames;
        double p1 = r1 * totalFrames;

        float cx0 = wx + xi;
        float cx1 = wx + xi + 1.0f;
        bool played = (r0 < playedRatio);

        float rL = 0, rM = 0, rH = 0;
        Color col = Theme.AccentBlue;
        float halfH = wh * 0.5f;

        // 1. DATA EXTRACTION & NORMALIZATION based on renderType (PWV2, PWV4,
        // PWV7)
        if (renderType == 3) {
          // PWV7: 3-Band data [Mid, High, Low] (values 0..255)
          Get3BandPeak(renderData, totalFrames, p0, p1, &rL, &rM, &rH);
          rL = (rL / 255.0f) * halfH * gLow * 1.8f;
          rM = (rM / 255.0f) * halfH * gMid * 1.8f;
          rH = (rH / 255.0f) * halfH * gHigh * 1.8f;
        } else if (renderType == 2) {
          // PWV4: RGB data (2 bytes per frame)
          int maxH = 0;
          Color maxCol = {0, 0, 0, 255};
          int startF = (int)floor(p0);
          int endF = (int)ceil(p1);
          if (startF < 0)
            startF = 0;
          if (endF >= totalFrames)
            endF = totalFrames - 1;
          if (endF < startF)
            endF = startF;

          for (int f = startF; f <= endF; f++) {
            Color tmpCol;
            int h = PWV4_Decode(renderData, f, totalFrames, &tmpCol);
            if (h > maxH) {
              maxH = h;
              maxCol = tmpCol;
            }
          }
          col = maxCol;
          float baseH = (maxH / 31.0f) * halfH;
          rL = baseH * ((float)col.r / 255.0f) * gLow * 1.8f;
          rM = baseH * ((float)col.g / 255.0f) * gMid * 1.8f;
          rH = baseH * ((float)col.b / 255.0f) * gHigh * 1.8f;
        } else {
          // PWV2: Blue data (1 byte per frame)
          int maxH = 0;
          Color maxCol = {0, 0, 0, 255};
          int startF = (int)floor(p0);
          int endF = (int)ceil(p1);
          if (startF < 0)
            startF = 0;
          if (endF >= totalFrames)
            endF = totalFrames - 1;
          if (endF < startF)
            endF = startF;

          for (int f = startF; f <= endF; f++) {
            Color tmpCol;
            int h = PWV2_Decode(renderData[f], &tmpCol);
            if (h > maxH) {
              maxH = h;
              maxCol = tmpCol;
            }
          }
          col = maxCol;
          float baseH = (maxH / 31.0f) * halfH;
          if (col.r > col.b && col.r > col.g) {
            rL = baseH * 0.4f * gLow * 1.8f;
            rM = baseH * 0.9f * gMid * 1.8f;
            rH = baseH * 0.2f * gHigh * 1.8f;
          } else if (col.b > col.r && col.b > col.g) {
            rL = baseH * 0.95f * gLow * 1.8f;
            rM = baseH * 0.6f * gMid * 1.8f;
            rH = baseH * 0.1f * gHigh * 1.8f;
          } else {
            rL = baseH * 0.8f * gLow * 1.8f;
            rM = baseH * 0.8f * gMid * 1.8f;
            rH = baseH * 0.6f * gHigh * 1.8f;
          }
        }

        // 2. STYLE APPLICATION (Apply Gains & Final Colors)
        if (style == WAVEFORM_STYLE_3BAND) {
          // Render as 3 discrete layers
          if (rL > halfH)
            rL = halfH;
          if (rM > halfH)
            rM = halfH;
          if (rH > halfH)
            rH = halfH;

          // Force strict nesting (Blue > Orange > White)
          rM = fmaxf(rM, rH);
          rL = fmaxf(rL, rM);

          float pLo = smLo;
          float pMi = smMi;
          float pHi = smHi;

          smLo = rL;
          smMi = rM;
          smHi = rH;

          // Boost and Clamp for visibility
          if (smLo > halfH)
            smLo = halfH;
          if (smMi > halfH)
            smMi = halfH;
          if (smHi > halfH)
            smHi = halfH;

          if (smLo < 0.5f && rL > 0)
            smLo = 0.5f;
          if (smMi < 0.5f && rM > 0)
            smMi = 0.5f;
          if (smHi < 0.5f && rH > 0)
            smHi = 0.5f;

          Color clL = played ? Fade(BL_LOW, 0.4f) : BL_LOW;
          Color clM = played ? Fade(BL_MID, 0.4f) : BL_MID;
          Color clH = played ? Fade(BL_HIGH, 0.4f) : BL_HIGH;

#define DRAW_TRAP_STATIC(p, c, cl)                                             \
  if (p > 0.1f || c > 0.1f) {                                                  \
    rlColor4ub(cl.r, cl.g, cl.b, cl.a);                                        \
    rlVertex2f(cx0, yy - p);                                                   \
    rlVertex2f(cx0, yy + p);                                                   \
    rlVertex2f(cx1, yy + c);                                                   \
    rlVertex2f(cx0, yy - p);                                                   \
    rlVertex2f(cx1, yy + c);                                                   \
    rlVertex2f(cx1, yy - c);                                                   \
  }
          DRAW_TRAP_STATIC(pLo, smLo, clL);
          DRAW_TRAP_STATIC(pMi, smMi, clM);
          DRAW_TRAP_STATIC(pHi, smHi, clH);
#undef DRAW_TRAP_STATIC

        } else {
          // Render as single symmetric layer (RGB or BLUE)
          float hVal = fmaxf(rL, fmaxf(rM, rH));
          Color finalCol = col;

          if (renderType == 3) {
            if (style == WAVEFORM_STYLE_RGB) {
              if (rL >= rM && rL >= rH)
                finalCol = BL_LOW;
              else if (rM >= rH)
                finalCol = BL_MID;
              else
                finalCol = BL_HIGH;
            } else
              finalCol = Theme.AccentBlue;
          } else {
            if (style == WAVEFORM_STYLE_RGB) {
              finalCol.r =
                  (unsigned char)fminf(255.0f, (float)col.r * eqLowMult);
              finalCol.g =
                  (unsigned char)fminf(255.0f, (float)col.g * eqMidMult);
              finalCol.b =
                  (unsigned char)fminf(255.0f, (float)col.b * eqHighMult);
            } else
              finalCol = Theme.AccentBlue;
          }

          if (hVal > halfH)
            hVal = halfH;
          float pVal = smLo;
          smLo = pVal + (hVal - pVal) * ((hVal > pVal) ? ATK : REL);

          Color finalC = played ? Fade(finalCol, 0.4f) : finalCol;
          rlColor4ub(finalC.r, finalC.g, finalC.b, finalC.a);
          rlVertex2f(cx0, yy - pVal);
          rlVertex2f(cx0, yy + pVal);
          rlVertex2f(cx1, yy + smLo);
          rlVertex2f(cx0, yy - pVal);
          rlVertex2f(cx1, yy + smLo);
          rlVertex2f(cx1, yy - smLo);
        }
      }
      rlEnd();
    } else {
      UIDrawText("WAVEFORM NOT LOADED", faceXXS, wx + S(10), wy + S(6), S(8),
                 Theme.BorderDefault);
    }

    // Cue Markers Rendering (Hot Cues & Memory Cues)
    if (totalMs > 0) {
      // Hot Cues
      for (int h = 0; h < d->State->LoadedTrack->HotCuesCount; h++) {
        float ratio = (float)d->State->LoadedTrack->HotCues[h].Start / totalMs;
        if (ratio >= 0.0f && ratio <= 1.0f) {
          float rx = wx + ratio * ww;
          static const Color hcPalette[8] = {
              {0, 255, 0, 255},   {255, 0, 0, 255},  {255, 128, 0, 255},
              {255, 255, 0, 255}, {0, 0, 255, 255},  {255, 0, 255, 255},
              {0, 255, 255, 255}, {128, 0, 255, 255}};

          int idx = d->State->LoadedTrack->HotCues[h].ID - 1;
          if (idx < 0)
            idx = 0;
          if (idx > 7)
            idx = 7;

          Color hcClr =
              GetCueColor(d->State->LoadedTrack->HotCues[h], hcPalette[idx]);

          // Thick line for pointer
          DrawLineEx((Vector2){rx, wy}, (Vector2){rx, wy + wh}, 2.0f, hcClr);

          // Downward triangle at top
          DrawTriangle((Vector2){rx - S(4), wy}, (Vector2){rx + S(4), wy},
                       (Vector2){rx, wy + S(6)}, hcClr);

          // Letter label with background
          char hcChar[2] = {(char)('A' + idx), 0};
          float txtW = MeasureTextEx(faceXXS, hcChar, S(7), 1).x;
          DrawRectangleRec((Rectangle){rx + S(2), wy, txtW + S(4), S(9)},
                           Theme.BgOverlay);
          UIDrawText(hcChar, faceXXS, rx + S(4), wy + S(1), S(7), hcClr);
        }
      }

      // Memory Cues
      for (int c = 0; c < d->State->LoadedTrack->CuesCount; c++) {
        float ratio = (float)d->State->LoadedTrack->Cues[c].Start / totalMs;
        if (ratio >= 0.0f && ratio <= 1.0f) {
          float rx = wx + ratio * ww;
          DrawLineEx((Vector2){rx, wy}, (Vector2){rx, wy + wh}, 1.0f,
                     Theme.AccentOrange);
        }
      }

      // Main Cue
      if (d->State->MainCueMs > 0) {
        float ratio = (float)d->State->MainCueMs / totalMs;
        if (ratio >= 0.0f && ratio <= 1.0f) {
          float rx = wx + ratio * ww;
          DrawLineEx((Vector2){rx, wy}, (Vector2){rx, wy + wh}, 1.5f,
                     Theme.TextPrimary);
          DrawTriangle((Vector2){rx - S(4), wy + wh},
                       (Vector2){rx + S(4), wy + wh},
                       (Vector2){rx, wy + wh - S(6)}, Theme.AccentOrange);
        }
      }

      // Playhead Position
      float ratio = (float)d->State->PositionMs / totalMs;
      if (ratio < 0)
        ratio = 0;
      if (ratio > 1)
        ratio = 1;
      float px = wx + ratio * ww;
      DrawRectangle(px - 1, wy, 2, wh, Theme.AccentRed);
    }

    // --- LOADING OVERLAY ---
    if (d->State->IsLoading) {
      float pulse = (sinf(GetTime() * 10.0f) * 0.5f + 0.5f);
      DrawRectangle(wx, wy, ww, wh, Fade(Theme.AccentOrange, 0.1f + pulse * 0.3f));
    }
  }

  // --- ENTIRE STRIP LOADING OVERLAY ---
  if (d->State->IsLoading) {
    float pulse = (sinf(GetTime() * 10.0f) * 0.5f + 0.5f);
    DrawRectangle(x, y, stripW, DECK_STR_H,
                  Fade(Theme.AccentOrange, 0.1f + pulse * 0.2f));
  }

  // Draw Bottom Borders
  DrawLine(x, y + DECK_STR_H - 1, x + stripW, y + DECK_STR_H - 1, Theme.BorderDefault);
}

void DeckStrip_Init(DeckStrip *d, int id, DeckState *state) {
  d->base.Update = DeckStrip_Update;
  d->base.Draw = DeckStrip_Draw;
  d->ID = id;
  d->State = state;
}
