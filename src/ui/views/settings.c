#include "ui/views/settings.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>

static int Settings_Update(Component *base) {
    SettingsRenderer *r = (SettingsRenderer *)base;
    if (!r->State->IsActive) return 0;

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        float divY = SCREEN_HEIGHT - 28.0f;

        if (mouse.x >= 30 && mouse.x <= 110 && mouse.y >= divY + 4 && mouse.y <= divY + 22) {
            if (r->OnClose) r->OnClose(r->callbackCtx);
        }
        if (mouse.x >= SCREEN_WIDTH - 110 && mouse.x <= SCREEN_WIDTH - 30 && mouse.y >= divY + 4 && mouse.y <= divY + 22) {
            if (r->OnClose) r->OnClose(r->callbackCtx);
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
        if (IsKeyPressed(KEY_LEFT)) {
            if (item->Current > 0) item->Current--;
            else item->Current = item->OptionsCount - 1;
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            if (item->Current < item->OptionsCount - 1) item->Current++;
            else item->Current = 0;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        if (r->OnClose) r->OnClose(r->callbackCtx);
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

    float rowH = 28.0f;
    int visibleRows = 12;

    for (int i = 0; i < visibleRows; i++) {
        int idx = r->State->Scroll + i;
        if (idx >= r->State->ItemsCount) break;

        SettingItem *item = &r->State->Items[idx];
        float ry = TOP_BAR_H + (i * rowH);

        bool selected = (i == r->State->CursorPos);
        if (selected) {
            DrawRectangle(0, ry, SCREEN_WIDTH - 2, rowH - 1, ColorGray);
        } else if (i % 2 == 0) {
            DrawRectangle(0, ry, SCREEN_WIDTH - 2, rowH - 1, ColorDark1);
        }

        Color rowClr = ColorWhite;

        if (selected) {
            UIDrawText("\xe2\x96\xba", faceXS, S(6), ry + S(6), S(9), ColorOrange); // ►
        }

        UIDrawText(item->Label, faceMd, S(24), ry + S(6), S(13), rowClr);

        const char* valStr = "";
        if (item->Current < item->OptionsCount) valStr = item->Options[item->Current];
        
        UIDrawText(valStr, faceMd, SCREEN_WIDTH - Si(120), ry + S(6), S(13), ColorOrange);

        if (selected && item->OptionsCount > 1) {
            UIDrawText("\xe2\x97\x84", faceXS, SCREEN_WIDTH - Si(150), ry + S(6), S(9), ColorShadow); // ◄
            UIDrawText("\xe2\x96\xba", faceXS, SCREEN_WIDTH - Si(15), ry + S(6), S(9), ColorShadow);  // ►
        }
    }

    DrawScrollbar(r->State->ItemsCount, r->State->Scroll + r->State->CursorPos, visibleRows);

    float divY = SCREEN_HEIGHT - 28.0f;
    DrawLine(4, divY, SCREEN_WIDTH - 4, divY, ColorDark1);

    DrawRectangle(30, divY + 4, 80, 18, ColorDGreen);
    UIDrawText("APPLY", faceSm, 44, divY + 7, S(11), ColorBlack);

    char countStr[32];
    sprintf(countStr, "%d / %d", r->State->Scroll + r->State->CursorPos + 1, r->State->ItemsCount);
    UIDrawText(countStr, faceXS, SCREEN_WIDTH / 2.0f - 24.0f, divY + 7, S(9), ColorShadow);

    DrawRectangle(SCREEN_WIDTH - 110, divY + 4, 80, 18, ColorDark1);
    DrawRectangleLines(SCREEN_WIDTH - 110, divY + 4, 80, 18, ColorShadow);
    UIDrawText("CANCEL", faceSm, SCREEN_WIDTH - 90, divY + 7, S(11), ColorWhite);
}

void SettingsRenderer_Init(SettingsRenderer *r, SettingsState *state) {
    r->base.Update = Settings_Update;
    r->base.Draw = Settings_Draw;
    r->State = state;
    r->OnClose = NULL;
    r->callbackCtx = NULL;
}
