#include "ui/views/pad.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>
#include <string.h>

static const char* PAD_MODE_LABELS[] = {
    "HOT CUE", "BEAT LOOP", "SLIP LOOP", "BEAT JUMP", "GATE CUE", "RELEASE FX"
};

static int Pad_Update(Component *base) {
    PadRenderer *r = (PadRenderer *)base;
    if (!r->State->IsActive) return 0;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = UIGetMousePosition();
        float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
        float panelW = (SCREEN_WIDTH - S(24)) / 2.0f;
        float panelH = availableH - S(16);
        
        float modeBarH = S(20);
        float padAreaW = panelW - S(20);
        float padAreaH = panelH - S(40) - modeBarH;
        float padW = (padAreaW - S(12)) / 4.0f;
        float padH = (padAreaH - S(4)) / 2.0f;

        for (int d = 0; d < 2; d++) {
            float panelX = S(8) + d * (panelW + S(8));
            float panelY = TOP_BAR_H + S(8);
            
            // Mode selection hits
            float modeBtnW = (panelW - S(20)) / 6.0f;
            float modeX = panelX + S(10);
            float modeY = panelY + S(28);
            
            for (int m = 0; m < PAD_MODE_COUNT; m++) {
                Rectangle mRect = { modeX + m * modeBtnW, modeY, modeBtnW, modeBarH };
                if (CheckCollisionPointRec(mouse, mRect)) {
                    r->State->Mode[d] = (PadMode)m;
                    if (r->OnModeChange) r->OnModeChange(r->callbackCtx, d, (PadMode)m);
                    return 1;
                }
            }

            // Pad hits
            float startX = panelX + S(10);
            float startY = panelY + S(30) + modeBarH + S(5);

            for (int i = 0; i < 8; i++) {
                int col = i % 4;
                int row = i / 4;
                float px = startX + col * (padW + S(4));
                float py = startY + row * (padH + S(4));
                Rectangle rect = { px, py, padW, padH };

                if (CheckCollisionPointRec(mouse, rect)) {
                    if (r->OnPadPress) r->OnPadPress(r->callbackCtx, d, i);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void Pad_Draw(Component *base) {
    PadRenderer *r = (PadRenderer *)base;
    if (!r->State->IsActive) return;

    // Background
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBGUtil);

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
        DrawRectangleRec((Rectangle){panelX, panelY, panelW, panelH}, ColorDark2);
        DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 1.0f, ColorDark1);
        
        // Header
        DrawRectangle(panelX, panelY, panelW, S(24), ColorDark3);
        DrawLine(panelX, panelY + S(24), panelX + panelW, panelY + S(24), ColorShadow);
        
        char headStr[32];
        sprintf(headStr, "DECK %d PADS", d + 1);
        DrawCentredText(headStr, faceMd, panelX, panelW, panelY + S(6), S(11), d == 0 ? ColorOrange : ColorBlue);

        // Mode Bar
        float modeBarH = S(20);
        float modeBtnW = (panelW - S(20)) / 6.0f;
        float modeX = panelX + S(10);
        float modeY = panelY + S(28);

        for (int m = 0; m < PAD_MODE_COUNT; m++) {
            bool active = (mode == (PadMode)m);
            Rectangle mRect = { modeX + m * modeBtnW, modeY, modeBtnW, modeBarH };
            
            Color modeCol = ColorWhite;
            if (m == PAD_MODE_HOT_CUE) modeCol = ColorWhite;
            else if (m == PAD_MODE_BEAT_LOOP) modeCol = ColorGreen;
            else if (m == PAD_MODE_SLIP_LOOP) modeCol = ColorYellow;
            else if (m == PAD_MODE_BEAT_JUMP) modeCol = ColorOrange;
            else if (m == PAD_MODE_GATE_CUE) modeCol = ColorBlue;
            else if (m == PAD_MODE_RELEASE_FX) modeCol = ColorPink;

            if (active) {
                DrawRectangleRec(mRect, Fade(modeCol, 0.4f));
                DrawRectangleLinesEx(mRect, 1.0f, modeCol);
            } else {
                DrawRectangleLinesEx(mRect, 1.0f, ColorDark1);
            }
            
            // Draw very small text label
            const char* lbl = PAD_MODE_LABELS[m];
            float txtW = MeasureTextEx(faceSm, lbl, S(8), 1).x;
            DrawTextEx(faceSm, lbl, (Vector2){ mRect.x + (modeBtnW - txtW)/2.0f, mRect.y + S(6) }, S(8), 1, active ? ColorWhite : ColorShadow);
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
            Rectangle rect = { px, py, padW, padH };

            // Determine pad color from mode
            Color padColor = ColorDark3;
            bool hasData = false;

            if (mode == PAD_MODE_HOT_CUE) {
                if (deck && deck->LoadedTrack && i < deck->LoadedTrack->HotCuesCount) {
                    HotCue hc = deck->LoadedTrack->HotCues[i];
                    padColor = GetCueColor(hc, ColorOrange);
                    hasData = true;
                }
            } else if (mode == PAD_MODE_BEAT_LOOP) {
                padColor = ColorGreen; hasData = true;
            } else if (mode == PAD_MODE_SLIP_LOOP) {
                padColor = ColorYellow; hasData = true;
            } else if (mode == PAD_MODE_BEAT_JUMP) {
                padColor = ColorOrange; hasData = true;
            } else if (mode == PAD_MODE_GATE_CUE) {
                padColor = ColorBlue; hasData = true;
            } else if (mode == PAD_MODE_RELEASE_FX) {
                padColor = ColorPink; hasData = true;
            }

            // Draw Pad
            DrawRectangleRec(rect, Fade(padColor, hasData ? 0.4f : 0.1f));
            DrawRectangleLinesEx(rect, 2.0f, hasData ? padColor : ColorDark1);
            
            // Pad Label (A-H or numbers depending on mode)
            char lbl[16];
            if (mode == PAD_MODE_BEAT_LOOP) {
                static const char* loops[] = {"1/4", "1/2", "1", "2", "4", "8", "16", "32"};
                strcpy(lbl, loops[i]);
            } else {
                lbl[0] = (char)('A' + i); lbl[1] = 0;
            }
            DrawCentredText(lbl, faceLg, px, padW, py + (padH/2.0f) - S(7), S(14), hasData ? ColorWhite : ColorShadow);
            
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), rect)) {
                DrawRectangleRec(rect, Fade(ColorWhite, 0.3f));
            }
        }
    }
}

void PadRenderer_Init(PadRenderer *r, PadState *state) {
    r->base.Update = Pad_Update;
    r->base.Draw = Pad_Draw;
    r->State = state;
    r->OnPadPress = NULL;
    r->OnModeChange = NULL;
    r->callbackCtx = NULL;
}
