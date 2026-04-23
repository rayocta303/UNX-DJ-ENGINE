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
                          if (r->OnValueChanged) r->OnValueChanged(r->callbackCtx, r->State->DropdownItemIdx);
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

  // Get filtered indices
  int filteredIndices[MAX_SETTINGS_ITEMS];
  int filteredCount = 0;
  for (int i = 0; i < r->State->ItemsCount; i++) {
    if (r->State->Items[i].Category == (SettingCategory)r->State->SelectedTab) {
      filteredIndices[filteredCount++] = i;
    }
  }

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    Vector2 delta = GetMouseDelta();
    r->State->TouchDragAccumulator += delta.y;
    
    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    float tabH = S(28.0f);
    int visibleRows = (int)((viewH - TOP_BAR_H - tabH - S(28.0f)) / S(28.0f));
    float threshold = S(20.0f);
    if (r->State->TouchDragAccumulator < -threshold) { 
      if (r->State->Scroll + visibleRows < filteredCount) {
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
          float viewH = SCREEN_HEIGHT - DECK_STR_H;
          float tabH = S(28.0f);
          int visibleRows = (int)((viewH - TOP_BAR_H - tabH - S(28.0f)) / S(28.0f));
          if (r->State->Scroll + visibleRows < filteredCount) {
              r->State->Scroll++;
          }
      }
  }

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      r->State->TouchDragAccumulator = 0;
  }

  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    Vector2 mouse = UIGetMousePosition();
    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    float bottomH = S(28.0f);
    float divY = viewH - bottomH;
    float tabH = S(28.0f);

    // Tab Switching Logic
    if (mouse.y >= TOP_BAR_H && mouse.y <= TOP_BAR_H + tabH) {
        float tabW = SCREEN_WIDTH / (float)SETTING_CAT_COUNT;
        int tabIdx = (int)(mouse.x / tabW);
        if (tabIdx >= 0 && tabIdx < SETTING_CAT_COUNT) {
            if (r->State->SelectedTab != tabIdx) {
                r->State->SelectedTab = tabIdx;
                r->State->Scroll = 0;
                r->State->CursorPos = 0;
            }
            return 1;
        }
    }

    // DONE Button
    if (mouse.x >= S(15) && mouse.x <= S(105) && mouse.y >= divY + S(5) &&
        mouse.y <= divY + S(23)) {
      if (r->OnApply)
        r->OnApply(r->callbackCtx);
      if (r->OnClose)
        r->OnClose(r->callbackCtx);
      return 1;
    }
    // CLOSE Button
    if (mouse.x >= SCREEN_WIDTH - S(105) && mouse.x <= SCREEN_WIDTH - S(15) &&
        mouse.y >= divY + S(5) && mouse.y <= divY + S(23)) {
      if (r->OnClose)
        r->OnClose(r->callbackCtx);
      return 1;
    }

    // List Item Selection & Action Clicking
    float rowH = S(28.0f);
    int visibleRows = (int)((divY - (TOP_BAR_H + tabH)) / rowH);
    for (int i = 0; i < visibleRows; i++) {
      int idx_f = r->State->Scroll + i;
      if (idx_f >= filteredCount)
        break;

      int idx = filteredIndices[idx_f];
      float ry = TOP_BAR_H + tabH + (i * rowH);
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
    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    float bottomH = S(28.0f);
    float divY = viewH - bottomH;
    float tabH = S(28.0f);
    float rowH = S(28.0f);
    int visibleRows = (int)((divY - (TOP_BAR_H + tabH)) / rowH);

    if (r->State->CursorPos < visibleRows - 1 &&
        r->State->Scroll + r->State->CursorPos < filteredCount - 1) {
      r->State->CursorPos++;
    } else if (r->State->Scroll + visibleRows < filteredCount) {
      r->State->Scroll++;
    }
  }

  int idx_f = r->State->Scroll + r->State->CursorPos;
  if (idx_f < filteredCount) {
    int idx = filteredIndices[idx_f];
    SettingItem *item = &r->State->Items[idx];
    if (item->Type == SETTING_TYPE_LIST) {
      if (IsKeyPressed(KEY_LEFT)) {
        if (item->Current > 0)
          item->Current--;
        else
          item->Current = item->OptionsCount - 1;
        if (r->OnValueChanged) r->OnValueChanged(r->callbackCtx, idx);
        if (r->OnApply) r->OnApply(r->callbackCtx);
      }
      if (IsKeyPressed(KEY_RIGHT)) {
        if (item->Current < item->OptionsCount - 1)
          item->Current++;
        else
          item->Current = 0;
        if (r->OnValueChanged) r->OnValueChanged(r->callbackCtx, idx);
        if (r->OnApply) r->OnApply(r->callbackCtx);
      }
      if (IsKeyPressed(KEY_ENTER)) {
          r->State->IsDropdownOpen = true;
          r->State->DropdownItemIdx = idx;
          r->State->DropdownScroll = 0;
          return 0;
      }
    } else if (item->Type == SETTING_TYPE_KNOB) {
      float step = (item->Step > 0) ? item->Step : (item->Max - item->Min) / 20.0f;
      
      // Discrete step on press
      if (IsKeyPressed(KEY_LEFT)) {
        item->Value -= step;
        if (item->Value < item->Min) item->Value = item->Min;
        if (r->OnApply) r->OnApply(r->callbackCtx);
      }
      if (IsKeyPressed(KEY_RIGHT)) {
        item->Value += step;
        if (item->Value > item->Max) item->Value = item->Max;
        if (r->OnApply) r->OnApply(r->callbackCtx);
      }

      // Smooth step on hold
      if (IsKeyDown(KEY_LEFT)) {
        item->Value -= step * GetFrameTime() * 5.0f;
        if (item->Value < item->Min)
          item->Value = item->Min;
      }
      if (IsKeyDown(KEY_RIGHT)) {
        item->Value += step * GetFrameTime() * 5.0f;
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

  // Horizontal Tab Switch with Page Up/Down or similar if needed?
  // User might like Tab key to cycle tabs
  if (IsKeyPressed(KEY_TAB)) {
      r->State->SelectedTab = (r->State->SelectedTab + 1) % SETTING_CAT_COUNT;
      r->State->Scroll = 0;
      r->State->CursorPos = 0;
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

  float bottomH = S(28.0f);
  float divY = viewH - bottomH;
  float tabH = S(28.0f);
  float rowH = S(28.0f);

  // Draw Tabs
  DrawRectangle(0, TOP_BAR_H, SCREEN_WIDTH, tabH, ColorDark2);
  const char *tabs[] = { "DECK", "AUDIO", "VIEW", "SYSTEM" };
  float tabW = SCREEN_WIDTH / (float)SETTING_CAT_COUNT;
  for (int i = 0; i < SETTING_CAT_COUNT; i++) {
      Rectangle tRect = { i * tabW, TOP_BAR_H, tabW, tabH };
      if (r->State->SelectedTab == i) {
          DrawRectangleRec(tRect, ColorOrange);
          DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8), S(11), ColorBlack);
      } else {
          DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8), S(11), ColorGray);
      }
      DrawLine(i * tabW, TOP_BAR_H, i * tabW, TOP_BAR_H + tabH, ColorShadow);
  }
  DrawLine(0, TOP_BAR_H + tabH, SCREEN_WIDTH, TOP_BAR_H + tabH, ColorOrange);

  // Get filtered indices
  int filteredIndices[MAX_SETTINGS_ITEMS];
  int filteredCount = 0;
  for (int i = 0; i < r->State->ItemsCount; i++) {
    if (r->State->Items[i].Category == (SettingCategory)r->State->SelectedTab) {
      filteredIndices[filteredCount++] = i;
    }
  }

  int visibleRows = (int)((divY - (TOP_BAR_H + tabH)) / rowH);

  for (int i = 0; i < visibleRows; i++) {
    int idx_f = r->State->Scroll + i;
    if (idx_f >= filteredCount)
      break;

    int idx = filteredIndices[idx_f];
    SettingItem *item = &r->State->Items[idx];
    float ry = TOP_BAR_H + tabH + (i * rowH);

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
        DrawSelectionTriangleEx(SCREEN_WIDTH - S(150), ry + S(9), S(8), 1, ColorShadow); // Left
        DrawSelectionTriangleEx(SCREEN_WIDTH - S(18), ry + S(9), S(8), 0, ColorShadow); // Right
      }
    } else if (item->Type == SETTING_TYPE_KNOB) {
      UIDrawKnob(SCREEN_WIDTH - S(80), ry + (rowH / 2.0f), S(9), item->Value,
                 item->Min, item->Max, item->Unit, ColorOrange, false);
                 
      char valBuf[16];
      if (item->Value == (int)item->Value) sprintf(valBuf, "%d", (int)item->Value);
      else sprintf(valBuf, "%.1f", item->Value);
      
      UIDrawText(valBuf, faceMd, SCREEN_WIDTH - S(65), ry + S(6), S(13), ColorOrange);
    } else if (item->Type == SETTING_TYPE_ACTION) {
      UIDrawText("\uf2f5", faceSm, SCREEN_WIDTH - S(30), ry + S(6), S(12),
                 ColorOrange);
    }
  }

  DrawScrollbar(SCREEN_WIDTH - S(2.5f), TOP_BAR_H + tabH, S(2), divY - (TOP_BAR_H + tabH),
                filteredCount, r->State->Scroll, visibleRows);

  // Bottom Background
  DrawRectangle(0, divY, SCREEN_WIDTH, bottomH, ColorDark1);
  DrawLine(0, divY, SCREEN_WIDTH, divY, ColorGray);

  // DONE Button (Blue)
  DrawRectangle(S(15), divY + S(5), S(90), S(18), ColorBlue);
  DrawRectangleLines(S(15), divY + S(5), S(90), S(18), ColorShadow);
  DrawCentredText("DONE", faceSm, S(15), S(90), divY + S(8), S(11), ColorWhite);

  char countStr[32];
  sprintf(countStr, "%d / %d", r->State->Scroll + r->State->CursorPos + 1,
          filteredCount);
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
      
      BeginScissorMode((int)dropRect.x, (int)dropRect.y, (int)dropRect.width, (int)dropRect.height);
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
          DrawRectangle((int)(dropdownX + dropdownW - S(4)), (int)sbY, (int)S(4), (int)sbH, ColorOrange);
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
