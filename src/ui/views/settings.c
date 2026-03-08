#include "ui/views/settings.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>

static int Settings_Update(Component *base) {
    SettingsRenderer *r = (SettingsRenderer *)base;
    if (!r->State->IsActive) return 0;

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = UIGetMousePosition();
        float divY = SCREEN_HEIGHT - S(28.0f);

        // Buttons in Reference Space (using Si to convert mouse to ref or scaled to scaled)
        // Actually UIGetMousePosition returns reference-like space (offset subtracted) 
        // but it's not divided by scale. So we should compare against scaled values.
        
        // APPLY Button: DrawRectangle(S(30), divY + S(4), S(80), S(18), ...)
        if (mouse.x >= S(30) && mouse.x <= S(110) && mouse.y >= divY + S(4) && mouse.y <= divY + S(22)) {
            if (r->OnApply) r->OnApply(r->callbackCtx);
            else if (r->OnClose) r->OnClose(r->callbackCtx);
        }
        // CANCEL Button: DrawRectangle(SCREEN_WIDTH - S(110), divY + S(4), S(80), S(18), ...)
        if (mouse.x >= SCREEN_WIDTH - S(110) && mouse.x <= SCREEN_WIDTH - S(30) && mouse.y >= divY + S(4) && mouse.y <= divY + S(22)) {
            if (r->OnClose) r->OnClose(r->callbackCtx);
        }

        // List Item Selection & Action Clicking
        float rowH = S(28.0f);
        int visibleRows = 12;
        for (int i = 0; i < visibleRows; i++) {
            int idx = r->State->Scroll + i;
            if (idx >= r->State->ItemsCount) break;

            float ry = TOP_BAR_H + (i * rowH);
            Rectangle rowRect = {0, ry, SCREEN_WIDTH, rowH};
            
            if (CheckCollisionPointRec(mouse, rowRect)) {
                static float lastClickTime = 0;
                float now = GetTime();
                
                if (r->State->CursorPos == i && (now - lastClickTime < 0.3f)) {
                    // Double click or confirmed select on item
                    SettingItem *clickedItem = &r->State->Items[idx];
                    if (clickedItem->Type == SETTING_TYPE_ACTION) {
                        if (r->OnAction) r->OnAction(r->callbackCtx, idx);
                    }
                }
                r->State->CursorPos = i;
                lastClickTime = now;
            }
        }
    }

    if (IsKeyPressed(KEY_UP)) {
        if (r->State->CursorPos > 0) {
            r->State->CursorPos--;
        } else if (r->State->Scroll > 0) {
            r->State->Scroll--;
        }
    }
    if (IsKeyPressed(KEY_DOWN)) {
        int visibleRows = 12;
        if (r->State->CursorPos < visibleRows - 1 && r->State->Scroll + r->State->CursorPos < r->State->ItemsCount - 1) {
            r->State->CursorPos++;
        } else if (r->State->Scroll + visibleRows < r->State->ItemsCount) {
            r->State->Scroll++;
        }
    }

    int idx = r->State->Scroll + r->State->CursorPos;
    if (idx < r->State->ItemsCount) {
        SettingItem *item = &r->State->Items[idx];
        if (item->Type == SETTING_TYPE_LIST) {
            if (IsKeyPressed(KEY_LEFT)) {
                if (item->Current > 0) item->Current--;
                else item->Current = item->OptionsCount - 1;
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                if (item->Current < item->OptionsCount - 1) item->Current++;
                else item->Current = 0;
            }
        } else if (item->Type == SETTING_TYPE_KNOB) {
            float step = (item->Max - item->Min) / 20.0f; // 5% steps
            if (IsKeyDown(KEY_LEFT)) {
                item->Value -= step * GetFrameTime() * 10.0f;
                if (item->Value < item->Min) item->Value = item->Min;
            }
            if (IsKeyDown(KEY_RIGHT)) {
                item->Value += step * GetFrameTime() * 10.0f;
                if (item->Value > item->Max) item->Value = item->Max;
            }
        } else if (item->Type == SETTING_TYPE_ACTION) {
            if (IsKeyPressed(KEY_ENTER)) {
                if (r->OnAction) r->OnAction(r->callbackCtx, idx);
                return 0; // Prevent fall-through to global Enter/Apply handler
            }
        }
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        if (r->OnClose) r->OnClose(r->callbackCtx);
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (r->OnApply) r->OnApply(r->callbackCtx);
    }
    return 0;
}

static void Settings_Draw(Component *base) {
    SettingsRenderer *r = (SettingsRenderer *)base;
    if (!r->State->IsActive) return;

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBGUtil);

    Font faceXS = UIFonts_GetFace(S(9));
    Font faceSm = UIFonts_GetFace(S(11));
    Font faceMd = UIFonts_GetFace(S(13));

    float rowH = S(28.0f);
    int visibleRows = 12;

    for (int i = 0; i < visibleRows; i++) {
        int idx = r->State->Scroll + i;
        if (idx >= r->State->ItemsCount) break;

        SettingItem *item = &r->State->Items[idx];
        float ry = TOP_BAR_H + (i * rowH);

        bool selected = (i == r->State->CursorPos);
        if (selected) {
            DrawRectangle(0, ry, SCREEN_WIDTH - S(2), rowH - S(1), ColorGray);
        } else if (i % 2 == 0) {
            DrawRectangle(0, ry, SCREEN_WIDTH - S(2), rowH - S(1), ColorDark1);
        }

        Color rowClr = ColorWhite;

        if (selected) {
            DrawSelectionTriangle(S(8), ry + S(9), ColorOrange);
        }

        UIDrawText(item->Label, faceMd, S(24), ry + S(6), S(13), rowClr);

        if (item->Type == SETTING_TYPE_LIST) {
            const char* valStr = "";
            if (item->Current < item->OptionsCount) valStr = item->Options[item->Current];
            
            UIDrawText(valStr, faceMd, SCREEN_WIDTH - S(120), ry + S(6), S(13), ColorOrange);

            if (selected && item->OptionsCount > 1) {
                UIDrawText("\xe2\x97\x84", faceXS, SCREEN_WIDTH - S(150), ry + S(6), S(9), ColorShadow); // ◄
                UIDrawText("\xe2\x96\xba", faceXS, SCREEN_WIDTH - S(15), ry + S(6), S(9), ColorShadow);  // ►
            }
        } else if (item->Type == SETTING_TYPE_KNOB) {
            UIDrawKnob(SCREEN_WIDTH - S(80), ry + (rowH / 2.0f), S(9), item->Value, item->Min, item->Max, item->Unit, ColorOrange);
        } else if (item->Type == SETTING_TYPE_ACTION) {
            // Action items like EXIT don't have side controls, maybe just a symbol
            UIDrawText("\uf2f5", faceSm, SCREEN_WIDTH - S(30), ry + S(6), S(12), ColorOrange); // Sign-out icon placeholder
        }
    }

    DrawScrollbar(r->State->ItemsCount, r->State->Scroll + r->State->CursorPos, visibleRows);

    float divY = SCREEN_HEIGHT - S(28.0f);
    DrawLine(S(4), divY, SCREEN_WIDTH - S(4), divY, ColorDark1);

    DrawRectangle(S(30), divY + S(4), S(80), S(18), ColorDGreen);
    UIDrawText("APPLY", faceSm, S(44), divY + S(7), S(11), ColorBlack);

    char countStr[32];
    sprintf(countStr, "%d / %d", r->State->Scroll + r->State->CursorPos + 1, r->State->ItemsCount);
    UIDrawText(countStr, faceXS, SCREEN_WIDTH / 2.0f - S(24.0f), divY + S(7), S(9), ColorShadow);

    DrawRectangle(SCREEN_WIDTH - S(110), divY + S(4), S(80), S(18), ColorDark1);
    DrawRectangleLines(SCREEN_WIDTH - S(110), divY + S(4), S(80), S(18), ColorShadow);
    UIDrawText("CLOSE", faceSm, SCREEN_WIDTH - S(90), divY + S(7), S(11), ColorWhite);
}

void SettingsRenderer_Init(SettingsRenderer *r, SettingsState *state) {
    r->base.Update = Settings_Update;
    r->base.Draw = Settings_Draw;
    r->State = state;
    r->OnClose = NULL;
    r->OnApply = NULL;
    r->callbackCtx = NULL;
}
