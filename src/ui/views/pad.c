#include "ui/views/pad.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>

static int Pad_Update(Component *base) {
    PadRenderer *r = (PadRenderer *)base;
    if (!r->State->IsActive) return 0;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = UIGetMousePosition();
        float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
        float panelW = (SCREEN_WIDTH - S(24)) / 2.0f;
        float panelH = availableH - S(16);
        float padAreaW = panelW - S(20);
        float padAreaH = panelH - S(40);
        float padW = (padAreaW - S(12)) / 4.0f;
        float padH = (padAreaH - S(4)) / 2.0f;

        for (int d = 0; d < 2; d++) {
            float panelX = S(8) + d * (panelW + S(8));
            float panelY = TOP_BAR_H + S(8);
            float startX = panelX + S(10);
            float startY = panelY + S(30);

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

    Font faceMd = UIFonts_GetFace(S(11));
    Font faceLg = UIFonts_GetFace(S(14));

    float availableH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
    float panelW = (SCREEN_WIDTH - S(24)) / 2.0f;
    float panelH = availableH - S(16);

    for (int d = 0; d < 2; d++) {
        float panelX = S(8) + d * (panelW + S(8));
        float panelY = TOP_BAR_H + S(8);
        DeckState *deck = r->State->Decks[d];

        // Panel Background with subtle gradient/glow
        DrawRectangleRec((Rectangle){panelX, panelY, panelW, panelH}, ColorDark2);
        DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 1.0f, ColorDark1);
        
        // Header
        DrawRectangle(panelX, panelY, panelW, S(24), ColorDark3);
        DrawLine(panelX, panelY + S(24), panelX + panelW, panelY + S(24), ColorShadow);
        
        char headStr[32];
        sprintf(headStr, "DECK %d PADS", d + 1);
        DrawCentredText(headStr, faceMd, panelX, panelW, panelY + S(6), S(11), d == 0 ? ColorOrange : ColorBlue);

        // Pad Grid
        float padAreaW = panelW - S(20);
        float padAreaH = panelH - S(40);
        float padW = (padAreaW - S(12)) / 4.0f;
        float padH = (padAreaH - S(4)) / 2.0f;
        float startX = panelX + S(10);
        float startY = panelY + S(30);

        for (int i = 0; i < 8; i++) {
            int col = i % 4;
            int row = i / 4;
            float px = startX + col * (padW + S(4));
            float py = startY + row * (padH + S(4));
            Rectangle rect = { px, py, padW, padH };

            // Determine pad color from HotCue if available
            Color padColor = ColorDark3;
            bool hasCue = false;
            if (deck && deck->LoadedTrack && i < deck->LoadedTrack->HotCuesCount) {
                HotCue hc = deck->LoadedTrack->HotCues[i];
                padColor = GetCueColor(hc, ColorOrange);
                hasCue = true;
            }

            // Draw Pad
            DrawRectangleRec(rect, Fade(padColor, hasCue ? 0.4f : 0.1f));
            DrawRectangleLinesEx(rect, 2.0f, hasCue ? padColor : ColorDark1);
            
            // Pad Label (A-H)
            char lbl[2] = { (char)('A' + i), 0 };
            DrawCentredText(lbl, faceLg, px, padW, py + (padH/2.0f) - S(7), S(14), hasCue ? ColorWhite : ColorShadow);
            
            // Visual Feedback when pressed (simplified since we handle click in Update)
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
    r->callbackCtx = NULL;
}
