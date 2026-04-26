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
  Color headColor = ColorBGUtil;
  if (d->State->IsLoading && ((int)(GetTime() * 4.0f) % 2 == 0)) {
      headColor = ColorOrange;
  }
  
  DrawRectangle(lColX, y, lColW, h, ColorDark3);

  DrawRectangle(lColX, y, lColW, S(headH), headColor);
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

  float mx = x + lColW + S(4);
  float mtX = mx;
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

  // Time Mode Toggle
  float tx = mtX + S(40) + S(8);
  bool isHoverTime = (mouse.x >= tx && mouse.x <= tx + S(80) &&
                      mouse.y >= midY && mouse.y <= midY + S(28));

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    if (isHoverQuantize)
      d->State->QuantizeEnabled = !d->State->QuantizeEnabled;
    if (isHoverMT)
      d->State->MasterTempo = !d->State->MasterTempo;
    if (isHoverSync)
      d->State->SyncMode = (d->State->SyncMode + 1) % 3;
    if (isHoverTime)
      d->State->TimeMode = (d->State->TimeMode + 1) % 2;
    if (isHoverTempoArea)
      d->State->TempoRange = (d->State->TempoRange + 1) % 4;
  }

  // Mouse Wheel for Tempo
  if (isHoverTempoArea) {
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      float step = 0.02f;
      if (d->State->TempoRange == 1) step = 0.05f;
      else if (d->State->TempoRange == 2) step = 0.05f;
      else if (d->State->TempoRange == 3) step = 0.5f;
      
      d->State->TempoPercent += (step * wheel);
      
      float maxRange = 6.0f;
      if (d->State->TempoRange == 1) maxRange = 10.0f;
      else if (d->State->TempoRange == 2) maxRange = 16.0f;
      else if (d->State->TempoRange == 3) maxRange = 100.0f;
      
      if (d->State->TempoPercent > maxRange) d->State->TempoPercent = maxRange;
      if (d->State->TempoPercent < -maxRange) d->State->TempoPercent = -maxRange;
    }
  }

  // Handle Seeks via waveform touch
  static bool uiTouching[2] = {false, false};
  if (d->State->LoadedTrack != NULL) {
    float wx = x + lColW + S(12);
    float ww = bpmX - wx - S(10);
    float wy = y + DECK_STR_H - S(30);
    float wh = S(22);
    Rectangle wfmRect = {wx, wy, ww, wh};

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, wfmRect)) {
      uiTouching[d->ID] = true;
    }

    if (uiTouching[d->ID]) {
      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        d->State->IsTouching = true; // Bypass MT
        float ratio = (mouse.x - wx) / ww;
        if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
        d->State->SeekMs = (long long)(ratio * d->State->TrackLengthMs);
        d->State->HasSeekRequest = true;
      } else {
        d->State->IsTouching = false;
        uiTouching[d->ID] = false;
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

  DrawRectangle(x, y, stripW, DECK_STR_H, ColorBlack);
  DrawLine(x, y, x + stripW, y, ColorGray);
  if (d->ID == 0)
    DrawLine(stripW, y, stripW, y + DECK_STR_H, ColorGray);

  // 1. LEFT Badge
  drawLeftBadgeColumn(d, x, y, DECK_STR_H);

  Font faceXXS = UIFonts_GetFace(S(7));
  Font faceSm = UIFonts_GetFace(S(9));
  Font faceMd = UIFonts_GetFace(S(11));
  Font faceLg = UIFonts_GetFace(S(18));
  Font faceSub = UIFonts_GetFace(S(13));
  Font faceBPM = UIFonts_GetFace(S(20));

  float lColW = S(40);
  float lColX = x;
  drawLeftBadgeColumn(d, x, y, DECK_STR_H);

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
  
  // REMAIN / TIME Indicators
  Font elFace = (d->State->TimeMode == 0) ? UIFonts_GetBoldFace(S(7)) : faceXXS;
  Font rmFace = (d->State->TimeMode == 1) ? UIFonts_GetBoldFace(S(7)) : faceXXS;
  Color elClr = (d->State->TimeMode == 0) ? ColorWhite : ColorShadow;
  Color rmClr = (d->State->TimeMode == 1) ? ColorWhite : ColorShadow;
  
  UIDrawText("TIME", elFace, timeX + S(50), midY, S(7), elClr);
  UIDrawText("/", faceXXS, timeX + S(43), midY, S(7), ColorShadow);
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
  float tBadgeY = midY - S(0.5f);
  DrawRectangle(tempoX + S(32), tBadgeY, S(tBadgeW), S(9), ColorOrange);
  DrawCentredText(rangeStr, faceXXS, tempoX + S(32), S(tBadgeW),
                  tBadgeY + S(0.5f), S(7), ColorBlack);

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

  // === New Preview Waveform rendering ===
  if (d->State->LoadedTrack != NULL) {
    float wx = lColX + lColW + S(12);
    float ww = bpmX - wx - S(10);
    float wy = y + DECK_STR_H - S(30); // Slightly more padding from bottom
    float wh = S(22); // Increased height

    // Background box
    DrawRectangle(wx, wy, ww, wh, (Color){5, 5, 5, 255});
    DrawRectangleLinesEx((Rectangle){wx, wy, ww, wh}, 1.0f, ColorDark1);
    
    // Center guide line
    DrawLine(wx, wy + wh * 0.5f, wx + ww, wy + wh * 0.5f, (Color){40, 40, 40, 255});

    int type = d->State->LoadedTrack->StaticWaveformType;
    unsigned char *data = d->State->LoadedTrack->StaticWaveform;
    int dataLen = d->State->LoadedTrack->StaticWaveformLen;
    float totalMs = (float)d->State->TrackLengthMs;
    float playedRatio = (totalMs > 0) ? (float)d->State->PositionMs / totalMs : 0;

    if (dataLen > 0 && ww > 0) {
      rlDrawRenderBatchActive();
      rlBegin(RL_TRIANGLES);

      WaveformStyle style = d->State->Waveform.Style;
      float gLow = d->State->Waveform.GainLow;
      float gMid = d->State->Waveform.GainMid;
      float gHigh = d->State->Waveform.GainHigh;

      // Decide which data source to use (High-res Dynamic or Low-res Static)
      // We prefer dynamic for 3-Band because it looks much better
      bool useDyn = (d->State->LoadedTrack->DynamicWaveformLen > 0);
      unsigned char *renderData = useDyn ? d->State->LoadedTrack->DynamicWaveform : data;
      int renderType = useDyn ? d->State->LoadedTrack->WaveformType : type;
      
      int bpf = 1;
      if (renderType == 3) bpf = 3;      // PWV7
      else if (renderType == 2) bpf = 2; // PWV4
      else bpf = 1;                     // PWV2
      
      int totalFrames = (useDyn ? d->State->LoadedTrack->DynamicWaveformLen : dataLen) / bpf;
      float yy = wy + wh * 0.5f;

      // Pioneer Palette
      Color BL_LOW = {32, 83, 217, 255};
      Color BL_MID = {242, 170, 60, 255};
      Color BL_HIGH = {255, 255, 255, 255};

      float smLo = 0, smMi = 0, smHi = 0;
      const float ATK = 0.85f; 
      const float REL = 0.40f;

      for (int xi = 0; xi < (int)ww; xi++) {
        float r0 = (float)xi / ww;
        float r1 = (float)(xi + 1) / ww;

        double p0 = r0 * totalFrames;
        double p1 = r1 * totalFrames;

        float cx0 = wx + xi;
        float cx1 = wx + xi + 1.0f;
        bool played = (r0 < playedRatio);

        float rL = 0, rM = 0, rH = 0;
        Color col = ColorBlue;

        // 1. DATA EXTRACTION based on renderType (PWV2, PWV4, PWV7)
        if (renderType == 3) {
           // PWV7: 3-Band data [Mid, High, Low]
           Get3BandPeak(renderData, totalFrames, p0, p1, &rL, &rM, &rH);
        } else if (renderType == 2) {
           // PWV4: RGB data (2 bytes)
           int h = PWV4_Decode(renderData, (int64_t)floor(p0 + 0.5), &col);
           float baseH = (h / 31.0f) * 255.0f;
           // Derive pseudo 3-band for style 3BAND
           if (col.r > col.b && col.r > col.g) { rL = baseH * 0.4f; rM = baseH * 0.9f; rH = baseH * 0.2f; }
           else if (col.b > col.r && col.b > col.g) { rL = baseH * 0.95f; rM = baseH * 0.6f; rH = baseH * 0.1f; }
           else { rL = baseH * 0.8f; rM = baseH * 0.8f; rH = baseH * 0.6f; }
           rL *= 0.5f; rM *= 0.5f; rH *= 0.5f; // Scale down for simulation
        } else {
           // PWV2: Blue data (1 byte)
           int h = PWV2_Decode(renderData[(int)p0], &col);
           float baseH = (h / 31.0f) * 255.0f;
           rL = baseH * 0.95f; rM = baseH * 0.6f; rH = baseH * 0.1f;
        }

        // 2. STYLE APPLICATION (Apply Gains & Final Colors)
        if (style == WAVEFORM_STYLE_3BAND) {
          // Render as 3 discrete layers
          rL = (rL / 255.0f) * wh * 0.5f * gLow * 2.1f;
          rM = (rM / 255.0f) * wh * 0.5f * gMid * 2.1f;
          rH = (rH / 255.0f) * wh * 0.5f * gHigh * 2.1f;
          
          if (rL > wh * 0.5f) rL = wh * 0.5f;
          if (rM > wh * 0.5f) rM = wh * 0.5f;
          if (rH > wh * 0.5f) rH = wh * 0.5f;

          float pLo = smLo; smLo = pLo + (rL - pLo) * ((rL > pLo) ? ATK : REL);
          float pMi = smMi; smMi = pMi + (rM - pMi) * ((rM > pMi) ? ATK : REL);
          float pHi = smHi; smHi = pHi + (rH - pHi) * ((rH > pHi) ? ATK : REL);

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
          float hVal = 0;
          Color finalCol = col;
          
          if (renderType == 3) {
             hVal = ((rL + rM + rH) / 3.0f) * (wh / 255.0f) * 0.5f * gLow * 2.1f;
             if (style == WAVEFORM_STYLE_RGB) {
                if (rL > rM && rL > rH) finalCol = BL_LOW;
                else if (rM > rH) finalCol = BL_MID;
                else finalCol = BL_HIGH;
             } else finalCol = ColorBlue;
          } else {
             float baseH = (renderType == 2 ? (rM / 0.9f) : (rL / 0.95f)); // Recover base height
             hVal = (baseH / 255.0f) * wh * 0.5f * gLow * 2.1f;
             if (style == WAVEFORM_STYLE_BLUE) finalCol = ColorBlue;
          }
          
          if (hVal > wh * 0.5f) hVal = wh * 0.5f;
          float pVal = smLo; smLo = pVal + (hVal - pVal) * ((hVal > pVal) ? ATK : REL);
          
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
                 ColorShadow);
    }

    // Cue Markers Rendering (Hot Cues & Memory Cues)
    if (totalMs > 0) {
      // Hot Cues
      for (int h = 0; h < d->State->LoadedTrack->HotCuesCount; h++) {
        float ratio = (float)d->State->LoadedTrack->HotCues[h].Start / totalMs;
        if (ratio >= 0.0f && ratio <= 1.0f) {
          float rx = wx + ratio * ww;
          static const Color hcPalette[8] = {
              {0, 255, 0, 255},   {255, 0, 0, 255},   {255, 128, 0, 255},
              {255, 255, 0, 255}, {0, 0, 255, 255},   {255, 0, 255, 255},
              {0, 255, 255, 255}, {128, 0, 255, 255}};
          
          int idx = d->State->LoadedTrack->HotCues[h].ID - 1;
          if (idx < 0) idx = 0; if (idx > 7) idx = 7;
          
          Color hcClr = GetCueColor(d->State->LoadedTrack->HotCues[h], hcPalette[idx]);
          
          // Downward triangle at top
          DrawTriangle((Vector2){rx - S(3), wy}, (Vector2){rx + S(3), wy},
                       (Vector2){rx, wy + S(5)}, hcClr);
          
          // Letter label
          char hcChar[2] = { (char)('A' + idx), 0 };
          UIDrawText(hcChar, faceXXS, rx + S(2), wy + S(1), S(7), hcClr);
        }
      }
      
      // Memory Cues
      for (int c = 0; c < d->State->LoadedTrack->CuesCount; c++) {
        float ratio = (float)d->State->LoadedTrack->Cues[c].Start / totalMs;
        if (ratio >= 0.0f && ratio <= 1.0f) {
          float rx = wx + ratio * ww;
          DrawLineEx((Vector2){rx, wy}, (Vector2){rx, wy + wh}, 1.0f, ColorOrange);
        }
      }

      // Playhead Position
      float ratio = (float)d->State->PositionMs / totalMs;
      if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
      float px = wx + ratio * ww;
      DrawRectangle(px - 1, wy, 2, wh, ColorRed);
    }
    
    // --- LOADING OVERLAY ---
    if (d->State->IsLoading) {
        float pulse = (sinf(GetTime() * 10.0f) * 0.5f + 0.5f);
        DrawRectangle(wx, wy, ww, wh, Fade(ColorOrange, 0.1f + pulse * 0.3f));
    }
  }

  // Draw Bottom Borders
  DrawLine(x, y + DECK_STR_H - 1, x + stripW, y + DECK_STR_H - 1, ColorDark1);
}

void DeckStrip_Init(DeckStrip *d, int id, DeckState *state) {
  d->base.Update = DeckStrip_Update;
  d->base.Draw = DeckStrip_Draw;
  d->ID = id;
  d->State = state;
}
