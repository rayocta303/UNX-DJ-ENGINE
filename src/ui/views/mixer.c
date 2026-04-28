#include "mixer.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/player_state.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int Mixer_Update(Component *base) {
  MixerRenderer *r = (MixerRenderer *)base;
  if (r->State->AudioPlugin && r->State->FXState) {
    AudioEngine *eng = r->State->AudioPlugin;
    BeatFXState *fx = r->State->FXState;
    // Pull any changes from engine (e.g. MIDI) or push UI changes
    // Here we ensure the UI state matches the engine
    fx->LevelDepth = eng->BeatFX.levelDepth;
    fx->SelectedChannel = eng->BeatFX.targetChannel;
    fx->SelectedFX = eng->BeatFX.activeFX;
    fx->IsFXOn = eng->BeatFX.isFxOn;
  }
  return 0;
}

// Helper for vertical fader
static void HandleVerticalFader(float *val, float fx, float fy, float fw,
                                float fh, Vector2 mousePos, bool mDown) {
  float handleH = S(22);
  float travelRange = fh - handleH;
  bool hovered = CheckCollisionPointRec(mousePos, (Rectangle){fx, fy, fw, fh});

  if (mDown && hovered) {
    float relY = (mousePos.y - (fy + handleH / 2.0f));
    *val = 1.0f - (relY / travelRange);
    if (*val < 0.0f)
      *val = 0.0f;
    if (*val > 1.0f)
      *val = 1.0f;
  }

  float wheel = GetMouseWheelMove();
  if (hovered && wheel != 0) {
    *val += wheel * 0.05f;
    if (*val < 0.0f) *val = 0.0f;
    if (*val > 1.0f) *val = 1.0f;
  }
}

static void DrawVerticalFader(float x, float y, float w, float h, float val,
                              bool cueActive) {
  float handleH = S(22);
  float handleW = w * 0.75f;
  float travelRange = h - handleH;

  DrawRectangleRec((Rectangle){x, y, w, h}, (Color){10, 10, 10, 255});
  DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, ColorDark1);

  Rectangle track = {x + w * 0.45f, y + handleH / 2.0f, w * 0.1f, travelRange};
  DrawRectangleRec(track, ColorBlack);

  for (int i = 0; i <= 10; i++) {
    float ty = (y + handleH / 2.0f) + (i / 10.0f) * travelRange;
    float tw = (i % 5 == 0) ? w * 0.25f : w * 0.12f;
    DrawLine(x + w * 0.5f - tw, ty, x + w * 0.5f + tw, ty, ColorDark2);
  }

  float hy = (y + handleH / 2.0f) + (1.0f - val) * travelRange - handleH / 2.0f;
  Rectangle handle = {x + (w - handleW) / 2.0f, hy, handleW, handleH};

  DrawRectangle(handle.x + S(2), handle.y + S(2), handle.width, handle.height,
                (Color){0, 0, 0, 150});
  DrawRectangleRec(handle, (Color){45, 45, 45, 255});
  DrawRectangleLinesEx(handle, 1.5f, ColorWhite);
  DrawLine(handle.x + S(4), handle.y + handleH / 2.0f,
           handle.x + handleW - S(4), handle.y + handleH / 2.0f, ColorWhite);

  if (cueActive) {
    DrawRectangleRec((Rectangle){handle.x + S(2), handle.y + S(2),
                                 handleW - S(4), handleH - S(4)},
                     Fade(ColorOrange, 0.4f));
  }
}

static bool DrawFXButton(const char *label, float x, float y, float w, float h,
                         bool active) {
  bool hovered =
      CheckCollisionPointRec(UIGetMousePosition(), (Rectangle){x, y, w, h});
  bool pressed = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  Color bg = active ? ColorOrange : (hovered ? ColorDark1 : ColorBlack);
  Color fg = active ? ColorBlack : ColorOrange;
  DrawRectangle(x, y, w, h, bg);
  DrawRectangleLines(x, y, w, h, ColorOrange);
  Font f = UIFonts_GetFace(S(8));
  int fontSize = S(8);
  DrawCentredText(label, f, x, w, y + (h - fontSize) / 2.0f, fontSize, fg);
  return pressed;
}

// Improved local knob drawer with percentage
static void Mixer_DrawKnob(float x, float y, float radius, float value,
                           float min, float max, const char *label, Color color,
                           bool centerZero) {
  UIDrawKnob(x, y, radius, value, min, max, label, color, centerZero);
}

static void HandleKnob(float *val, float cx, float cy, float r, float min,
                       float max, bool centerZero, Vector2 mousePos,
                       bool mDown) {
  bool hovered =
      CheckCollisionPointCircle(mousePos, (Vector2){cx, cy}, r + S(12));
  if (mDown && hovered) {
    Vector2 delta = GetMouseDelta();
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
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f && hovered) {
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
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
      CheckCollisionPointCircle(mousePos, (Vector2){cx, cy}, r)) {
    static float lastClickTime = 0;
    if (GetTime() - lastClickTime < 0.3) {
      *val = centerZero ? (min + (max - min) / 2.0f) : min;
    }
    lastClickTime = GetTime();
  }
}

static void DrawVertVU(float vx, float vy, float vw, float vh, float level) {
  DrawRectangle(vx, vy, vw, vh, (Color){5, 5, 5, 255});
  DrawRectangleLinesEx((Rectangle){vx, vy, vw, vh}, 1.0f, ColorDark1);
  int segs = 18;
  float segH = (vh - 2) / segs;
  for (int i = 0; i < segs; i++) {
    float th = (float)(i + 1) / segs;
    Color c = (i < 12) ? ColorDGreen : (i < 16 ? ColorOrange : ColorRed);
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
  Vector2 mousePos = UIGetMousePosition();
  bool mDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  DrawRectangle(0, 0, SCREEN_WIDTH, viewH, ColorBGUtil);

  DrawRectangle(0, 0, SCREEN_WIDTH, TOP_BAR_H, ColorDark1);
  DrawLine(0, TOP_BAR_H, SCREEN_WIDTH, TOP_BAR_H, ColorShadow);
  UIDrawText("MIXER", UIFonts_GetFace(S(12)), S(15), S(5), S(12), ColorWhite);

  // --- LAYOUT CONSTANTS ---
  float colFXW = S(120);
  float colMixW = S(320);
  float colRightW = S(140);
  
  float totalW = colFXW + colMixW + colRightW;
  float panelX = (SCREEN_WIDTH - totalW) / 2.0f;
  float panelY = TOP_BAR_H + S(5);
  float panelH = viewH - TOP_BAR_H - S(10);

  DrawRectangle(panelX, panelY, totalW, panelH, ColorDark3);
  DrawRectangleLinesEx((Rectangle){panelX, panelY, totalW, panelH}, 1.0f, ColorGray);

  float leftX = panelX;
  float centerX = panelX + colFXW;
  float rightX = centerX + colMixW;

  Font fSub = UIFonts_GetFace(S(9));
  Font fTiny = UIFonts_GetFace(S(7));

  // =========================================================================
  // COLUMN 1: SOUND COLOR FX (LEFT)
  // =========================================================================
  DrawRectangle(leftX + S(2), panelY + S(2), colFXW - S(4), panelH - S(4), (Color){20, 20, 20, 255});
  float fxY = panelY + S(12);
  DrawCentredText("SOUND COLOR FX", fTiny, leftX, colFXW, fxY, S(7), ColorShadow);
  
  float cfy = fxY + S(14);
  char *cfxNames[] = {"SPACE", "DUB ECHO", "SWEEP", "NOISE", "CRUSH", "FILTER", "JET"};
  ColorFXType cfxTypes[] = {COLORFX_SPACE, COLORFX_DUBECHO, COLORFX_SWEEP, COLORFX_NOISE, COLORFX_CRUSH, COLORFX_FILTER, COLORFX_JET};
  float cbtnW = colFXW - S(16);
  float cbtnH = S(14);
  for (int i = 0; i < 7; i++) {
    float bx = leftX + S(8);
    float by = cfy + i * (cbtnH + S(4));
    if (DrawFXButton(cfxNames[i], bx, by, cbtnW, cbtnH, eng->Decks[0].ColorFX.activeFX == cfxTypes[i])) {
      ColorFXType next = (eng->Decks[0].ColorFX.activeFX == cfxTypes[i]) ? COLORFX_NONE : cfxTypes[i];
      ColorFXManager_SetFX(&eng->Decks[0].ColorFX, next);
      ColorFXManager_SetFX(&eng->Decks[1].ColorFX, next);
    }
  }

  float paramY = panelY + panelH - S(50);
  Mixer_DrawKnob(leftX + colFXW/2, paramY, S(12), eng->Decks[0].ColorFX.parameter, 0.0f, 1.0f, "PARAM", ColorShadow, true);
  HandleKnob(&eng->Decks[0].ColorFX.parameter, leftX + colFXW/2, paramY, S(12), 0.0f, 1.0f, true, mousePos, mDown);
  eng->Decks[1].ColorFX.parameter = eng->Decks[0].ColorFX.parameter;

  // =========================================================================
  // COLUMN 2: CORE MIXER (CENTER) - [Fader1 | EQ1 | VU | EQ2 | Fader2]
  // =========================================================================
  DrawRectangle(centerX + S(2), panelY + S(2), colMixW - S(4), panelH - S(4), (Color){22, 22, 22, 255});
  
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
    DrawRectangleLinesEx((Rectangle){cueBtnX, cueBtnY, cueBtnSize, cueBtnSize}, 1.0f, d->IsCueActive ? ColorOrange : ColorShadow);
    if (d->IsCueActive) DrawRectangleRec((Rectangle){cueBtnX + 2, cueBtnY + 2, cueBtnSize - 4, cueBtnSize - 4}, (Color){255, 150, 0, 40});

    Font iconFont = UIFonts_GetIcon(S(12));
    UIDrawText("\xef\x80\xa5", iconFont, fcx - S(6), cueBtnY + S(4), S(12), d->IsCueActive ? ColorOrange : ColorShadow);
    if (CheckCollisionPointRec(mousePos, (Rectangle){cueBtnX, cueBtnY, cueBtnSize, cueBtnSize}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        d->IsCueActive = !d->IsCueActive;
    }
    
    // Channel Label
    UIDrawText(i == 0 ? "CH1" : "CH2", fTiny, ecx - S(10), topY + S(2), S(9), ColorWhite);

    // --- EQ STACK (Inner) ---
    float ky = panelY + S(38); // Lowered slightly
    Mixer_DrawKnob(ecx, ky, kR, d->Trim, 0.0f, 1.0f, "TRIM", ColorWhite, false);
    HandleKnob(&d->Trim, ecx, ky, kR, 0.0f, 1.0f, false, mousePos, mDown);
    ky += kStep;
    Mixer_DrawKnob(ecx, ky, kR, d->EqHigh, 0.0f, 1.0f, "HI", ColorWhite, true);
    HandleKnob(&d->EqHigh, ecx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
    ky += kStep;
    Mixer_DrawKnob(ecx, ky, kR, d->EqMid, 0.0f, 1.0f, "MID", ColorWhite, true);
    HandleKnob(&d->EqMid, ecx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
    ky += kStep;
    Mixer_DrawKnob(ecx, ky, kR, d->EqLow, 0.0f, 1.0f, "LOW", ColorWhite, true);
    HandleKnob(&d->EqLow, ecx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);

    // --- FADER (Outer) ---
    float fy = panelY + S(38); // Align fader start with Trim
    float fW = S(22);
    HandleVerticalFader(&d->Fader, fcx - fW/2, fy, fW, fH, mousePos, mDown);
    
    // Custom Fader Draw to match image
    Color faderCol = (i == 0) ? (Color){150, 100, 255, 255} : (Color){100, 200, 255, 255};
    DrawRectangle(fcx - 1, fy, 2, fH, (Color){10, 10, 10, 255});
    float valH = d->Fader * fH;
    DrawRectangle(fcx - 1, fy + fH - valH, 2, valH, faderCol);
    float handleY = fy + (1.0f - d->Fader) * (fH - S(10));
    DrawRectangle(fcx - fW/2, handleY, fW, S(10), (Color){40, 40, 40, 255});
    DrawRectangleLines(fcx - fW/2, handleY, fW, S(10), ColorGray);
    DrawLine(fcx - fW/2 + 2, handleY + S(5), fcx + fW/2 - 2, handleY + S(5), ColorWhite);

    // --- CHANNEL VU ---
    float cvuX = (i == 0) ? (fcx + fW/2 + S(6)) : (fcx - fW/2 - S(10)); // Increased spacing
    DrawVertVU(cvuX, fy, S(4), fH, fmaxf(d->VuMeterL, d->VuMeterR));

    // --- CFX (COLOR) KNOB (Below Fader) ---
    float colorY = fy + fH + S(22);
    Mixer_DrawKnob(fcx, colorY, S(12), d->ColorFX.colorValue, -1.0f, 1.0f, "COLOR", ColorOrange, true);
    HandleKnob(&d->ColorFX.colorValue, fcx, colorY, S(12), -1.0f, 1.0f, true, mousePos, mDown);
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
  DrawRectangleRounded((Rectangle){cfX, cfY + cfH/2 - 2, cfW, 4}, 1.0f, 4, (Color){10, 10, 10, 255});
  float hX = cfX + (eng->Crossfader + 1.0f) * 0.5f * (cfW - S(12));
  DrawRectangleRounded((Rectangle){hX, cfY, S(12), cfH}, 0.2f, 4, (Color){50, 50, 50, 255});
  DrawLine(hX + S(6), cfY + 2, hX + S(6), cfY + cfH - 2, ColorWhite);
  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, (Rectangle){cfX, cfY, cfW, cfH})) {
      eng->Crossfader = ((mousePos.x - cfX) / cfW) * 2.0f - 1.0f;
  }


  // =========================================================================
  // COLUMN 3: BEAT FX (RIGHT)
  // =========================================================================
  DrawRectangle(rightX + S(2), panelY + S(2), colRightW - S(4), panelH - S(4), (Color){20, 20, 20, 255});
  float masterKnobY = panelY + S(25);
  Mixer_DrawKnob(rightX + colRightW / 2.0f, masterKnobY, S(15), eng->MasterVolume, 0.0f, 1.0f, "MASTER", ColorRed, false);
  HandleKnob(&eng->MasterVolume, rightX + colRightW / 2.0f, masterKnobY, S(15), 0.0f, 1.0f, false, mousePos, mDown);

  float bfxY = masterKnobY + S(40);
  DrawCentredText("BEAT FX", fTiny, rightX, colRightW, bfxY, S(7), ColorShadow);
  
  const char *bfxNames[] = {"DELAY", "ECHO", "P-PONG", "SPIRAL", "REVERB", "TRANS", "FILTER", "FLANGER", "PHASER", "PITCH", "SLIPROLL", "ROLL", "BRAKE", "HELIX"};
  float bSelectorY = bfxY + S(12);
  DrawRectangle(rightX + S(10), bSelectorY, colRightW - S(20), S(18), ColorBlack);
  DrawCentredText(bfxNames[eng->BeatFX.activeFX % 14], fSub, rightX, colRightW, bSelectorY + S(4), S(9), ColorWhite);
  if (CheckCollisionPointRec(mousePos, (Rectangle){rightX + S(10), bSelectorY, colRightW - S(20), S(18)}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      BeatFXManager_SetFX(&eng->BeatFX, (eng->BeatFX.activeFX + 1) % 14);
  }

  float targetY = bSelectorY + S(24);
  const char *targetNames[] = {"MASTER", "CH 1", "CH 2"};
  if (DrawFXButton(targetNames[eng->BeatFX.targetChannel], rightX + S(10), targetY, colRightW - S(20), S(18), false)) {
      eng->BeatFX.targetChannel = (eng->BeatFX.targetChannel + 1) % 3;
  }

  float bDepthY = panelY + panelH - S(100);
  Mixer_DrawKnob(rightX + colRightW / 2.0f, bDepthY, S(13), eng->BeatFX.levelDepth, 0.0f, 1.0f, "DEPTH", ColorOrange, false);
  HandleKnob(&eng->BeatFX.levelDepth, rightX + colRightW / 2.0f, bDepthY, S(13), 0.0f, 1.0f, false, mousePos, mDown);

  float bOnOffY = panelY + panelH - S(45);
  if (DrawFXButton(eng->BeatFX.isFxOn ? "ON" : "OFF", rightX + S(15), bOnOffY, colRightW - S(30), S(30), eng->BeatFX.isFxOn)) {
      eng->BeatFX.isFxOn = !eng->BeatFX.isFxOn;
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
