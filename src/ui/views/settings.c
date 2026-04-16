#include "ui/views/settings.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>
#include <math.h>


static int Settings_Update(Component *base) {
  SettingsRenderer *r = (SettingsRenderer *)base;
  if (!r->State->IsActive)
    return 0;

  Vector2 mouse = GetMousePosition();

  // Handle dropdown intercept
  if (r->State->IsDropdownOpen) {
      SettingItem *item = &r->State->Items[r->State->DropdownItemIdx];
      float winH = GetScreenHeight() - DECK_STR_H;
      float winW = GetScreenWidth();
      
      float dropdownW = S(200.0f);
      float opHeight = S(40.0f);
      float contentH = item->OptionsCount * opHeight;
      float dropdownH = contentH > (winH * 0.7f) ? (winH * 0.7f) : contentH;
      float dropdownX = (winW - dropdownW) / 2.0f;
      float dropdownY = (winH - dropdownH) / 2.0f;
      
      Rectangle dropRect = { dropdownX, dropdownY, dropdownW, dropdownH };
      
      // Scroll handling for dropdown
      Vector2 mouseReq = GetMouseDelta();
      r->State->DropdownScroll -= GetMouseWheelMove() * S(30.0f);
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, dropRect)) {
          r->State->DropdownScroll -= mouseReq.y;
      }
      
      float maxScroll = contentH - dropdownH;
      if (maxScroll < 0) maxScroll = 0;
      if (r->State->DropdownScroll < 0) r->State->DropdownScroll = 0;
      if (r->State->DropdownScroll > maxScroll) r->State->DropdownScroll = maxScroll;
      
      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
          if (!CheckCollisionPointRec(mouse, dropRect)) {
              r->State->IsDropdownOpen = false;
          } else {
              float cy = dropdownY - r->State->DropdownScroll;
              for(int i=0; i<item->OptionsCount; i++) {
                  Rectangle opRect = { dropdownX, cy, dropdownW, opHeight };
                  if (CheckCollisionPointRec(mouse, opRect) && cy >= dropdownY && (cy + opHeight) <= (dropdownY + dropdownH)) {
                      if (fabsf(r->State->TouchDragAccumulator) < 10.0f) {
                          item->Current = i;
                          r->State->IsDropdownOpen = false;
                          if (r->OnApply) r->OnApply(r->callbackCtx);
                      }
                  }
                  cy += opHeight;
              }
          }
      }
      
      if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
         r->State->IsDropdownOpen = false;
      }
      return 1; // block background UI interaction
  }

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    Vector2 delta = GetMouseDelta();
    r->State->TouchDragAccumulator += delta.y;
    
    int visibleRows = 12;
    float threshold = S(20.0f);
    if (r->State->TouchDragAccumulator < -threshold) { 
      if (r->State->Scroll + visibleRows < r->State->ItemsCount) {
        r->State->Scroll++;
      }
      r->State->TouchDragAccumulator = 0;
    } else if (r->State->TouchDragAccumulator > threshold) {
      if (r->State->Scroll > 0) {
        r->State->Scroll--;
      }
      r->State->TouchDragAccumulator = 0;
    }
  }

  // Mouse Wheel Scroll
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
      if (wheel > 0) {
          if (r->State->Scroll > 0) r->State->Scroll--;
      } else {
          int visibleRows = 12;
          if (r->State->Scroll + visibleRows < r->State->ItemsCount) {
              r->State->Scroll++;
          }
      }
  }

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      r->State->TouchDragAccumulator = 0;
  }

  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    Vector2 mouse = UIGetMousePosition();
    float bottomH = S(28.0f);
    float divY = SCREEN_HEIGHT - DECK_STR_H - bottomH;

    // DONE Button: DrawRectangle(S(15), divY + S(5), S(90), S(18), ...)
    if (mouse.x >= S(15) && mouse.x <= S(105) && mouse.y >= divY + S(5) &&
        mouse.y <= divY + S(23)) {
      if (r->OnApply)
        r->OnApply(r->callbackCtx);
      if (r->OnClose)
        r->OnClose(r->callbackCtx);
      return 1;
    }
    // CLOSE Button: DrawRectangle(SCREEN_WIDTH - S(105), divY + S(5), S(90), S(18), ...)
    if (mouse.x >= SCREEN_WIDTH - S(105) && mouse.x <= SCREEN_WIDTH - S(15) &&
        mouse.y >= divY + S(5) && mouse.y <= divY + S(23)) {
      if (r->OnClose)
        r->OnClose(r->callbackCtx);
      return 1;
    }

    // List Item Selection & Action Clicking
    float rowH = S(28.0f);
    int visibleRows = 12;
    for (int i = 0; i < visibleRows; i++) {
      int idx = r->State->Scroll + i;
      if (idx >= r->State->ItemsCount)
        break;

      float ry = TOP_BAR_H + (i * rowH);
      Rectangle rowRect = {0, ry, SCREEN_WIDTH, rowH};

      if (CheckCollisionPointRec(mouse, rowRect) && fabsf(r->State->TouchDragAccumulator) < 10.0f) {
        r->State->CursorPos = i; 
        
        SettingItem *clickedItem = &r->State->Items[idx];
        if (clickedItem->Type == SETTING_TYPE_ACTION) {
           if (r->OnAction)
             r->OnAction(r->callbackCtx, idx);
        } else if (clickedItem->Type == SETTING_TYPE_LIST) {
            r->State->IsDropdownOpen = true;
            r->State->DropdownItemIdx = idx;
            r->State->DropdownScroll = 0;
            return 1;
        }
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
    if (r->State->CursorPos < visibleRows - 1 &&
        r->State->Scroll + r->State->CursorPos < r->State->ItemsCount - 1) {
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
        if (item->Current > 0)
          item->Current--;
        else
          item->Current = item->OptionsCount - 1;
        if (r->OnApply) r->OnApply(r->callbackCtx);
      }
      if (IsKeyPressed(KEY_RIGHT)) {
        if (item->Current < item->OptionsCount - 1)
          item->Current++;
        else
          item->Current = 0;
        if (r->OnApply) r->OnApply(r->callbackCtx);
      }
      if (IsKeyPressed(KEY_ENTER)) {
          r->State->IsDropdownOpen = true;
          r->State->DropdownItemIdx = idx;
          r->State->DropdownScroll = 0;
          return 0;
      }
    } else if (item->Type == SETTING_TYPE_KNOB) {
      float step = (item->Max - item->Min) / 20.0f; // 5% steps
      if (IsKeyDown(KEY_LEFT)) {
        item->Value -= step * GetFrameTime() * 10.0f;
        if (item->Value < item->Min)
          item->Value = item->Min;
      }
      if (IsKeyDown(KEY_RIGHT)) {
        item->Value += step * GetFrameTime() * 10.0f;
        if (item->Value > item->Max)
          item->Value = item->Max;
      }
      
      if (IsKeyReleased(KEY_LEFT) || IsKeyReleased(KEY_RIGHT)) {
          if (r->OnApply) r->OnApply(r->callbackCtx);
      }
    } else if (item->Type == SETTING_TYPE_ACTION) {
      if (IsKeyPressed(KEY_ENTER)) {
        if (r->OnAction)
          r->OnAction(r->callbackCtx, idx);
        return 0; // Prevent fall-through to global Enter/Apply handler
      }
    }
  }

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
    if (r->OnClose)
      r->OnClose(r->callbackCtx);
  }

  if (IsKeyPressed(KEY_ENTER)) {
    if (r->OnApply)
      r->OnApply(r->callbackCtx);
  }
  return 0;
}

static void Settings_Draw(Component *base) {
  SettingsRenderer *r = (SettingsRenderer *)base;
  if (!r->State->IsActive)
    return;

  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  DrawRectangle(0, 0, SCREEN_WIDTH, viewH, ColorBGUtil);

  Font faceXS = UIFonts_GetFace(S(9));
  Font faceSm = UIFonts_GetFace(S(11));
  Font faceMd = UIFonts_GetFace(S(13));

  float rowH = S(28.0f);
  int visibleRows = 12;

  for (int i = 0; i < visibleRows; i++) {
    int idx = r->State->Scroll + i;
    if (idx >= r->State->ItemsCount)
      break;

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
      const char *valStr = "";
      if (item->Current < item->OptionsCount)
        valStr = item->Options[item->Current];

      UIDrawText(valStr, faceMd, SCREEN_WIDTH - S(120), ry + S(6), S(13),
                 ColorOrange);

      if (selected && item->OptionsCount > 1) {
        UIDrawText("\xe2\x97\x84", faceXS, SCREEN_WIDTH - S(150), ry + S(6),
                   S(9), ColorShadow); // ◄
        UIDrawText("\xe2\x96\xba", faceXS, SCREEN_WIDTH - S(15), ry + S(6),
                   S(9), ColorShadow); // ►
      }
    } else if (item->Type == SETTING_TYPE_KNOB) {
      UIDrawKnob(SCREEN_WIDTH - S(80), ry + (rowH / 2.0f), S(9), item->Value,
                 item->Min, item->Max, item->Unit, ColorOrange);
    } else if (item->Type == SETTING_TYPE_ACTION) {
      // Action items like EXIT don't have side controls, maybe just a symbol
      UIDrawText("\uf2f5", faceSm, SCREEN_WIDTH - S(30), ry + S(6), S(12),
                 ColorOrange); // Sign-out icon placeholder
    }
  }

  float bottomH = S(28.0f);
  DrawScrollbar(SCREEN_WIDTH - S(2.5f), TOP_BAR_H, S(2), viewH - TOP_BAR_H - bottomH,
                r->State->ItemsCount, r->State->Scroll, visibleRows);

  float divY = viewH - bottomH;

  // Bottom Background
  DrawRectangle(0, divY, SCREEN_WIDTH, bottomH, ColorDark1);
  DrawLine(0, divY, SCREEN_WIDTH, divY, ColorGray);

  // DONE Button (Blue)
  DrawRectangle(S(15), divY + S(5), S(90), S(18), ColorBlue);
  DrawRectangleLines(S(15), divY + S(5), S(90), S(18), ColorShadow);
  DrawCentredText("DONE", faceSm, S(15), S(90), divY + S(8), S(11), ColorWhite);

  char countStr[32];
  sprintf(countStr, "%d / %d", r->State->Scroll + r->State->CursorPos + 1,
          r->State->ItemsCount);
  UIDrawText(countStr, faceXS, SCREEN_WIDTH / 2.0f - S(24.0f), divY + S(8),
             S(9), ColorShadow);

  // CLOSE Button
  DrawRectangle(SCREEN_WIDTH - S(105), divY + S(5), S(90), S(18), ColorDark2);
  DrawRectangleLines(SCREEN_WIDTH - S(105), divY + S(5), S(90), S(18),
                     ColorShadow);
  DrawCentredText("CLOSE", faceSm, SCREEN_WIDTH - S(105), S(90), divY + S(8), S(11), ColorWhite);

  // Draw Dropdown Overlay
  if (r->State->IsDropdownOpen) {
      DrawRectangle(0, 0, SCREEN_WIDTH, viewH, (Color){ 0, 0, 0, 200 }); // Dim BG
      
      SettingItem *item = &r->State->Items[r->State->DropdownItemIdx];
      float winW = GetScreenWidth();
      float dropdownW = S(200.0f);
      float opHeight = S(40.0f);
      float contentH = item->OptionsCount * opHeight;
      float dropdownH = contentH > (viewH * 0.7f) ? (viewH * 0.7f) : contentH;
      float dropdownX = (winW - dropdownW) / 2.0f;
      float dropdownY = (viewH - dropdownH) / 2.0f;
      
      Rectangle dropRect = { dropdownX, dropdownY, dropdownW, dropdownH };
      
      BeginScissorMode(dropRect.x, dropRect.y, dropRect.width, dropRect.height);
      DrawRectangleRec(dropRect, ColorBGUtil);
      
      float cy = dropdownY - r->State->DropdownScroll;
      for(int i=0; i<item->OptionsCount; i++) {
          Rectangle opRect = { dropdownX, cy, dropdownW, opHeight };
          
          if (cy + opHeight > dropdownY && cy < dropdownY + dropdownH) {
              if (item->Current == i) {
                  DrawRectangleRec(opRect, ColorGray);
              } else {
                  DrawRectangleRec(opRect, ColorDark1);
              }
              DrawRectangleLinesEx(opRect, 1, ColorShadow);
              UIDrawText(item->Options[i], faceMd, dropdownX + S(20), cy + S(12), S(15), (item->Current == i) ? ColorOrange : ColorWhite);
          }
          cy += opHeight;
      }
      EndScissorMode();
      
      DrawRectangleLinesEx(dropRect, 2, ColorOrange);
      
      // Draw dropdown scrollbar if needed
      if (contentH > dropdownH) {
          float sbY = dropdownY + (r->State->DropdownScroll / contentH) * dropdownH;
          float sbH = (dropdownH / contentH) * dropdownH;
          DrawRectangle(dropdownX + dropdownW - S(4), sbY, S(4), sbH, ColorOrange);
      }
  }
}

void SettingsRenderer_Init(SettingsRenderer *r, SettingsState *state) {
  r->base.Update = Settings_Update;
  r->base.Draw = Settings_Draw;
  r->State = state;
  r->OnClose = NULL;
  r->OnApply = NULL;
  r->callbackCtx = NULL;
}
