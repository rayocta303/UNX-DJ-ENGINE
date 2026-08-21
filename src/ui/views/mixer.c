#include "mixer.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "input/input.h"
#include "ui/player/player_state.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int Mixer_Update(Component *base) {
  MixerRenderer *r = (MixerRenderer *)base;
  (void)r;
  return 0;
}

// Helper for vertical fader
static void HandleVerticalFader(MixerState *state, float *val, float fx, float fy, float fw,
                                float fh, Vector2 mousePos, bool mDown) {
  float handleH = S(22);
  float travelRange = fh - handleH;
  bool hovered = CheckCollisionPointRec(mousePos, (Rectangle){fx, fy, fw, fh});

  // Start drag
  if (mDown && hovered && state->ActiveHandle == NULL) {
    state->ActiveHandle = val;
  }

  // Update if this is the active handle
  if (mDown && state->ActiveHandle == val) {
    float relY = (mousePos.y - (fy + handleH / 2.0f));
    *val = 1.0f - (relY / travelRange);
    if (*val < 0.0f)
      *val = 0.0f;
    if (*val > 1.0f)
      *val = 1.0f;
  }

  float wheel = Mouse_GetWheel();
  if (hovered && wheel != 0 && (state->ActiveHandle == NULL || state->ActiveHandle == val)) {
    *val += wheel * 0.05f;
    if (*val < 0.0f) *val = 0.0f;
    if (*val > 1.0f) *val = 1.0f;
  }
}

/*
static void DrawVerticalFader(float x, float y, float w, float h, float val,
                              bool cueActive) {
  float handleH = S(22);
  float handleW = w * 0.75f;
  float travelRange = h - handleH;

  DrawRectangleRec((Rectangle){x, y, w, h}, Theme.BgMain);
  DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, Theme.BorderDefault);

  Rectangle track = {x + w * 0.45f, y + handleH / 2.0f, w * 0.1f, travelRange};
  DrawRectangleRec(track, Theme.BgMain);

  for (int i = 0; i <= 10; i++) {
    float ty = (y + handleH / 2.0f) + (i / 10.0f) * travelRange;
    float tw = (i % 5 == 0) ? w * 0.25f : w * 0.12f;
    DrawLine(x + w * 0.5f - tw, ty, x + w * 0.5f + tw, ty, Theme.BgPanel);
  }

  float hy = (y + handleH / 2.0f) + (1.0f - val) * travelRange - handleH / 2.0f;
  Rectangle handle = {x + (w - handleW) / 2.0f, hy, handleW, handleH};

  DrawRectangle(handle.x + S(2), handle.y + S(2), handle.width, handle.height,
                Theme.BgOverlay);
  DrawRectangleRec(handle, Theme.BorderDefault);
  DrawRectangleLinesEx(handle, 1.5f, Theme.TextPrimary);
  DrawLine(handle.x + S(4), handle.y + handleH / 2.0f,
           handle.x + handleW - S(4), handle.y + handleH / 2.0f, Theme.TextPrimary);

  if (cueActive) {
    DrawRectangleRec((Rectangle){handle.x + S(2), handle.y + S(2),
                                 handleW - S(4), handleH - S(4)},
                     Fade(Theme.AccentOrange, 0.4f));
  }
}
*/

static bool DrawFXButton(const char *label, float x, float y, float w, float h,
                         bool active) {
  bool hovered =
      CheckCollisionPointRec(Input_GetPointerPos(), (Rectangle){x, y, w, h});
  bool pressed = Touch_CheckClick((Rectangle){x, y, w, h}, S(2.0f));
  bool isFxBlinking = (fmod(GetTime(), 0.5) < 0.25);
  Color bg = active ? (isFxBlinking ? Theme.AccentBlue : Theme.HoverActive) : (hovered ? Theme.BorderDefault : Theme.BgMain);
  Color fg = active ? Theme.TextPrimary : Theme.AccentOrange;
  DrawRectangle(x, y, w, h, bg);
  DrawRectangleLines(x, y, w, h, active ? Theme.TextPrimary : Theme.AccentOrange);
  int fontSize = (h > S(18)) ? S(9.5f) : S(8);
  Font f = UIFonts_GetFace(fontSize);
  DrawCentredText(label, f, x, w, y + (h - fontSize) / 2.0f, fontSize, fg);
  return pressed;
}

// Improved local knob drawer with percentage
static void Mixer_DrawKnob(float x, float y, float radius, float value,
                           float min, float max, const char *label, Color color,
                           bool centerZero) {
  UIDrawKnob(x, y, radius, value, min, max, label, color, centerZero);
}

static void HandleKnob(MixerState *state, float *val, float cx, float cy, float r, float min,
                       float max, bool centerZero, Vector2 mousePos,
                       bool mDown) {
  bool hovered =
      CheckCollisionPointCircle(mousePos, (Vector2){cx, cy}, r + S(12));
  
  // Start drag
  if (mDown && hovered && state->ActiveHandle == NULL) {
    state->ActiveHandle = val;
  }

  // Update if this is the active handle
  if (mDown && state->ActiveHandle == val) {
    Vector2 delta = Mouse_GetDelta();
    float range = max - min;
    float center = min + range / 2.0f;
    float oldVal = *val;
    *val -= (delta.y / S(110.0f)) * range;
    
    // Center Snapping
    if (centerZero) {
        float snapThreshold = range * 0.02f; // 2% threshold
        if (oldVal > center && *val <= center + snapThreshold && *val >= center - snapThreshold) {
            *val = center;
        } else if (oldVal < center && *val >= center - snapThreshold && *val <= center + snapThreshold) {
            *val = center;
        }
    }

    if (*val < min) *val = min;
    if (*val > max) *val = max;
  }

  float wheel = Mouse_GetWheel();
  if (wheel != 0.0f && hovered && (state->ActiveHandle == NULL || state->ActiveHandle == val)) {
    float range = max - min;
    float center = min + range / 2.0f;
    *val += (wheel * 0.05f) * range;
    
    // Snap on wheel too
    if (centerZero) {
        float snapThreshold = range * 0.03f;
        if (fabsf(*val - center) < snapThreshold) *val = center;
    }

    if (*val < min) *val = min;
    if (*val > max) *val = max;
  }

  if (Input_IsPressed() && hovered) {
    static float lastClickTime = 0;
    if (GetTime() - lastClickTime < 0.3) {
      *val = centerZero ? (min + (max - min) / 2.0f) : min;
    }
    lastClickTime = GetTime();
  }
}

static void DrawVertVU(float vx, float vy, float vw, float vh, float level) {
  DrawRectangle(vx, vy, vw, vh, Theme.BgMain);
  DrawRectangleLinesEx((Rectangle){vx, vy, vw, vh}, 1.0f, Theme.BorderDefault);
  int segs = 18;
  float segH = (vh - 2) / segs;
  for (int i = 0; i < segs; i++) {
    float th = (float)(i + 1) / segs;
    Color c = (i < 12) ? Theme.AccentGreen : (i < 16 ? Theme.AccentOrange : Theme.AccentRed);
    if (level < th)
      c = Fade(c, 0.12f);
    DrawRectangle(vx + 2, vy + vh - (i + 1) * segH - 1, vw - 4, segH - 1, c);
  }
}

static void Mixer_Draw(Component *base) {
  MixerRenderer *r = (MixerRenderer *)base;
  if (!r->State->IsActive || r->State->AudioPlugin == NULL)
    return;

  AudioEngine *eng = r->State->AudioPlugin;
  Vector2 mousePos = Input_GetPointerPos();
  bool mDown = Input_IsDown();

  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  DrawRectangle(0, 0, SCREEN_WIDTH, viewH, Theme.BgMain);

  // Clear handle if mouse released
  if (!mDown) r->State->ActiveHandle = NULL;

  DrawRectangle(0, 0, SCREEN_WIDTH, TOP_BAR_H, Theme.BorderDefault);
  DrawLine(0, TOP_BAR_H, SCREEN_WIDTH, TOP_BAR_H, Theme.BorderDefault);
  UIDrawText("MIXER", UIFonts_GetFace(S(12)), S(15), S(5), S(12), Theme.TextPrimary);

  // --- LAYOUT CONSTANTS ---
  float colFXW = S(124);
  float colMixW = S(330);
  float colRightW = S(140);
  
  float panelY = TOP_BAR_H + S(5);
  float panelH = viewH - TOP_BAR_H - S(10);

  float leftX = S(6);                                // Pepet kiri layar
  float rightX = SCREEN_WIDTH - colRightW - S(6);     // Pepet kanan layar
  float centerX = (SCREEN_WIDTH - colMixW) / 2.0f;    // Centered core mixer

  Font fSub = UIFonts_GetFace(S(9));
  Font fTiny = UIFonts_GetFace(S(7));

  // =========================================================================
  // COLUMN 1: SOUND COLOR FX (FAR LEFT)
  // =========================================================================
  DrawRectangle(leftX, panelY, colFXW, panelH, Theme.BgPanel);
  DrawRectangleLinesEx((Rectangle){leftX, panelY, colFXW, panelH}, 1.0f, Theme.BorderDefault);
  float fxY = panelY + S(12);
  DrawCentredText("SOUND COLOR FX", fSub, leftX, colFXW, fxY, S(9), Theme.BorderDefault);
  
  float cfy = fxY + S(14);
  char *cfxNames[] = {"SPACE", "DUB ECHO", "SWEEP", "NOISE", "FILTER", "JET"};
  ColorFXType cfxTypes[] = {COLORFX_SPACE, COLORFX_DUBECHO, COLORFX_SWEEP, COLORFX_NOISE, COLORFX_FILTER, COLORFX_JET};
  float cbtnW = colFXW - S(16);
  float cbtnH = S(20);
  float cbtnGap = S(4);
  for (int i = 0; i < 6; i++) {
    float bx = leftX + S(8);
    float by = cfy + i * (cbtnH + cbtnGap);
    if (DrawFXButton(cfxNames[i], bx, by, cbtnW, cbtnH, eng->Decks[0].ColorFX.activeFX == cfxTypes[i])) {
      ColorFXType next = (eng->Decks[0].ColorFX.activeFX == cfxTypes[i]) ? COLORFX_NONE : cfxTypes[i];
      ColorFXManager_SetFX(&eng->Decks[0].ColorFX, next);
      ColorFXManager_SetFX(&eng->Decks[1].ColorFX, next);
    }
  }

  float paramY = panelY + panelH - S(50);
  Mixer_DrawKnob(leftX + colFXW/2, paramY, S(12), eng->Decks[0].ColorFX.parameter, 0.0f, 1.0f, "PARAM", Theme.BorderDefault, true);
  HandleKnob(r->State, &eng->Decks[0].ColorFX.parameter, leftX + colFXW/2, paramY, S(12), 0.0f, 1.0f, true, mousePos, mDown);
  eng->Decks[1].ColorFX.parameter = eng->Decks[0].ColorFX.parameter;

  // =========================================================================
  // COLUMN 2: CORE MIXER (CENTER) - [Fader1 | EQ1 | VU | EQ2 | Fader2]
  // =========================================================================
  DrawRectangle(centerX, panelY, colMixW, panelH, Theme.BgPanelAlt);
  DrawRectangleLinesEx((Rectangle){centerX, panelY, colMixW, panelH}, 1.0f, Theme.BorderDefault);
  
  float innerPad = S(10);
  float mixerInnerW = colMixW - (innerPad * 2);
  float slotW = mixerInnerW / 4.0f;
  
  float fader1X = centerX + innerPad;
  float eq1X = fader1X + slotW;
  float eq2X = eq1X + slotW;
  float fader2X = eq2X + slotW;
  
  float mVuX = centerX + colMixW / 2.0f;
  float kStep = S(54); 
  float kR = S(10);
  float fH = S(160); // Reduced to accommodate Color knob below

  for (int i = 0; i < 2; i++) {
    DeckAudioState *d = &eng->Decks[i];
    float fX = (i == 0) ? fader1X : fader2X;
    float eX = (i == 0) ? eq1X : eq2X;
    float ecx = eX + slotW / 2.0f;
    float fcx = fX + slotW / 2.0f;

    // --- TOP BAR (Cue & FX) ---
    float topY = panelY + S(5);
    
    // Square Cue Button
    float cueBtnSize = S(20);
    float cueBtnX = fcx - cueBtnSize / 2.0f;
    float cueBtnY = topY - S(2);
    DrawRectangleLinesEx((Rectangle){cueBtnX, cueBtnY, cueBtnSize, cueBtnSize}, 1.0f, d->IsCueActive ? Theme.AccentOrange : Theme.BorderDefault);
    if (d->IsCueActive) DrawRectangleRec((Rectangle){cueBtnX + 2, cueBtnY + 2, cueBtnSize - 4, cueBtnSize - 4}, Fade(Theme.CueMarker, 0.15f));

    Font iconFont = UIFonts_GetIcon(S(12));
    UIDrawText("\xef\x80\xa5", iconFont, fcx - S(6), cueBtnY + S(4), S(12), d->IsCueActive ? Theme.AccentOrange : Theme.BorderDefault);
    if (Touch_CheckClick((Rectangle){cueBtnX, cueBtnY, cueBtnSize, cueBtnSize}, S(2.0f))) {
        d->IsCueActive = !d->IsCueActive;
    }

    // Channel Label
    UIDrawText(i == 0 ? "CH1" : "CH2", fTiny, ecx - S(10), topY + S(2), S(9), Theme.TextPrimary);

    // --- EQ STACK (Inner) ---
    float ky = panelY + S(38); // Lowered slightly
    Mixer_DrawKnob(ecx, ky, kR, d->Trim, 0.0f, 1.0f, "TRIM", Theme.TextPrimary, true);
    HandleKnob(r->State, &d->Trim, ecx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
    ky += kStep;
    Mixer_DrawKnob(ecx, ky, kR, d->EqHigh, 0.0f, 1.0f, "HI", Theme.TextPrimary, true);
    HandleKnob(r->State, &d->EqHigh, ecx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
    ky += kStep;
    Mixer_DrawKnob(ecx, ky, kR, d->EqMid, 0.0f, 1.0f, "MID", Theme.TextPrimary, true);
    HandleKnob(r->State, &d->EqMid, ecx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
    ky += kStep;
    Mixer_DrawKnob(ecx, ky, kR, d->EqLow, 0.0f, 1.0f, "LOW", Theme.TextPrimary, true);
    HandleKnob(r->State, &d->EqLow, ecx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);

    // --- FADER (Outer) ---
    float fy = panelY + S(38); // Align fader start with Trim
    float fW = S(22);
    HandleVerticalFader(r->State, &d->Fader, fcx - fW/2, fy, fW, fH, mousePos, mDown);
    
    // Custom Fader Draw to match image
    Color faderCol = (i == 0) ? Theme.AccentBlue : Theme.AccentBlue;
    DrawRectangle(fcx - 1, fy, 2, fH, Theme.BgMain);
    float valH = d->Fader * fH;
    DrawRectangle(fcx - 1, fy + fH - valH, 2, valH, faderCol);
    float handleY = fy + (1.0f - d->Fader) * (fH - S(10));
    DrawRectangle(fcx - fW/2, handleY, fW, S(10), Theme.BorderDefault);
    DrawRectangleLines(fcx - fW/2, handleY, fW, S(10), Theme.TextSecondary);
    DrawLine(fcx - fW/2 + 2, handleY + S(5), fcx + fW/2 - 2, handleY + S(5), Theme.TextPrimary);

    // --- CHANNEL VU ---
    float cvuX = (i == 0) ? (fcx + fW/2 + S(6)) : (fcx - fW/2 - S(10)); // Increased spacing
    float rawPeak = fmaxf(d->VuMeterL, d->VuMeterR);
    float chanVu = (d->Fader > 0.01f) ? (rawPeak * d->Fader) : (d->IsCueActive ? rawPeak : rawPeak * d->Fader);
    DrawVertVU(cvuX, fy, S(4), fH, chanVu);

    // --- CFX (COLOR) KNOB (Below Fader) ---
    float colorY = fy + fH + S(22);
    Mixer_DrawKnob(fcx, colorY, S(12), d->ColorFX.colorValue, -1.0f, 1.0f, "COLOR", Theme.AccentOrange, true);
    HandleKnob(r->State, &d->ColorFX.colorValue, fcx, colorY, S(12), -1.0f, 1.0f, true, mousePos, mDown);
  }

  // Master VU (Squeezed in the very middle)
  float mVuH = fH;
  DrawVertVU(mVuX - S(5), panelY + S(38), S(4), mVuH, eng->MasterVuL);
  DrawVertVU(mVuX + S(1), panelY + S(38), S(4), mVuH, eng->MasterVuR);


  // Crossfader
  float cfW = S(130); // Shortened to 1/2 size
  float cfH = S(18);
  float cfX = centerX + (colMixW - cfW) / 2.0f;
  float cfY = panelY + panelH - cfH - S(10);
  DrawRectangleRounded((Rectangle){cfX, cfY + cfH/2 - 2, cfW, 4}, 1.0f, 4, Theme.BgMain);
  float hX = cfX + (eng->Crossfader + 1.0f) * 0.5f * (cfW - S(12));
  DrawRectangleRounded((Rectangle){hX, cfY, S(12), cfH}, 0.2f, 4, Theme.BorderDefault);
  DrawLine(hX + S(6), cfY + 2, hX + S(6), cfY + cfH - 2, Theme.TextPrimary);
  
  if (mDown && CheckCollisionPointRec(mousePos, (Rectangle){cfX, cfY, cfW, cfH}) && r->State->ActiveHandle == NULL) {
      r->State->ActiveHandle = &eng->Crossfader;
  }
  if (mDown && r->State->ActiveHandle == &eng->Crossfader) {
      eng->Crossfader = ((mousePos.x - cfX) / cfW) * 2.0f - 1.0f;
      if (eng->Crossfader < -1.0f) eng->Crossfader = -1.0f;
      if (eng->Crossfader > 1.0f) eng->Crossfader = 1.0f;
  }


  // =========================================================================
  // COLUMN 3: BEAT FX (FAR RIGHT)
  // =========================================================================
  BeatFXState *fxs = r->State->FXState;
  DrawRectangle(rightX, panelY, colRightW, panelH, Theme.BgPanel);
  DrawRectangleLinesEx((Rectangle){rightX, panelY, colRightW, panelH}, 1.0f, Theme.BorderDefault);
  float masterKnobY = panelY + S(25);
  Mixer_DrawKnob(rightX + colRightW / 2.0f, masterKnobY, S(15), eng->MasterVolume, 0.0f, 2.0f, "MASTER", Theme.AccentRed, true);
  HandleKnob(r->State, &eng->MasterVolume, rightX + colRightW / 2.0f, masterKnobY, S(15), 0.0f, 2.0f, true, mousePos, mDown);

  float bfxY = masterKnobY + S(40);
  DrawCentredText("BEAT FX", fSub, rightX, colRightW, bfxY, S(9), Theme.BorderDefault);
  
  const char *bfxNames[] = {"DELAY", "ECHO", "P-PONG", "SPIRAL", "REVERB", "TRANS", "FILTER", "FLANGER", "PHASER", "PITCH", "SLIPROLL", "ROLL", "BRAKE", "HELIX"};
  float bSelectorY = bfxY + S(15);
  Rectangle fxSelRect = {rightX + S(10), bSelectorY, colRightW - S(20), S(26)};
  DrawRectangleRec(fxSelRect, Theme.BgMain);
  DrawRectangleLinesEx(fxSelRect, 1.0f, Theme.TextPrimary);
  int focus = fxs->FocusedSlot;
  DrawCentredText(bfxNames[fxs->Slots[focus].FXType % 14], fSub, rightX, colRightW, bSelectorY + S(8), S(9), Theme.TextPrimary);
  if (Touch_CheckClick(fxSelRect, S(2.0f))) {
      fxs->Slots[focus].FXType = (fxs->Slots[focus].FXType + 1) % 14;
      BeatFXManager_SetFX(&eng->BeatFX, fxs->Slots[focus].FXType);
  }

  float targetY = bSelectorY + S(32);
  const char *targetNames[] = {"MASTER", "CH 1", "CH 2"};
  Rectangle chSelRect = {rightX + S(10), targetY, colRightW - S(20), S(26)};
  DrawRectangleRec(chSelRect, Theme.BgMain);
  DrawRectangleLinesEx(chSelRect, 1.0f, Theme.TextPrimary);
  DrawCentredText(targetNames[fxs->SelectedChannel % 3], fSub, rightX, colRightW, targetY + S(8), S(9), Theme.AccentOrange);
  if (Touch_CheckClick(chSelRect, S(2.0f))) {
      fxs->SelectedChannel = (fxs->SelectedChannel + 1) % 3;
  }

  float bDepthY = panelY + panelH - S(100);
  Mixer_DrawKnob(rightX + colRightW / 2.0f, bDepthY, S(13), fxs->LevelDepth, 0.0f, 1.0f, "DEPTH", Theme.AccentOrange, false);
  HandleKnob(r->State, &fxs->LevelDepth, rightX + colRightW / 2.0f, bDepthY, S(13), 0.0f, 1.0f, false, mousePos, mDown);

  float bOnOffY = panelY + panelH - S(45);
  if (DrawFXButton(fxs->Slots[focus].IsOn ? "ON" : "OFF", rightX + S(15), bOnOffY, colRightW - S(30), S(30), fxs->Slots[focus].IsOn)) {
      fxs->Slots[focus].IsOn = !fxs->Slots[focus].IsOn;
      BeatFXManager_SetFXOn(&eng->BeatFX, fxs->Slots[focus].IsOn);
  }
}


void MixerRenderer_Init(MixerRenderer *r, MixerState *state) {
  if (!r)
    return;
  memset(r, 0, sizeof(MixerRenderer));
  r->State = state;
  r->base.Draw = Mixer_Draw;
  r->base.Update = Mixer_Update;
}
