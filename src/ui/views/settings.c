#include "ui/views/settings.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "core/midi/midi_handler.h"


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
  
  if (r->State->IsLearningMidi) {
      uint8_t status, midino;
      if (MIDI_GetLastMessage(&status, &midino)) {
          int idx = r->State->LearningItemIdx;
          MidiMapping *map = MIDI_GetGlobalMapping();
          // The MIDI items in Settings start from index 18 (after PRESET item at 17)
          int mapIdx = idx - 18;
          if (mapIdx >= 0 && mapIdx < map->count) {
              map->entries[mapIdx].status = status;
              map->entries[mapIdx].midino = midino;
              // Update display unit
              snprintf(r->State->Items[idx].Unit, 16, "0x%02X:0x%02X", status, midino);
          }
          r->State->IsLearningMidi = false;
      }
      if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
          r->State->IsLearningMidi = false;
      }
      return 1;
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
        } else if (clickedItem->Category == SETTING_CAT_CONTROLLERS && idx >= 18) {
            r->State->IsLearningMidi = true;
            r->State->LearningItemIdx = idx;
            // Flush any old messages
            uint8_t s, m;
            while(MIDI_GetLastMessage(&s, &m)); 
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
  float rowH = S(32.0f); // Slightly taller rows

  // Draw Tabs
  DrawRectangle(0, TOP_BAR_H, SCREEN_WIDTH, tabH, ColorDark3);
  const char *tabs[] = { "DECK", "AUDIO", "VIEW", "SYSTEM", "CONTROLLERS" };
  float tabW = SCREEN_WIDTH / (float)SETTING_CAT_COUNT;
  for (int i = 0; i < SETTING_CAT_COUNT; i++) {
      Rectangle tRect = { i * tabW, TOP_BAR_H, tabW, tabH };
      if (r->State->SelectedTab == i) {
          DrawRectangleRec(tRect, ColorOrange);
          DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8), S(11), ColorBlack);
          DrawRectangle(i * tabW, TOP_BAR_H + tabH - S(2), tabW, S(2), ColorWhite);
      } else {
          DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8), S(11), ColorGray);
      }
      if (i > 0) DrawLine(i * tabW, TOP_BAR_H + S(4), i * tabW, TOP_BAR_H + tabH - S(4), ColorGray);
  }
  DrawLine(0, TOP_BAR_H + tabH, SCREEN_WIDTH, TOP_BAR_H + tabH, ColorOrange);

  // Table Header for Controllers
  float listY = TOP_BAR_H + tabH;
  if (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) {
      float headH = S(20);
      DrawRectangle(0, listY, SCREEN_WIDTH, headH, ColorDark2);
      UIDrawText("CONTROL / TARGET", faceSm, S(32), listY + S(4), S(10), ColorGray);
      UIDrawText("CHANNEL : MSG", faceSm, SCREEN_WIDTH - S(150), listY + S(4), S(10), ColorGray);
      UIDrawText("TYPE", faceSm, SCREEN_WIDTH - S(60), listY + S(4), S(10), ColorGray);
      DrawLine(0, listY + headH, SCREEN_WIDTH, listY + headH, ColorGray);
      listY += headH;
  }

  // Get filtered indices
  int filteredIndices[MAX_SETTINGS_ITEMS];
  int filteredCount = 0;
  for (int i = 0; i < r->State->ItemsCount; i++) {
    if (r->State->Items[i].Category == (SettingCategory)r->State->SelectedTab) {
      filteredIndices[filteredCount++] = i;
    }
  }

  int visibleRows = (int)((divY - listY) / rowH);

  // Layout params
  float labelX = S(20);
  float valueWidth = S(180);
  float valueX = SCREEN_WIDTH - valueWidth - S(20);

  BeginScissorMode(0, (int)listY, (int)SCREEN_WIDTH, (int)(divY - listY));

  for (int i = 0; i < visibleRows; i++) {
    int idx_f = r->State->Scroll + i;
    if (idx_f >= filteredCount)
      break;

    int idx = filteredIndices[idx_f];
    SettingItem *item = &r->State->Items[idx];
    float ry = listY + (i * rowH);

    bool selected = (i == r->State->CursorPos);
    if (selected) {
      DrawRectangle(S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4), ColorGray);
      DrawSelectionTriangle(S(12), ry + (rowH / 2.0f), ColorOrange);
    } else if (idx_f % 2 == 0) {
      DrawRectangle(S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4), ColorDark1);
    }

    // Label with clipping
    float labelLimit = valueX - S(10);
    BeginScissorMode((int)labelX, (int)ry, (int)(labelLimit - labelX), (int)rowH);
    UIDrawText(item->Label, faceMd, labelX + S(12), ry + (rowH / 2.0f) - S(7), S(13), ColorWhite);
    EndScissorMode();

    if (item->Type == SETTING_TYPE_LIST) {
      const char *valStr = "";
      if (item->Current < item->OptionsCount)
        valStr = item->Options[item->Current];

      float innerValX = valueX + S(25);
      float innerValW = valueWidth - S(50);

      if (selected && item->OptionsCount > 1) {
          DrawSelectionTriangleEx(valueX + S(10), ry + (rowH / 2.0f), S(7), 1, ColorOrange);
          DrawSelectionTriangleEx(valueX + valueWidth - S(10), ry + (rowH / 2.0f), S(7), 0, ColorOrange);
      }

      if (item->Category == SETTING_CAT_CONTROLLERS) {
          // Highlight the preset selection row more prominently
          DrawRectangle(valueX, ry + S(4), valueWidth, rowH - S(8), ColorDark2);
          DrawCentredText(valStr, faceMd, valueX, valueWidth, ry + (rowH / 2.0f) - S(7), S(12), ColorOrange);
      } else {
          BeginScissorMode((int)innerValX, (int)ry, (int)innerValW, (int)rowH);
          DrawCentredText(valStr, faceMd, innerValX, innerValW, ry + (rowH / 2.0f) - S(7), S(13), ColorOrange);
          EndScissorMode();
      }

    } else if (item->Type == SETTING_TYPE_KNOB) {
      UIDrawKnob(valueX + valueWidth - S(95), ry + (rowH / 2.0f), S(9), item->Value,
                 item->Min, item->Max, NULL, ColorOrange, false);
                 
      char valBuf[32];
      if (item->Value == (int)item->Value) sprintf(valBuf, "%d", (int)item->Value);
      else sprintf(valBuf, "%.1f", item->Value);
      
      if (item->Unit[0] != '\0') {
        char fullBuf[64];
        sprintf(fullBuf, "%s %s", valBuf, item->Unit);
        UIDrawText(fullBuf, faceMd, valueX + valueWidth - S(80), ry + (rowH / 2.0f) - S(7), S(13), ColorOrange);
      } else {
        UIDrawText(valBuf, faceMd, valueX + valueWidth - S(80), ry + (rowH / 2.0f) - S(7), S(13), ColorOrange);
      }
    } else if (item->Type == SETTING_TYPE_ACTION) {
      if (item->Category == SETTING_CAT_CONTROLLERS) {
          // Table Layout for Controllers with column lines
          if (idx >= 18) { // Mapping entries
              // Column Dividers
              DrawLine(SCREEN_WIDTH - S(160), ry + S(2), SCREEN_WIDTH - S(160), ry + rowH - S(2), ColorGray);
              DrawLine(SCREEN_WIDTH - S(75), ry + S(2), SCREEN_WIDTH - S(75), ry + rowH - S(2), ColorGray);

              // Channel:Msg as a "badge"
              Rectangle badgeRect = { SCREEN_WIDTH - S(155), ry + S(6), S(75), rowH - S(12) };
              DrawRectangleRounded(badgeRect, 0.5f, 4, ColorDark2);
              DrawCentredText(item->Unit, faceSm, badgeRect.x, badgeRect.width, ry + (rowH / 2.0f) - S(6), S(10), ColorOrange);

              // Type Badge
              Color typeColor = ColorGray;
              const char *typeIcon = "";
              if (strcmp(item->Options[0], "SCRIPT") == 0) { typeColor = ColorBlue; typeIcon = "\uf121"; }
              else if (strcmp(item->Options[0], "REL") == 0) { typeColor = ColorOrange; typeIcon = "\uf01e"; }
              else if (strcmp(item->Options[0], "14BIT") == 0) { typeColor = ColorDGreen; typeIcon = "\uf0c9"; }

              UIDrawText(typeIcon, faceSm, SCREEN_WIDTH - S(68), ry + (rowH / 2.0f) - S(6), S(10), typeColor);
              UIDrawText(item->Options[0], faceSm, SCREEN_WIDTH - S(52), ry + (rowH / 2.0f) - S(6), S(9), typeColor);

              if (r->State->IsLearningMidi && r->State->LearningItemIdx == idx) {
                  DrawRectangleLinesEx((Rectangle){S(6), ry + S(3), SCREEN_WIDTH - S(12), rowH - S(6)}, 2.0f, ColorBlue);
                  UIDrawText("WAITING...", faceSm, SCREEN_WIDTH - S(155), ry + (rowH / 2.0f) - S(6), S(10), ColorBlue);
              }

          } else { // Preset Selection
              UIDrawText(item->Unit, faceMd, valueX + valueWidth - S(90), ry + (rowH / 2.0f) - S(7), S(13), ColorOrange);
          }
          UIDrawText("\uf35a", faceSm, valueX + valueWidth - S(25), ry + (rowH / 2.0f) - S(6), S(12), ColorGray);
      } else if (strcmp(item->Label, "ABOUT") != 0 && strcmp(item->Label, "EXIT APPLICATION") != 0) {
        UIDrawText("\uf2f5", faceSm, valueX + valueWidth - S(35), ry + (rowH / 2.0f) - S(6), S(12), ColorOrange);
      }
    }
  }

  EndScissorMode();

  DrawScrollbar(SCREEN_WIDTH - S(2.5f), listY, S(2), divY - listY,
                filteredCount, r->State->Scroll, visibleRows);

  // Bottom Background
  DrawRectangle(0, divY, SCREEN_WIDTH, bottomH, ColorDark1);
  DrawLine(0, divY, SCREEN_WIDTH, divY, ColorGray);

  // DONE / CLOSE
  DrawRectangle(S(15), divY + S(5), S(90), S(18), ColorBlue);
  DrawCentredText("DONE", faceSm, S(15), S(90), divY + S(8), S(11), ColorWhite);

  char countStr[32];
  sprintf(countStr, "%d / %d", r->State->Scroll + r->State->CursorPos + 1, filteredCount);
  UIDrawText(countStr, faceXS, SCREEN_WIDTH / 2.0f - S(24.0f), divY + S(8), S(9), ColorShadow);

  DrawRectangle(SCREEN_WIDTH - S(105), divY + S(5), S(90), S(18), ColorDark2);
  DrawCentredText("CLOSE", faceSm, SCREEN_WIDTH - S(105), S(90), divY + S(8), S(11), ColorWhite);

  if (r->State->IsDropdownOpen) {
      DrawRectangle(0, 0, SCREEN_WIDTH, viewH, (Color){ 0, 0, 0, 200 });
      SettingItem *item = &r->State->Items[r->State->DropdownItemIdx];
      float dropdownW = S(240.0f);
      float opHeight = S(40.0f);
      float contentH = item->OptionsCount * opHeight;
      float dropdownH = contentH > (viewH * 0.7f) ? (viewH * 0.7f) : contentH;
      float dropdownX = (SCREEN_WIDTH - dropdownW) / 2.0f;
      float dropdownY = (viewH - dropdownH) / 2.0f;
      
      Rectangle dropRect = { dropdownX, dropdownY, dropdownW, dropdownH };
      BeginScissorMode((int)dropRect.x, (int)dropRect.y, (int)dropRect.width, (int)dropRect.height);
      DrawRectangleRec(dropRect, ColorBGUtil);
      float cy = dropdownY - r->State->DropdownScroll;
      for(int i=0; i<item->OptionsCount; i++) {
          Rectangle opRect = { dropdownX, cy, dropdownW, opHeight };
          if (cy + opHeight > dropdownY && cy < dropdownY + dropdownH) {
              if (item->Current == i) DrawRectangleRec(opRect, ColorGray);
              else DrawRectangleRec(opRect, ColorDark1);
              DrawRectangleLinesEx(opRect, 1, ColorShadow);
              UIDrawText(item->Options[i], faceMd, dropdownX + S(20), cy + S(12), S(15), (item->Current == i) ? ColorOrange : ColorWhite);
          }
          cy += opHeight;
      }
      EndScissorMode();
      DrawRectangleLinesEx(dropRect, 2, ColorOrange);
      if (contentH > dropdownH) {
          float sbY = dropdownY + (r->State->DropdownScroll / contentH) * dropdownH;
          float sbH = (dropdownH / contentH) * dropdownH;
          DrawRectangle((int)(dropdownX + dropdownW - S(4)), (int)sbY, (int)S(4), (int)sbH, ColorOrange);
      }
  }
  
  if (r->State->IsLearningMidi) {
      DrawRectangle(0, 0, SCREEN_WIDTH, viewH, (Color){ 0, 0, 0, 200 });
      DrawCentredText("MIDI LEARN", faceMd, 0, SCREEN_WIDTH, viewH/2.0f - S(30), S(20), ColorOrange);
      DrawCentredText("Move a controller or press a button...", faceSm, 0, SCREEN_WIDTH, viewH/2.0f + S(10), S(12), ColorWhite);
      DrawCentredText("ESC to cancel", faceXS, 0, SCREEN_WIDTH, viewH/2.0f + S(40), S(9), ColorGray);
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
