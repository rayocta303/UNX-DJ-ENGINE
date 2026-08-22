#include "ui/views/pad.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "input/input.h"
#include <stdio.h>
#include <string.h>



static int Pad_Update(Component *base) {
  PadRenderer *r = (PadRenderer *)base;
  if (!r->State->IsActive)
    return 0;

  static int pressedPad = -1;
  static int pressedDeck = -1;

  Vector2 mouse = Input_GetPointerPos();
  bool isDown = Input_IsDown();
  bool isPressed = Input_IsPressed();
  bool isReleased = Input_IsReleased();

  float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
  float panelW = (SCREEN_WIDTH - S(24)) / 2.0f;
  float panelH = availableH - S(16);

  float modeBarH = S(22);
  float padAreaW = panelW - S(20);
  float padAreaH = panelH - S(40) - modeBarH;
  float padW = (padAreaW - S(12)) / 4.0f;
  float padH = (padAreaH - S(4)) / 2.0f;

  for (int d = 0; d < 2; d++) {
    float panelX = S(8) + d * (panelW + S(8));
    float panelY = TOP_BAR_H + S(8);

    // SHIFT Button hit (Touch Utility)
    Rectangle shiftBtn = { panelX + panelW - S(52), panelY + S(2), S(48), S(20) };
    if (Touch_CheckClick(shiftBtn, S(6.0f))) {
      r->State->ShiftActive[d] = !r->State->ShiftActive[d];
      return 1;
    }

    // Mode selection hits (Touch Utility)
    float modeBtnW = (panelW - S(20)) / 5.0f;
    float modeX = panelX + S(10);
    float modeY = panelY + S(28);

    bool isShiftActive = r->State->ShiftActive[d] || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    for (int m = 0; m < 5; m++) {
      Rectangle mRect = {modeX + m * modeBtnW, modeY, modeBtnW, modeBarH};
      if (Touch_CheckClick(mRect, S(4.0f))) {
        PadMode newMode;
        if (m == 0) newMode = isShiftActive ? PAD_MODE_GATE_CUE : PAD_MODE_HOT_CUE;
        else if (m == 1) newMode = isShiftActive ? PAD_MODE_SLIP_LOOP : PAD_MODE_BEAT_LOOP;
        else if (m == 2) newMode = PAD_MODE_BEAT_JUMP;
        else if (m == 3) newMode = PAD_MODE_PAD_FX;
        else newMode = PAD_MODE_SAMPLER;

        r->State->Mode[d] = newMode;
        if (r->OnModeChange)
          r->OnModeChange(r->callbackCtx, d, newMode);
        return 1;
      }
    }
  }

  if (isDown) {
    for (int d = 0; d < 2; d++) {
      float panelX = S(8) + d * (panelW + S(8));
      float panelY = TOP_BAR_H + S(8);
      float startX = panelX + S(10);
      float startY = panelY + S(30) + modeBarH + S(5);

      for (int i = 0; i < 8; i++) {
        int col = i % 4;
        int row = i / 4;
        float px = startX + col * (padW + S(4));
        float py = startY + row * (padH + S(4));
        Rectangle rect = {px, py, padW, padH};

        if (CheckCollisionPointRec(mouse, rect)) {
          if (pressedPad != i || pressedDeck != d) {
            PadMode mode = r->State->Mode[d];

            // Immediate press trigger for touch & mouse on initial touch down
            if (isPressed || pressedPad == -1 || mode == PAD_MODE_BEAT_LOOP ||
                mode == PAD_MODE_SLIP_LOOP) {
              // Avoid releasing if we are dragging in Slip Loop mode to prevent
              // resets
              if (pressedPad != -1 && r->OnPadRelease &&
                  mode != PAD_MODE_SLIP_LOOP) {
                r->OnPadRelease(r->callbackCtx, pressedDeck, pressedPad);
              }

              pressedPad = i;
              pressedDeck = d;
              if (r->OnPadPress)
                r->OnPadPress(r->callbackCtx, d, i);
              return 1;
            }
          }
          goto handled; // Found pad, stop looking
        }
      }
    }

    // If mouse is down but not over any pad, and we HAD a pad pressed
    // we might want to release it if we moved outside the grid?
    // For loops, we usually keep them until release.
  }

handled:
  if (isReleased) {
    if (pressedPad != -1) {
      if (r->OnPadRelease)
        r->OnPadRelease(r->callbackCtx, pressedDeck, pressedPad);
      pressedPad = -1;
      pressedDeck = -1;
      return 1;
    }
  }
  return 0;
}

static void Pad_Draw(Component *base) {
  PadRenderer *r = (PadRenderer *)base;
  if (!r->State->IsActive)
    return;

  // Background
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Theme.BgMain);

  Font faceSm = UIFonts_GetFace(S(8));
  Font faceMd = UIFonts_GetFace(S(11));
  Font faceLg = UIFonts_GetFace(S(14));

  float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
  float panelW = (SCREEN_WIDTH - S(24)) / 2.0f;
  float panelH = availableH - S(16);

  for (int d = 0; d < 2; d++) {
    float panelX = S(8) + d * (panelW + S(8));
    float panelY = TOP_BAR_H + S(8);
    DeckState *deck = r->State->Decks[d];
    PadMode mode = r->State->Mode[d];

    // Panel Background
    DrawRectangleRec((Rectangle){panelX, panelY, panelW, panelH}, Theme.BgPanel);
    DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 1.0f,
                         Theme.BorderDefault);

    // Header
    DrawRectangle(panelX, panelY, panelW, S(24), Theme.BgPanelAlt);
    DrawLine(panelX, panelY + S(24), panelX + panelW, panelY + S(24),
             Theme.BorderDefault);

    char headStr[32];
    sprintf(headStr, "DECK %d PADS", d + 1);
    DrawCentredText(headStr, faceMd, panelX + S(10), panelW - S(70), panelY + S(6), S(11),
                    d == 0 ? Theme.AccentOrange : Theme.AccentBlue);

    // SHIFT Toggle Button
    bool isShiftActive = r->State->ShiftActive[d] || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    Rectangle shiftBtn = { panelX + panelW - S(52), panelY + S(2), S(48), S(20) };
    DrawRectangleRec(shiftBtn, isShiftActive ? Theme.AccentOrange : Theme.BorderDefault);
    DrawRectangleLinesEx(shiftBtn, 1.0f, isShiftActive ? Theme.TextPrimary : Theme.BorderDefault);
    DrawCentredText("SHIFT", faceSm, shiftBtn.x, shiftBtn.width, shiftBtn.y + S(5), S(8), isShiftActive ? Theme.BgMain : Theme.TextPrimary);

    // Mode Bar
    float modeBarH = S(20);
    float modeBtnW = (panelW - S(20)) / 5.0f;
    float modeX = panelX + S(10);
    float modeY = panelY + S(28);

    for (int m = 0; m < 5; m++) {
      PadMode btnModePrimary;
      PadMode btnModeSecondary;
      if (m == 0) { btnModePrimary = PAD_MODE_HOT_CUE; btnModeSecondary = PAD_MODE_GATE_CUE; }
      else if (m == 1) { btnModePrimary = PAD_MODE_BEAT_LOOP; btnModeSecondary = PAD_MODE_SLIP_LOOP; }
      else if (m == 2) { btnModePrimary = PAD_MODE_BEAT_JUMP; btnModeSecondary = PAD_MODE_BEAT_JUMP; }
      else if (m == 3) { btnModePrimary = PAD_MODE_PAD_FX; btnModeSecondary = PAD_MODE_PAD_FX; }
      else { btnModePrimary = PAD_MODE_SAMPLER; btnModeSecondary = PAD_MODE_SAMPLER; }
      
      bool isPrimaryActive = (mode == btnModePrimary);
      bool isSecondaryActive = (mode == btnModeSecondary);
      bool active = isPrimaryActive || isSecondaryActive;

      Rectangle mRect = {modeX + m * modeBtnW, modeY, modeBtnW, modeBarH};

      Color modeCol = Theme.TextPrimary;
      if (active) {
         if (mode == PAD_MODE_HOT_CUE) modeCol = Theme.TextPrimary;
         else if (mode == PAD_MODE_BEAT_LOOP) modeCol = Theme.AccentGreen;
         else if (mode == PAD_MODE_SLIP_LOOP) modeCol = Theme.AccentYellow;
         else if (mode == PAD_MODE_GATE_CUE) modeCol = Theme.AccentBlue;
         else if (mode == PAD_MODE_BEAT_JUMP) modeCol = Theme.AccentOrange;
         else if (mode == PAD_MODE_PAD_FX) modeCol = Theme.AccentRed;
         else if (mode == PAD_MODE_SAMPLER) modeCol = Theme.AccentOrange;
      }

      if (active) {
        DrawRectangleRec(mRect, Fade(modeCol, 0.4f));
        DrawRectangleLinesEx(mRect, 1.0f, modeCol);
      } else {
        DrawRectangleLinesEx(mRect, 1.0f, Theme.BorderDefault);
      }

      const char *lbl = "";
      if (isShiftActive) {
          if (m == 0) lbl = "GATE CUE";
          else if (m == 1) lbl = "SLIP LOOP";
          else if (m == 2) lbl = "BEAT JUMP";
          else if (m == 3) lbl = "FX";
          else lbl = "SAMPLER";
      } else {
          if (m == 0) lbl = isSecondaryActive ? "GATE CUE" : "HOT CUE";
          else if (m == 1) lbl = isSecondaryActive ? "SLIP LOOP" : "BEAT LOOP";
          else if (m == 2) lbl = "BEAT JUMP";
          else if (m == 3) lbl = "FX";
          else lbl = "SAMPLER";
      }

      float txtW = MeasureTextEx(faceSm, lbl, S(8), 1).x;
      DrawTextEx(faceSm, lbl,
                 (Vector2){mRect.x + (modeBtnW - txtW) / 2.0f, mRect.y + S(6)},
                 S(8), 1, active ? Theme.TextPrimary : Theme.BorderDefault);
    }

    // Pad Grid
    float padAreaW = panelW - S(20);
    float padAreaH = panelH - S(40) - modeBarH;
    float padW = (padAreaW - S(12)) / 4.0f;
    float padH = (padAreaH - S(4)) / 2.0f;
    float startX = panelX + S(10);
    float startY = modeY + modeBarH + S(5);

    for (int i = 0; i < 8; i++) {
      int col = i % 4;
      int row = i / 4;
      float px = startX + col * (padW + S(4));
      float py = startY + row * (padH + S(4));
      Rectangle rect = {px, py, padW, padH};

      // Determine pad color from mode
      Color padColor = Theme.BgPanelAlt;
      bool hasData = false;

      if (mode == PAD_MODE_HOT_CUE) {
        if (deck && deck->LoadedTrack) {
          const Color hcPalette[8] = {
              Theme.AccentYellow, Theme.AccentOrange, Theme.AccentRed, Theme.AccentRed,
              Theme.AccentGreen,  Theme.AccentGreen, Theme.AccentBlue, Theme.AccentBlue
          };

          for (int h = 0; h < deck->LoadedTrack->HotCuesCount; h++) {
            if (deck->LoadedTrack->HotCues[h].ID == (unsigned int)(i + 1)) {
              HotCue hc = deck->LoadedTrack->HotCues[h];
              padColor = GetCueColor(hc, hcPalette[i % 8]);

              // Active Loop blinking
              if (hc.Status == 4 && (int)(GetTime() * 4) % 2 == 0) {
                padColor = Theme.TextPrimary;
              }

              hasData = true;
              break;
            }
          }
        }
      } else if (mode == PAD_MODE_BEAT_LOOP || mode == PAD_MODE_SLIP_LOOP) {
        bool isActive = (r->State->ActiveLoopIdx[d] == i);
        if (mode == PAD_MODE_BEAT_LOOP) {
          padColor = Theme.AccentGreen;
          hasData = isActive;
        } else {
          // Slip Loop (Roll) - All pads usually dimmed, active is bright
          padColor = Theme.AccentYellow;
          hasData = true; // Show all pads as available
          if (!isActive)
            padColor = Fade(Theme.AccentYellow, 0.3f);
        }
      } else if (mode == PAD_MODE_BEAT_JUMP) {
        padColor = Theme.AccentOrange;
        hasData = true;
      } else if (mode == PAD_MODE_GATE_CUE) {
        if (deck && deck->LoadedTrack) {
          for (int h = 0; h < deck->LoadedTrack->HotCuesCount; h++) {
            if (deck->LoadedTrack->HotCues[h].ID == (unsigned int)(i + 1)) {
              padColor = Theme.AccentBlue;
              hasData = true;
              break;
            }
          }
        }
      } else if (mode == PAD_MODE_PAD_FX) {
        padColor = Theme.AccentRed;
        hasData = true;
      } else if (mode == PAD_MODE_SAMPLER) {
        padColor = Theme.AccentOrange;
        hasData = true;
      }

      // Draw Pad Base (Sharp Rectangle)
      DrawRectangleRec(rect, Theme.BgPanelAlt); // Use theme dark base
      DrawRectangleLinesEx(rect, 1.0f, Theme.BorderDefault);

      if (hasData) {
        // Bottom Colored Accent Bar (Moved up slightly)
        Rectangle accentBar = {px + S(4), py + padH - S(10), padW - S(8), S(3)};
        DrawRectangleRec(accentBar, padColor);
        // Subtle glow on the accent bar
        DrawRectangleRec(accentBar, Fade(padColor, 0.3f));
      }

      // Pad Label (A-H or numbers depending on mode)
      char lbl[16] = {0};
      if (mode == PAD_MODE_HOT_CUE || mode == PAD_MODE_GATE_CUE) {
        lbl[0] = 'A' + i;
        lbl[1] = '\0';
      } else if (mode == PAD_MODE_BEAT_LOOP || mode == PAD_MODE_SLIP_LOOP) {
        static const char *loops[] = {"1/4", "1/2", "1",  "2",
                                      "4",   "8",   "16", "32"};
        strcpy(lbl, loops[i]);
      } else if (mode == PAD_MODE_BEAT_JUMP) {
        static const char *jumps[] = {"<< 4", "<< 8", "<< 16", "<< 32",
                                      "4 >>", "8 >>", "16 >>", "32 >>"};
        strcpy(lbl, jumps[i]);
      } else if (mode == PAD_MODE_PAD_FX) {
        static const char *rfx[] = {"BRAKE S",  "BRAKE L", "SPIN S", "SPIN L",
                                    "ECHO 1/2", "ECHO 1",  "ECHO 2", "MUTE"};
        strcpy(lbl, rfx[i]);
      } else if (mode == PAD_MODE_SAMPLER) {
        sprintf(lbl, "SMPL %d", i + 1);
      } else {
        lbl[0] = 'A' + i;
        lbl[1] = '\0';
      }

      // Top-Left Label like XDJ hardware
      UIDrawText(lbl, faceLg, px + S(6), py + S(4), S(13),
                 hasData ? Theme.TextPrimary : Theme.TextMuted);

      if (Input_IsDown() &&
          CheckCollisionPointRec(Input_GetPointerPos(), rect)) {
        DrawRectangleRec(rect, Fade(Theme.TextPrimary, 0.3f));
      }
    }
  }
}

void PadRenderer_Init(PadRenderer *r, PadState *state) {
  r->base.Update = Pad_Update;
  r->base.Draw = Pad_Draw;
  r->State = state;
  r->OnPadPress = NULL;
  r->OnPadRelease = NULL;
  r->OnModeChange = NULL;
  r->callbackCtx = NULL;
}
