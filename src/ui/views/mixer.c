#include "mixer.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int Mixer_Update(Component *base) {
  (void)base;
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
    *val -= (delta.y / S(110.0f)) * range;
    if (*val < min)
      *val = min;
    if (*val > max)
      *val = max;
  }
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f && hovered) {
    float range = max - min;
    *val += (wheel * 0.05f) * range;
    if (*val < min)
      *val = min;
    if (*val > max)
      *val = max;
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

  // --- MAIN PANEL ---
  float panelW = S(640);
  float panelH = viewH - TOP_BAR_H - S(10);
  float panelX = (SCREEN_WIDTH - panelW) / 2.0f;
  float panelY = TOP_BAR_H + S(5);

  DrawRectangle(panelX, panelY, panelW, panelH, ColorDark3);
  DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 1.0f,
                       ColorGray);

  float chW = S(210);
  float centerW = panelW - (chW * 2);
  float ch1X = panelX;
  float centerX = panelX + chW;
  float ch2X = centerX + centerW;

  Font fSub = UIFonts_GetFace(S(9));
  Font fTiny = UIFonts_GetFace(S(7));

  // --- CHANNELS ---
  for (int i = 0; i < 2; i++) {
    DeckAudioState *d = &eng->Decks[i];
    float x = (i == 0) ? ch1X : ch2X;

    DrawRectangle(x + S(2), panelY + S(2), chW - S(4), panelH - S(4),
                  (Color){20, 20, 20, 255});
    DrawRectangleLinesEx(
        (Rectangle){x + S(2), panelY + S(2), chW - S(4), panelH - S(4)}, 1,
        ColorDark1);

    float eqW = chW * 0.45f;
    float rColW = chW - eqW - S(8);
    float eqCx = x + eqW / 2.0f + S(4);

    float ky = panelY + S(28);
    float kStep = S(48);
    float kR = S(12);

    // UIDrawText(i == 0 ? "CH 1" : "CH 2", fSub, eqCx - S(10), ky - S(18),
    // S(9), ColorOrange);

    Mixer_DrawKnob(eqCx, ky, kR, d->Trim, 0.0f, 2.0f, "TRIM",
                   (Color){0, 150, 255, 255}, true);
    HandleKnob(&d->Trim, eqCx, ky, kR, 0.0f, 2.0f, true, mousePos, mDown);

    ky += kStep;
    Mixer_DrawKnob(eqCx, ky, kR, d->EqHigh, 0.0f, 1.0f, "HIGH",
                   (Color){0, 150, 255, 255}, true);
    HandleKnob(&d->EqHigh, eqCx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
    ky += kStep;
    Mixer_DrawKnob(eqCx, ky, kR, d->EqMid, 0.0f, 1.0f, "MID",
                   (Color){0, 150, 255, 255}, true);
    HandleKnob(&d->EqMid, eqCx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);
    ky += kStep;
    Mixer_DrawKnob(eqCx, ky, kR, d->EqLow, 0.0f, 1.0f, "LOW",
                   (Color){0, 150, 255, 255}, true);
    HandleKnob(&d->EqLow, eqCx, ky, kR, 0.0f, 1.0f, true, mousePos, mDown);

    // FADER CLUSTER
    float rColX = x + eqW + S(4);
    float colorY = panelY + S(28);
    float colorCx = rColX + rColW / 2.0f;

    Mixer_DrawKnob(colorCx, colorY, S(16), d->ColorFX.colorValue, -1.0f, 1.0f,
                   "COLOR", ColorOrange, true);
    HandleKnob(&d->ColorFX.colorValue, colorCx, colorY, S(16), -1.0f, 1.0f,
               true, mousePos, mDown);

    float controlsY = colorY + S(48);
    float controlsH = panelH - (controlsY - panelY) - S(45);
    float vuW = S(11);
    float faderW = rColW - vuW - S(10);
    float faderX = rColX + S(4);
    float vuX = faderX + faderW + S(2);

    DrawVertVU(vuX, controlsY, vuW, controlsH, fmaxf(d->VuMeterL, d->VuMeterR));
    HandleVerticalFader(&d->Fader, faderX, controlsY, faderW, controlsH,
                        mousePos, mDown);
    DrawVerticalFader(faderX, controlsY, faderW, controlsH, d->Fader,
                      d->IsCueActive);

    float cueY = controlsY + controlsH + S(8);
    if (DrawFXButton("CUE", rColX + S(5), cueY, rColW - S(10), S(22),
                     d->IsCueActive))
      d->IsCueActive = !d->IsCueActive;
  }

  // --- CENTER: FX GLOBAL (2 Column Layout) ---
  float colW = centerW / 2.0f;
  float leftX = centerX;
  float rightX = centerX + colW;
  float fxY = panelY + S(12);

  // LEFT COLUMN: SOUND COLOR FX
  DrawCentredText("SOUND COLOR FX", fTiny, leftX, colW, fxY, S(8), ColorShadow);
  float cfy = fxY + S(14);
  char *cfxNames[] = {"SPACE", "DUB ECHO", "SWEEP", "NOISE", "CRUSH", "FILTER"};
  ColorFXType cfxTypes[] = {COLORFX_SPACE, COLORFX_DUBECHO, COLORFX_SWEEP, COLORFX_NOISE, COLORFX_CRUSH, COLORFX_FILTER};
  float btnW = colW - S(16);
  float btnH = S(16);
  for (int i = 0; i < 6; i++) {
    float bx = leftX + S(8);
    float by = cfy + i * (btnH + S(4));
    if (DrawFXButton(cfxNames[i], bx, by, btnW, btnH, eng->Decks[0].ColorFX.activeFX == cfxTypes[i])) {
      if (eng->Decks[0].ColorFX.activeFX == cfxTypes[i]) {
        ColorFXManager_SetFX(&eng->Decks[0].ColorFX, COLORFX_NONE);
        ColorFXManager_SetFX(&eng->Decks[1].ColorFX, COLORFX_NONE);
      } else {
        ColorFXManager_SetFX(&eng->Decks[0].ColorFX, cfxTypes[i]);
        ColorFXManager_SetFX(&eng->Decks[1].ColorFX, cfxTypes[i]);
      }
    }
  }
  // Param knob aligned to bottom, same level as Depth knob
  float fxBtnH = S(32);
  float fxBtnY = panelY + panelH - fxBtnH - S(15);
  float depthKnobY = fxBtnY - S(45);
  float paramY = depthKnobY;

  Mixer_DrawKnob(leftX + colW / 2.0f, paramY, S(13), eng->Decks[0].ColorFX.parameter, 0.0f, 1.0f, "PARAM", ColorShadow, true);
  if (CheckCollisionPointCircle(mousePos, (Vector2){leftX + colW / 2.0f, paramY}, S(20))) {
    HandleKnob(&eng->Decks[0].ColorFX.parameter, leftX + colW / 2.0f, paramY, S(13), 0.0f, 1.0f, true, mousePos, mDown);
    eng->Decks[1].ColorFX.parameter = eng->Decks[0].ColorFX.parameter; // Sync
  }

  // RIGHT COLUMN: BEAT FX
  DrawCentredText("BEAT FX", fTiny, rightX, colW, fxY, S(8), ColorShadow);
  float bfy = fxY + S(14);
  const char *bfxNames[] = {"DELAY",    "ECHO",   "P-PONG",  "SPIRAL", "REVERB",
                            "TRANS",    "FILTER", "FLANGER", "PHASER", "PITCH",
                            "SLIPROLL", "ROLL",   "BRAKE",   "HELIX"};
  float bItemW = colW - S(16);
  
  // FX Selector
  DrawRectangle(rightX + S(8), bfy, bItemW, S(18), ColorBlack);
  DrawRectangleLinesEx((Rectangle){rightX + S(8), bfy, bItemW, S(18)}, 1.0f, ColorDark1);
  DrawCentredText(bfxNames[eng->BeatFX.activeFX % 14], fSub, rightX, colW, bfy + S(4), S(9), ColorWhite);
  if (CheckCollisionPointRec(mousePos, (Rectangle){rightX + S(8), bfy, bItemW, S(18)}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    BeatFXManager_SetFX(&eng->BeatFX, (eng->BeatFX.activeFX + 1) % 14);
  }
  bfy += S(22);

  // Target Select
  const char *targetNames[] = {"MASTER", "CH 1", "CH 2"};
  if (DrawFXButton(targetNames[eng->BeatFX.targetChannel], rightX + S(8), bfy, bItemW, S(18), false)) {
    eng->BeatFX.targetChannel = (eng->BeatFX.targetChannel + 1) % 3;
  }
  bfy += S(35);

  // Beat FX Controls pushed to bottom
  fxBtnH = S(32);
  fxBtnY = panelY + panelH - fxBtnH - S(15);
  depthKnobY = fxBtnY - S(45);

  // Depth Knob
  Mixer_DrawKnob(rightX + colW / 2.0f, depthKnobY, S(13), eng->BeatFX.levelDepth, 0.0f, 1.0f, "DEPTH", ColorOrange, false);
  HandleKnob(&eng->BeatFX.levelDepth, rightX + colW / 2.0f, depthKnobY, S(13), 0.0f, 1.0f, false, mousePos, mDown);

  // On/Off Button
  if (DrawFXButton(eng->BeatFX.isFxOn ? "ON" : "OFF", rightX + S(12), fxBtnY, colW - S(24), fxBtnH, eng->BeatFX.isFxOn)) {
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
