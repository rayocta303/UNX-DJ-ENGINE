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
  bool isHoverMT = (mouse.x >= mtX && mouse.x <= mtX + mtW && mouse.y >= mtY &&
                    mouse.y <= mtY + mtH);
  bool isHoverQuantize = (mouse.x >= x && mouse.x <= x + lColW &&
                          mouse.y >= y + S(68) && mouse.y <= y + S(88));
  bool isHoverSync = (mouse.x >= bpmX && mouse.x <= bpmX + bpmBoxW &&
                      mouse.y >= syncY && mouse.y <= syncY + syncH);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    if (isHoverQuantize)
      d->State->QuantizeEnabled = !d->State->QuantizeEnabled;
    if (isHoverMT)
      d->State->MasterTempo = !d->State->MasterTempo;
    if (isHoverSync) {
      if (d->State->SyncMode == 0)
        d->State->SyncMode = 1;
      else if (d->State->SyncMode == 1)
        d->State->SyncMode = 2;
      else
        d->State->SyncMode = 0;
    }
  }

  // Handle Seeks via waveform touch
  if (d->State->LoadedTrack != NULL) {
    float wx = x + lColW + S(12);
    float ww = bpmX - wx - S(10);
    float wy = y + DECK_STR_H - S(28);
    float wh = S(18);
    Rectangle wfmRect = {wx, wy, ww, wh};

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointRec(mouse, wfmRect)) {
      float ratio = (mouse.x - wx) / ww;
      if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
      d->State->SeekMs = (long long)(ratio * d->State->TrackLengthMs);
      d->State->HasSeekRequest = true;
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

  // 2. TITLE BAR
  float lColW = S(40);
  float lColX = x;
  float titleX = lColX + lColW + S(4);
  float titleBgH = S(16);
  DrawRectangle(titleX, y + S(2), stripW - lColW - S(8), titleBgH, (Color){20, 20, 20, 255});
  
  Font faceXXS = UIFonts_GetFace(S(7));
  Font faceXS = UIFonts_GetFace(S(8));
  Font faceSm = UIFonts_GetFace(S(9));
  Font faceMd = UIFonts_GetFace(S(11));
  Font faceLg = UIFonts_GetFace(S(18));
  Font iconSm = UIFonts_GetIcon(S(10));

  if (d->State->LoadedTrack != NULL) {
      UIDrawText("\uf001", iconSm, titleX + S(4), y + S( titleBgH / 2.0f - 2.5f), S(10), ColorWhite);
      char titleBuf[128];
      strncpy(titleBuf, d->State->TrackTitle, 127);
      UIDrawText(titleBuf, faceSm, titleX + S(18), y + S(5), S(9), ColorWhite);
  }

  // 3. MIDDLE AREA (Time, BPM, MT)
  float midY = y + titleBgH + S(4);
  
  // TIME
  float timeX = titleX + S(80);
  char timeLabel[16] = "REMAIN";
  if (d->State->TimeMode == 0) strcpy(timeLabel, "ELAPSED");
  
  DrawCentredText(timeLabel, faceXXS, timeX, S(60), midY, S(7), ColorShadow);
  DrawCentredText("/", faceXS, timeX + S(35), S(10), midY, S(8), ColorShadow);
  DrawCentredText("TIME", faceXXS, timeX + S(45), S(40), midY, S(7), ColorWhite);
  
  long long tMs = d->State->PositionMs;
  if (d->State->TimeMode == 1) tMs = d->State->TrackLengthMs - d->State->PositionMs;
  if (tMs < 0) tMs = 0;
  
  int mins = (int)(tMs / 60000);
  int secs = (int)((tMs % 60000) / 1000);
  int msec = (int)(tMs % 1000);
  
  char timeStr[32];
  sprintf(timeStr, "%02d:%02d", mins, secs);
  char msStr[8];
  sprintf(msStr, ".%03d", msec);
  
  UIDrawText(timeStr, faceLg, timeX, midY + S(8), S(18), ColorWhite);
  UIDrawText(msStr, faceMd, timeX + S(54), midY + S(14), S(11), ColorShadow);

  // MT & VINYL
  float mtX = titleX;
  float mtY = midY + S(26);
  float mtW = S(40);
  float mtH = S(9);
  
  DrawRectangleLinesEx((Rectangle){mtX, mtY, mtW, mtH}, 1, d->State->MasterTempo ? ColorOrange : ColorDark1);
  DrawCentredText("MT", faceXXS, mtX, mtW, mtY + S(1), S(7), d->State->MasterTempo ? ColorOrange : ColorShadow);
  
  float vnX = mtX + mtW + S(4);
  DrawRectangle(vnX, mtY, mtW, mtH, d->State->VinylModeEnabled ? ColorBlue : ColorDark2);
  DrawCentredText("VINYL", faceXXS, vnX, mtW, mtY + S(1), S(7), d->State->VinylModeEnabled ? ColorWhite : ColorShadow);

  // BPM & TEMPO
  float bpmBoxW = S(50);
  float bpmX = x + stripW - bpmBoxW - S(4);
  float tempoX = bpmX - S(64);
  
  DrawCentredText("TEMPO", faceXXS, tempoX, S(60), midY, S(7), ColorShadow);
  
  char tempoRangeStr[16] = "6%";
  if (d->State->TempoRange == 1) strcpy(tempoRangeStr, "10%");
  if (d->State->TempoRange == 2) strcpy(tempoRangeStr, "16%");
  if (d->State->TempoRange == 3) strcpy(tempoRangeStr, "WIDE");
  
  DrawRectangle(bpmX - S(38), midY - S(2), S(34), S(9), ColorOrange);
  DrawCentredText(tempoRangeStr, faceXXS, bpmX - S(38), S(34), midY - S(1), S(7), ColorBlack);
  
  char tempoStr[16];
  sprintf(tempoStr, "%.2f%%", d->State->TempoPercent);
  UIDrawText(tempoStr, faceMd, tempoX + S(28), midY + S(10), S(12), ColorWhite);
  
  // BPM BOX
  float bpmBoxH = S(28);
  DrawRectangle(bpmX, midY, bpmBoxW, bpmBoxH, (Color){15, 15, 15, 255});
  DrawRectangleLinesEx((Rectangle){bpmX, midY, bpmBoxW, bpmBoxH}, 1, ColorOrange);
  DrawCentredText("BPM", faceXXS, bpmX, S(20), midY + S(2), S(7), ColorOrange);
  if (d->State->IsMaster) {
      DrawRectangle(bpmX + bpmBoxW - S(32), midY + S(2), S(30), S(7), ColorOrange);
      DrawCentredText("MASTER", faceXXS, bpmX + bpmBoxW - S(32), S(30), midY + S(2), S(7), ColorBlack);
  }
  
  char bpmVal[16];
  sprintf(bpmVal, "%.1f", d->State->CurrentBPM);
  DrawCentredText(bpmVal, faceLg, bpmX, bpmBoxW, midY + S(8), S(18), ColorOrange);

  // SYNC Button Area
  float syncY = midY + bpmBoxH + S(2);
  float syncH = S(12);
  const char* syncText = "SYNC";
  Color syncColor = ColorDark1;
  
  if (d->State->SyncMode == 1) {
    syncColor = ColorWhite;
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
