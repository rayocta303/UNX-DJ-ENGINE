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
      if (UI_IsDown()) {
          r->State->DropdownScroll -= mouseReq.y;
      }
      
      float maxScroll = contentH - dropdownH;
      if (maxScroll < 0) maxScroll = 0;
      if (r->State->DropdownScroll < 0) r->State->DropdownScroll = 0;
      if (r->State->DropdownScroll > maxScroll) r->State->DropdownScroll = maxScroll;
      
      if (UI_IsReleased()) {
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
          // Mappings start at index MIDI_MAPPING_START_IDX in PopulateMidiSettings
          int mapIdx = idx - MIDI_MAPPING_START_IDX;
          if (mapIdx >= 0 && mapIdx < map->count) {
              map->entries[mapIdx].status = status;
              map->entries[mapIdx].midino = midino;
              snprintf(r->State->Items[idx].Unit, 16, "0x%02X:0x%02X", status, midino);
              if (r->OnApply) r->OnApply(r->callbackCtx);
          }
          r->State->IsLearningMidi = false;
      }
      if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
          r->State->IsLearningMidi = false;
      }
      return 1;
  }

  if (r->State->IsEditMappingOpen) {
      SettingItem *item = &r->State->Items[r->State->EditMappingItemIdx];
      MidiMapping *map = MIDI_GetGlobalMapping();
      int mapIdx = r->State->EditMappingItemIdx - MIDI_MAPPING_START_IDX;

      float winH = SCREEN_HEIGHT - DECK_STR_H;
      float winW = SCREEN_WIDTH;
      float modalW = S(320.0f);
      float modalH = S(185.0f);
      float modalX = (winW - modalW) / 2.0f;
      float modalY = (winH - modalH) / 2.0f;

      // Modal Interactive Buttons Hitboxes
      Rectangle btn1 = { modalX + S(15), modalY + S(105), S(135), S(24) };
      Rectangle btn2 = { modalX + S(170), modalY + S(105), S(135), S(24) };
      Rectangle btn3 = { modalX + S(15), modalY + S(140), S(135), S(24) };
      Rectangle btn4 = { modalX + S(170), modalY + S(140), S(135), S(24) };

      if (UI_IsReleased()) {
          Vector2 mousePos = UIGetMousePosition();
          if (CheckCollisionPointRec(mousePos, btn1)) {
              r->State->IsLearningMidi = true;
              r->State->LearningItemIdx = r->State->EditMappingItemIdx;
              uint8_t s, m;
              while(MIDI_GetLastMessage(&s, &m)); // Flush
          } else if (CheckCollisionPointRec(mousePos, btn2)) {
              if (mapIdx >= 0 && mapIdx < map->count) {
                  if (strcmp(item->Options[0], "SCRIPT") == 0) {
                      strcpy(item->Options[0], "REL");
                      map->entries[mapIdx].options = 1; // RELATIVE
                  } else if (strcmp(item->Options[0], "REL") == 0) {
                      strcpy(item->Options[0], "14BIT");
                      map->entries[mapIdx].options = 8; // 14BIT
                  } else {
                      strcpy(item->Options[0], "SCRIPT");
                      map->entries[mapIdx].options = 4; // SCRIPT
                  }
                  if (r->OnApply) r->OnApply(r->callbackCtx);
              }
          } else if (CheckCollisionPointRec(mousePos, btn3)) {
              if (mapIdx >= 0 && mapIdx < map->count) {
                  map->entries[mapIdx].status = 0;
                  map->entries[mapIdx].midino = 0;
                  strcpy(item->Unit, "0x00:0x00");
                  if (r->OnApply) r->OnApply(r->callbackCtx);
              }
          } else if (CheckCollisionPointRec(mousePos, btn4)) {
              r->State->IsEditMappingOpen = false;
          }
      }

      if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ENTER)) {
          r->State->IsEditMappingOpen = false;
      }
      return 1;
  }

  if (r->State->IsMappingListOpen) {
      // Gather all actual mapping items (indices >= 22)
      int mapIndices[MAX_SETTINGS_ITEMS];
      int mapCount = 0;
      for (int i = 22; i < r->State->ItemsCount; i++) {
          if (r->State->Items[i].Category == SETTING_CAT_CONTROLLERS) {
              mapIndices[mapCount++] = i;
          }
      }
      
      float viewH = SCREEN_HEIGHT - DECK_STR_H;
      float listY = TOP_BAR_H + S(28.0f);
      float rowH = S(32.0f);
      float bottomH = S(46.0f);
      float divY = viewH - bottomH;
      int visibleRows = (int)((divY - listY) / rowH);

      // Mouse click list item inside mapping list sub-window
      if (UI_IsReleased()) {
          Vector2 mousePos = UIGetMousePosition();
          
          // Back Button at the bottom
          Rectangle backRect = { S(15), divY + S(23), S(90), S(18) };
          if (CheckCollisionPointRec(mousePos, backRect)) {
              r->State->IsMappingListOpen = false;
              return 1;
          }
          
          // CREATE NEW Button
          Rectangle createRect = { SCREEN_WIDTH - S(250), divY + S(23), S(110), S(18) };
          if (CheckCollisionPointRec(mousePos, createRect)) {
              if (r->OnAction) r->OnAction(r->callbackCtx, 20);
              return 1;
          }
          
          // SAVE AS CUSTOM Button
          Rectangle saveRect = { SCREEN_WIDTH - S(130), divY + S(23), S(115), S(18) };
          if (CheckCollisionPointRec(mousePos, saveRect)) {
              if (r->OnAction) r->OnAction(r->callbackCtx, 21);
              return 1;
          }
          
          // Check list clicks
          for (int i = 0; i < visibleRows; i++) {
              int idx_f = r->State->MappingListScroll + i;
              if (idx_f >= mapCount) break;
              
              float ry = listY + (i * rowH);
              Rectangle rowRect = { 0, ry, SCREEN_WIDTH, rowH };
              if (CheckCollisionPointRec(mousePos, rowRect)) {
                  r->State->MappingListCursorPos = i;
                  int actualIdx = mapIndices[idx_f];
                  r->State->IsEditMappingOpen = true;
                  r->State->EditMappingItemIdx = actualIdx;
                  return 1;
              }
          }
      }
      
      // Mouse Wheel Scroll inside sub-window
      float wheel = GetMouseWheelMove();
      if (wheel != 0) {
          if (wheel > 0) {
              if (r->State->MappingListScroll > 0) r->State->MappingListScroll--;
          } else {
              if (r->State->MappingListScroll + visibleRows < mapCount) {
                  r->State->MappingListScroll++;
              }
          }
      }
      
      // Keyboard Navigation inside sub-window
      if (IsKeyPressed(KEY_UP)) {
          if (r->State->MappingListCursorPos > 0) {
              r->State->MappingListCursorPos--;
          } else if (r->State->MappingListScroll > 0) {
              r->State->MappingListScroll--;
          }
      }
      if (IsKeyPressed(KEY_DOWN)) {
          if (r->State->MappingListCursorPos < visibleRows - 1 &&
              r->State->MappingListScroll + r->State->MappingListCursorPos < mapCount - 1) {
              r->State->MappingListCursorPos++;
          } else if (r->State->MappingListScroll + visibleRows < mapCount) {
              r->State->MappingListScroll++;
          }
      }
      if (IsKeyPressed(KEY_ENTER)) {
          int idx_f = r->State->MappingListScroll + r->State->MappingListCursorPos;
          if (idx_f >= 0 && idx_f < mapCount) {
              r->State->IsEditMappingOpen = true;
              r->State->EditMappingItemIdx = mapIndices[idx_f];
          }
          return 1;
      }
      if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
          r->State->IsMappingListOpen = false;
          return 1;
      }
      
      return 1; // block background interaction
  }

  // Handle MIDI navigation for Settings
  if (r->State->MidiBrowseDelta != 0) {
      r->State->CursorPos += r->State->MidiBrowseDelta;
      r->State->MidiBrowseDelta = 0;
      if (r->State->CursorPos < 0) r->State->CursorPos = 0;
      if (r->State->CursorPos >= r->State->ItemsCount) r->State->CursorPos = r->State->ItemsCount - 1;
  }
  if (r->State->MidiRequestEnter) {
      // Logic to trigger the action at CursorPos
      r->OnAction(r->callbackCtx, r->State->CursorPos);
      r->State->MidiRequestEnter = false;
  }

  // Get filtered indices (Hides individual mapping entries in main view)
  int filteredIndices[MAX_SETTINGS_ITEMS];
  int filteredCount = 0;
  for (int i = 0; i < r->State->ItemsCount; i++) {
    if (r->State->Items[i].Category == (SettingCategory)r->State->SelectedTab) {
      if (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) {
          if (strcmp(r->State->Items[i].Label, "CONNECTED DEVICE") == 0 || strcmp(r->State->Items[i].Label, "MAPPING PRESET") == 0) {
              filteredIndices[filteredCount++] = i;
          }
      } else {
          filteredIndices[filteredCount++] = i;
      }
    }
  }

  if (UI_IsDown()) {
    Vector2 delta = GetMouseDelta();
    r->State->TouchDragAccumulator += delta.y;
    
    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    float tabH = S(28.0f);
    float rowH = (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(38.0f) : S(32.0f);
    float bottomH = S(46.0f);
    float listY = TOP_BAR_H + tabH;
    int visibleRows = (int)((viewH - listY - bottomH) / rowH);
    
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

  // Calculate hovered item row
  Vector2 mousePos = UIGetMousePosition();
  float tabH = S(28.0f);
  float rowH = (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(38.0f) : S(32.0f);
  float listY = TOP_BAR_H + tabH;
  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  float bottomH = S(46.0f);
  float divY = viewH - bottomH;
  int visibleRows = (int)((divY - listY) / rowH);

  int hoveredItemIdx = -1;
  if (mousePos.y >= listY && mousePos.y < divY) {
      int rowIdx = (int)((mousePos.y - listY) / rowH);
      if (rowIdx >= 0 && rowIdx < visibleRows) {
          int idx_f = r->State->Scroll + rowIdx;
          if (idx_f < filteredCount) {
              hoveredItemIdx = filteredIndices[idx_f];
          }
      }
  }

  // Mouse Wheel & Click & Drag Knob Controls
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
      if (hoveredItemIdx >= 0 && r->State->Items[hoveredItemIdx].Type == SETTING_TYPE_KNOB) {
          SettingItem *kItem = &r->State->Items[hoveredItemIdx];
          float step = (kItem->Step > 0) ? kItem->Step : (kItem->Max - kItem->Min) / 50.0f;
          if (step < 1.0f && (kItem->Max - kItem->Min) > 50.0f) step = 10.0f;
          kItem->Value += (wheel > 0 ? step : -step);
          if (kItem->Value < kItem->Min) kItem->Value = kItem->Min;
          if (kItem->Value > kItem->Max) kItem->Value = kItem->Max;
          if (r->OnApply) r->OnApply(r->callbackCtx);
      } else {
          if (wheel > 0) {
              if (r->State->Scroll > 0) r->State->Scroll--;
          } else {
              if (r->State->Scroll + visibleRows < filteredCount) {
                  r->State->Scroll++;
              }
          }
      }
  }

  if (UI_IsDown()) {
      Vector2 mouseDelta = GetMouseDelta();
      if (hoveredItemIdx >= 0 && r->State->Items[hoveredItemIdx].Type == SETTING_TYPE_KNOB) {
          if (fabsf(mouseDelta.x) > 0.001f || fabsf(mouseDelta.y) > 0.001f) {
              SettingItem *kItem = &r->State->Items[hoveredItemIdx];
              float deltaVal = (mouseDelta.x - mouseDelta.y);
              float range = (kItem->Max - kItem->Min);
              float sensitivity = (range > 100.0f) ? (range / 150.0f) : (range / 80.0f);
              kItem->Value += deltaVal * sensitivity;
              if (kItem->Value < kItem->Min) kItem->Value = kItem->Min;
              if (kItem->Value > kItem->Max) kItem->Value = kItem->Max;
              if (r->OnApply) r->OnApply(r->callbackCtx);
          }
      }
  }

  if (UI_IsPressed()) {
      r->State->TouchDragAccumulator = 0;
  }

  if (UI_IsReleased()) {
    Vector2 mouse = UIGetMousePosition();
    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    float bottomH = S(46.0f);
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
    float rowH = (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(38.0f) : S(32.0f); // Match Settings_Draw
    float listY = TOP_BAR_H + tabH;

    int visibleRows = (int)((divY - listY) / rowH);
    for (int i = 0; i < visibleRows; i++) {
      int idx_f = r->State->Scroll + i;
      if (idx_f >= filteredCount)
        break;

      int idx = filteredIndices[idx_f];
      float ry = listY + (i * rowH);
      Rectangle rowRect = {0, ry, SCREEN_WIDTH, rowH};

      if (CheckCollisionPointRec(mouse, rowRect) && fabsf(r->State->TouchDragAccumulator) < 10.0f) {
        r->State->CursorPos = i; 
        
        SettingItem *clickedItem = &r->State->Items[idx];
        if (clickedItem->Category == SETTING_CAT_CONTROLLERS && idx >= MIDI_MAPPING_START_IDX) {
            r->State->IsEditMappingOpen = true;
            r->State->EditMappingItemIdx = idx;
            return 1;
        } else if (clickedItem->Type == SETTING_TYPE_ACTION) {
            if (idx == 20) {
                r->State->IsMappingListOpen = true;
                r->State->MappingListScroll = 0;
                r->State->MappingListCursorPos = 0;
                return 1;
            }
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
    float bottomH = S(46.0f);
    float divY = viewH - bottomH;
    float tabH = S(28.0f);
    float rowH = (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(38.0f) : S(32.0f); // Match Settings_Draw
    float listY = TOP_BAR_H + tabH;
    if (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) listY += S(20);
    int visibleRows = (int)((divY - listY) / rowH);

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
        if (item->Category == SETTING_CAT_CONTROLLERS && idx >= MIDI_MAPPING_START_IDX) {
            r->State->IsEditMappingOpen = true;
            r->State->EditMappingItemIdx = idx;
        } else if (idx == 20) {
            r->State->IsMappingListOpen = true;
            r->State->MappingListScroll = 0;
            r->State->MappingListCursorPos = 0;
        } else {
            if (r->OnAction)
              r->OnAction(r->callbackCtx, idx);
        }
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
  Font faceIcon = UIFonts_GetIcon(S(12));
  Font faceIconSm = UIFonts_GetIcon(S(10));

  float bottomH = S(46.0f);
  float divY = viewH - bottomH;
  float tabH = S(28.0f);
  float rowH = (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(38.0f) : S(32.0f);

  // Draw Tabs with state-of-the-art visual style
  DrawRectangle(0, TOP_BAR_H, SCREEN_WIDTH, tabH, ColorDark3);
  const char *tabs[] = { "DECK", "AUDIO", "VIEW", "SYSTEM", "CONTROLLERS" };
  float tabW = SCREEN_WIDTH / (float)SETTING_CAT_COUNT;
  for (int i = 0; i < SETTING_CAT_COUNT; i++) {
      Rectangle tRect = { i * tabW, TOP_BAR_H, tabW, tabH };
      if (r->State->SelectedTab == i) {
          DrawRectangleRec(tRect, (Color){ 255, 121, 0, 35 });
          DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8), S(11), ColorOrange);
          DrawRectangle(i * tabW, TOP_BAR_H + tabH - S(3), tabW, S(3), ColorOrange);
      } else {
          Vector2 mouse = UIGetMousePosition();
          if (CheckCollisionPointRec(mouse, tRect)) {
              DrawRectangleRec(tRect, (Color){ 255, 255, 255, 15 });
              DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8), S(11), ColorWhite);
          } else {
              DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8), S(11), ColorGray);
          }
      }
      if (i > 0) DrawLine(i * tabW, TOP_BAR_H + S(4), i * tabW, TOP_BAR_H + tabH - S(4), ColorGray);
  }
  DrawLine(0, TOP_BAR_H + tabH, SCREEN_WIDTH, TOP_BAR_H + tabH, ColorOrange);

  float listY = TOP_BAR_H + tabH;

  // Get filtered indices (Hides individual mapping entries in main view)
  int filteredIndices[MAX_SETTINGS_ITEMS];
  int filteredCount = 0;
  for (int i = 0; i < r->State->ItemsCount; i++) {
    if (r->State->Items[i].Category == (SettingCategory)r->State->SelectedTab) {
      if (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) {
          if (strcmp(r->State->Items[i].Label, "CONNECTED DEVICE") == 0 || strcmp(r->State->Items[i].Label, "MAPPING PRESET") == 0) {
              filteredIndices[filteredCount++] = i;
          }
      } else {
          filteredIndices[filteredCount++] = i;
      }
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
      DrawRectangleRounded((Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)}, 0.15f, 4, (Color){ 255, 121, 0, 45 });
      DrawRectangleRoundedLines((Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)}, 0.15f, 4, 1.0f, ColorOrange);
      DrawCircle(S(14), ry + (rowH / 2.0f), S(3.5f), ColorOrange);
    } else if (idx_f % 2 == 0) {
      DrawRectangleRounded((Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)}, 0.15f, 4, ColorDark1);
    } else {
      DrawRectangleRounded((Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)}, 0.15f, 4, (Color){18, 18, 18, 255});
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
          if (idx >= MIDI_MAPPING_START_IDX) { // Mapping entries
              // Column Dividers
              DrawLine(SCREEN_WIDTH - S(160), ry + S(2), SCREEN_WIDTH - S(160), ry + rowH - S(2), ColorGray);
              DrawLine(SCREEN_WIDTH - S(75), ry + S(2), SCREEN_WIDTH - S(75), ry + rowH - S(2), ColorGray);

                bool isLearning = (r->State->IsLearningMidi && r->State->LearningItemIdx == idx);

                if (isLearning) {
                    float alpha = (sinf(GetTime() * 12.0f) * 0.4f + 0.6f);
                    Color pulseColor = { 0, 121, 241, (unsigned char)(alpha * 255) };
                    DrawRectangleRounded((Rectangle){S(6), ry + S(3), SCREEN_WIDTH - S(12), rowH - S(6)}, 0.2f, 4, (Color){0, 40, 80, 200});
                    DrawRectangleRoundedLines((Rectangle){S(6), ry + S(3), SCREEN_WIDTH - S(12), rowH - S(6)}, 0.2f, 4, 2.0f, pulseColor);
                    DrawCentredText("WAITING FOR MIDI INPUT...", faceSm, 0, SCREEN_WIDTH, ry + (rowH / 2.0f) - S(6), S(11), pulseColor);
                } else {
                    // Channel:Msg as a "badge"
                    Rectangle badgeRect = { SCREEN_WIDTH - S(160) + S(10), ry + (rowH - S(20)) / 2.0f, S(65), S(20) };
                    DrawRectangleRounded(badgeRect, 0.4f, 4, ColorDark2);
                    DrawRectangleRoundedLines(badgeRect, 0.4f, 4, 1.0f, ColorGray);
                    DrawCentredText(item->Unit, faceSm, badgeRect.x, badgeRect.width, ry + (rowH / 2.0f) - S(6), S(10), ColorOrange);
                    
                    // Type Badge
                    Color typeColor = ColorGray;
                    const char *typeIcon = "";
                    if (strcmp(item->Options[0], "SCRIPT") == 0) { typeColor = ColorBlue; typeIcon = "\uf121"; }
                    else if (strcmp(item->Options[0], "REL") == 0) { typeColor = ColorOrange; typeIcon = "\uf01e"; }
                    else if (strcmp(item->Options[0], "14BIT") == 0) { typeColor = ColorDGreen; typeIcon = "\uf0c9"; }

                    // Type Icon & Text
                    UIDrawText(typeIcon, faceIconSm, SCREEN_WIDTH - S(72), ry + (rowH / 2.0f) - S(5), S(10), typeColor);
                    UIDrawText(item->Options[0], faceSm, SCREEN_WIDTH - S(55), ry + (rowH / 2.0f) - S(6), S(9), typeColor);
                }

           } else { // Preset Selection Actions (CREATE/SAVE)
               UIDrawText(item->Unit, faceMd, valueX + valueWidth - S(90), ry + (rowH / 2.0f) - S(7), S(13), ColorOrange);
               UIDrawText("\uf35a", faceIcon, valueX + valueWidth - S(25), ry + (rowH / 2.0f) - S(6), S(12), ColorGray);
           }
      } else if (strcmp(item->Label, "ABOUT") != 0 && 
                 strcmp(item->Label, "CREDITS") != 0 &&
                 strcmp(item->Label, "EXIT APPLICATION") != 0) {
        UIDrawText("\uf2f5", faceIcon, valueX + valueWidth - S(35), ry + (rowH / 2.0f) - S(6), S(12), ColorOrange);
      }
    }
  }

  EndScissorMode();

  DrawScrollbar(SCREEN_WIDTH - S(2.5f), listY, S(2), divY - listY,
                filteredCount, r->State->Scroll, visibleRows);

  // Bottom Background
  DrawRectangle(0, divY, SCREEN_WIDTH, bottomH, ColorDark1);
  DrawLine(0, divY, SCREEN_WIDTH, divY, ColorGray);

  // Selected Item Details Banner (Key, Value / Type)
  int idx_f = r->State->Scroll + r->State->CursorPos;
  SettingItem *selectedItem = NULL;
  if (idx_f >= 0 && idx_f < filteredCount) {
      selectedItem = &r->State->Items[filteredIndices[idx_f]];
  }
  char detailBuf[128] = "";
  if (selectedItem) {
      if (selectedItem->Category == SETTING_CAT_CONTROLLERS) {
          int actualIdx = filteredIndices[idx_f];
          if (actualIdx >= MIDI_MAPPING_START_IDX) {
              const char *mapType = (selectedItem->OptionsCount > 0) ? selectedItem->Options[0] : "UNKNOWN";
              const char *mapAddress = (selectedItem->Unit[0] != '\0') ? selectedItem->Unit : "UNMAPPED";
              snprintf(detailBuf, sizeof(detailBuf), "Selected Target: %s  |  MIDI Address: %s  |  Type: %s", 
                       selectedItem->Label, mapAddress, mapType);
          } else {
              snprintf(detailBuf, sizeof(detailBuf), "Selected Action: %s", selectedItem->Label);
          }
      } else {
          if (selectedItem->Type == SETTING_TYPE_LIST) {
              const char *valStr = (selectedItem->Current < selectedItem->OptionsCount) ? selectedItem->Options[selectedItem->Current] : "";
              snprintf(detailBuf, sizeof(detailBuf), "Selected: %s  |  Value: %s  |  Type: LIST", selectedItem->Label, valStr);
          } else if (selectedItem->Type == SETTING_TYPE_KNOB) {
              char valBuf[32];
              if (selectedItem->Value == (int)selectedItem->Value) sprintf(valBuf, "%d", (int)selectedItem->Value);
              else sprintf(valBuf, "%.2f", selectedItem->Value);
              if (selectedItem->Unit[0] != '\0') {
                  snprintf(detailBuf, sizeof(detailBuf), "Selected: %s  |  Value: %s %s  |  Type: KNOB/SLIDER", selectedItem->Label, valBuf, selectedItem->Unit);
              } else {
                  snprintf(detailBuf, sizeof(detailBuf), "Selected: %s  |  Value: %s  |  Type: KNOB/SLIDER", selectedItem->Label, valBuf);
              }
          } else {
              snprintf(detailBuf, sizeof(detailBuf), "Selected Action: %s  |  Type: ACTION", selectedItem->Label);
          }
      }
  }

  // Draw Details Strip
  DrawRectangle(0, divY, SCREEN_WIDTH, S(18), ColorDark2);
  UIDrawText(detailBuf[0] != '\0' ? detailBuf : "No item selected", faceXS, S(15), divY + S(5), S(9.5f), ColorOrange);
  DrawLine(0, divY + S(18), SCREEN_WIDTH, divY + S(18), ColorGray);

  // DONE / CLOSE Buttons (Offset by details strip height)
  Rectangle doneRect = { S(15), divY + S(23), S(90), S(18) };
  DrawRectangleRounded(doneRect, 0.5f, 4, ColorBlue);
  DrawRectangleRoundedLines(doneRect, 0.5f, 4, 1.0f, ColorWhite);
  DrawCentredText("DONE", faceSm, doneRect.x, doneRect.width, divY + S(26), S(11), ColorWhite);

  char countStr[32];
  sprintf(countStr, "%d / %d", r->State->Scroll + r->State->CursorPos + 1, filteredCount);
  UIDrawText(countStr, faceXS, SCREEN_WIDTH / 2.0f - S(24.0f), divY + S(26), S(9), ColorGray);

  Rectangle closeRect = { SCREEN_WIDTH - S(105), divY + S(23), S(90), S(18) };
  DrawRectangleRounded(closeRect, 0.5f, 4, ColorDark2);
  DrawRectangleRoundedLines(closeRect, 0.5f, 4, 1.0f, ColorGray);
  DrawCentredText("CLOSE", faceSm, closeRect.x, closeRect.width, divY + S(26), S(11), ColorWhite);

  // MIDI Monitor & Tip Label
  if (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) {
      uint8_t s, m;
      Rectangle monRect = { S(115), divY + S(24), S(135), S(16) };
      DrawRectangleRounded(monRect, 0.2f, 4, ColorBlack);
      DrawRectangleRoundedLines(monRect, 0.2f, 4, 1.0f, ColorDark2);
      
      if (MIDI_PeekLastMessage(&s, &m)) {
          char monBuf[64];
          snprintf(monBuf, 64, "MIDI: 0x%02X : 0x%02X", s, m);
          UIDrawText(monBuf, faceXS, monRect.x + S(18), divY + S(27), S(9), ColorDGreen);
          DrawCircle(monRect.x + S(10), divY + S(32), S(3), ColorDGreen);
      } else {
          UIDrawText("MIDI IDLE", faceXS, monRect.x + S(18), divY + S(27), S(9), ColorDark2);
          DrawCircle(monRect.x + S(10), divY + S(32), S(3), ColorDark2);
      }
      
      UIDrawText("Hardware Auto-Mapping Active", faceXS, S(265), divY + S(27), S(9), ColorGray);
  }

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
      // Dark glassmorphic backdrop
      DrawRectangle(0, 0, SCREEN_WIDTH, viewH, (Color){ 10, 10, 15, 230 });
      
      // Dialog Card layout
      float cardW = S(320);
      float cardH = S(180);
      Rectangle cardRect = { (SCREEN_WIDTH - cardW) / 2.0f, (viewH - cardH) / 2.0f, cardW, cardH };
      
      // Neon orange border glow
      DrawRectangleRounded((Rectangle){ cardRect.x - S(2), cardRect.y - S(2), cardRect.width + S(4), cardRect.height + S(4) }, 0.1f, 4, (Color){ 255, 121, 0, 80 });
      DrawRectangleRounded(cardRect, 0.1f, 4, ColorBGUtil);
      DrawRectangleRoundedLines(cardRect, 0.1f, 4, 1.5f, ColorOrange);
      
      // Bouncing MIDI connection icon
      float bounce = sinf(GetTime() * 8.0f) * S(4.0f);
      UIDrawText("\uf121", UIFonts_GetIcon(S(28)), cardRect.x + cardW / 2.0f - S(14), cardRect.y + S(20) + bounce, S(28), ColorOrange);
      
      // Texts
      DrawCentredText("MIDI LEARN ACTIVE", faceMd, cardRect.x, cardRect.width, cardRect.y + S(65), S(14), ColorWhite);
      DrawCentredText("Move a knob, fader, or press a pad...", faceSm, cardRect.x, cardRect.width, cardRect.y + S(95), S(11), ColorGray);
      
      // Bouncing signal pulse status
      float alpha = (sinf(GetTime() * 10.0f) * 0.3f + 0.7f);
      Color pulseColor = { 0, 220, 100, (unsigned char)(alpha * 255) };
      DrawCentredText("LISTENING FOR MIDI EVENT...", faceXS, cardRect.x, cardRect.width, cardRect.y + S(125), S(10), pulseColor);
      
      DrawCentredText("Press [ESC] to cancel", faceXS, cardRect.x, cardRect.width, cardRect.y + S(150), S(9), ColorGray);
  }

  if (r->State->IsMappingListOpen) {
      // Gather all mapping items
      int mapIndices[MAX_SETTINGS_ITEMS];
      int mapCount = 0;
      for (int i = MIDI_MAPPING_START_IDX; i < r->State->ItemsCount; i++) {
          if (r->State->Items[i].Category == SETTING_CAT_CONTROLLERS) {
              mapIndices[mapCount++] = i;
          }
      }
      
      // Draw Window Backdrop
      DrawRectangle(0, 0, SCREEN_WIDTH, viewH, ColorBGUtil);
      
      // Draw Window Title
      DrawRectangle(0, TOP_BAR_H, SCREEN_WIDTH, S(28.0f), ColorDark3);
      UIDrawText("EDIT PRESET MAPPINGS", faceSm, S(20), TOP_BAR_H + S(8), S(11), ColorOrange);
      DrawLine(0, TOP_BAR_H + S(28.0f), SCREEN_WIDTH, TOP_BAR_H + S(28.0f), ColorOrange);
      
      float listY = TOP_BAR_H + S(28.0f);
      float rowH = S(32.0f);
      int visibleRows = (int)((divY - listY) / rowH);
      
      // Render mapping items in a beautiful list without table head
      BeginScissorMode(0, (int)listY, (int)SCREEN_WIDTH, (int)(divY - listY));
      
      if (mapCount == 0) {
          // Show empty state
          float cx = SCREEN_WIDTH / 2.0f;
          float cy = listY + (divY - listY) / 2.0f;
          UIDrawText("\uf05a", UIFonts_GetIcon(S(30)), cx - S(15), cy - S(35), S(30), ColorGray);
          DrawCentredText("NO MAPPING DATA LOADED", faceSm, 0, SCREEN_WIDTH, cy - S(2), S(12), ColorGray);
          DrawCentredText("Select a Preset from the Controllers tab first.", faceXS, 0, SCREEN_WIDTH, cy + S(14), S(10), ColorGray);
      }
      
      for (int i = 0; i < visibleRows; i++) {
          int idx_f = r->State->MappingListScroll + i;
          if (idx_f >= mapCount) break;
          
          int idx = mapIndices[idx_f];
          SettingItem *item = &r->State->Items[idx];
          float ry = listY + (i * rowH);
          
          bool selected = (i == r->State->MappingListCursorPos);
          if (selected) {
              DrawRectangleRounded((Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)}, 0.15f, 4, (Color){ 255, 121, 0, 45 });
              DrawRectangleRoundedLines((Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)}, 0.15f, 4, 1.0f, ColorOrange);
              DrawCircle(S(14), ry + (rowH / 2.0f), S(3.5f), ColorOrange);
          } else if (idx_f % 2 == 0) {
              DrawRectangleRounded((Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)}, 0.15f, 4, ColorDark1);
          } else {
              DrawRectangleRounded((Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)}, 0.15f, 4, (Color){18, 18, 18, 255});
          }
          
          // Label
          UIDrawText(item->Label, faceMd, S(32), ry + (rowH / 2.0f) - S(7), S(13), ColorWhite);
          
          // Mapped Address Badge
          Rectangle badgeRect = { SCREEN_WIDTH - S(160) + S(10), ry + (rowH - S(20)) / 2.0f, S(65), S(20) };
          DrawRectangleRounded(badgeRect, 0.4f, 4, ColorDark2);
          DrawRectangleRoundedLines(badgeRect, 0.4f, 4, 1.0f, ColorGray);
          DrawCentredText(item->Unit, faceSm, badgeRect.x, badgeRect.width, ry + (rowH / 2.0f) - S(6), S(10), ColorOrange);
          
          // Type Badge
          Color typeColor = ColorGray;
          const char *typeIcon = "";
          if (strcmp(item->Options[0], "SCRIPT") == 0) { typeColor = ColorBlue; typeIcon = "\uf121"; }
          else if (strcmp(item->Options[0], "REL") == 0) { typeColor = ColorOrange; typeIcon = "\uf01e"; }
          else if (strcmp(item->Options[0], "14BIT") == 0) { typeColor = ColorDGreen; typeIcon = "\uf0c9"; }
          
          UIDrawText(typeIcon, faceIconSm, SCREEN_WIDTH - S(72), ry + (rowH / 2.0f) - S(5), S(10), typeColor);
          UIDrawText(item->Options[0], faceSm, SCREEN_WIDTH - S(55), ry + (rowH / 2.0f) - S(6), S(9), typeColor);
      }
      EndScissorMode();
      
      // Scrollbar
      DrawScrollbar(SCREEN_WIDTH - S(2.5f), listY, S(2), divY - listY,
                    mapCount, r->State->MappingListScroll, visibleRows);
      
      // Bottom Background & Details Banner
      DrawRectangle(0, divY, SCREEN_WIDTH, bottomH, ColorDark1);
      DrawLine(0, divY, SCREEN_WIDTH, divY, ColorGray);
      
      // Details Banner for currently selected sub-item
      SettingItem *selectedItem = NULL;
      int sIdx = r->State->MappingListScroll + r->State->MappingListCursorPos;
      if (sIdx >= 0 && sIdx < mapCount) {
          selectedItem = &r->State->Items[mapIndices[sIdx]];
      }
      char detailBuf[128] = "";
      if (selectedItem) {
          const char *mapType = (selectedItem->OptionsCount > 0) ? selectedItem->Options[0] : "UNKNOWN";
          const char *mapAddress = (selectedItem->Unit[0] != '\0') ? selectedItem->Unit : "UNMAPPED";
          snprintf(detailBuf, sizeof(detailBuf), "Selected Target: %s  |  MIDI Address: %s  |  Type: %s", 
                   selectedItem->Label, mapAddress, mapType);
      }
      
      DrawRectangle(0, divY, SCREEN_WIDTH, S(18), ColorDark2);
      UIDrawText(detailBuf[0] != '\0' ? detailBuf : "No target selected", faceXS, S(15), divY + S(5), S(9.5f), ColorOrange);
      DrawLine(0, divY + S(18), SCREEN_WIDTH, divY + S(18), ColorGray);
      
      // BACK Button
      Rectangle backRect = { S(15), divY + S(23), S(90), S(18) };
      DrawRectangleRounded(backRect, 0.5f, 4, ColorBlue);
      DrawRectangleRoundedLines(backRect, 0.5f, 4, 1.0f, ColorWhite);
      DrawCentredText("BACK", faceSm, backRect.x, backRect.width, divY + S(26), S(11), ColorWhite);
      
      // CREATE NEW Action Button
      Rectangle createRect = { SCREEN_WIDTH - S(250), divY + S(23), S(110), S(18) };
      DrawRectangleRounded(createRect, 0.5f, 4, ColorDark2);
      DrawRectangleRoundedLines(createRect, 0.5f, 4, 1.0f, ColorGray);
      DrawCentredText("CREATE NEW TEMPLATE", faceXS, createRect.x, createRect.width, divY + S(27), S(8.5f), ColorOrange);
      
      // SAVE AS CUSTOM Action Button
      Rectangle saveRect = { SCREEN_WIDTH - S(130), divY + S(23), S(115), S(18) };
      DrawRectangleRounded(saveRect, 0.5f, 4, ColorDark2);
      DrawRectangleRoundedLines(saveRect, 0.5f, 4, 1.0f, ColorGray);
      DrawCentredText("SAVE AS CUSTOM XML", faceXS, saveRect.x, saveRect.width, divY + S(27), S(8.5f), ColorOrange);

      // Item count string in sub-window
      char countStr[32];
      sprintf(countStr, "%d / %d", sIdx + 1, mapCount);
      UIDrawText(countStr, faceXS, SCREEN_WIDTH / 2.0f - S(80.0f), divY + S(26), S(9), ColorGray);
      
      // Tip label in sub-window
      UIDrawText("Tip: Click any target row to edit mapping.", faceXS, SCREEN_WIDTH / 2.0f - S(25.0f), divY + S(26), S(9), ColorGray);
      
      // Still draw Edit Modal and MIDI Learn dialog on top if active!
      if (r->State->IsEditMappingOpen) {
          // Handled next
      }
  }

  if (r->State->IsEditMappingOpen) {
      // Dark glassmorphic backdrop
      DrawRectangle(0, 0, SCREEN_WIDTH, viewH, (Color){ 10, 10, 15, 230 });
      
      // Dialog Card layout
      float cardW = S(320.0f);
      float cardH = S(185.0f);
      Rectangle cardRect = { (SCREEN_WIDTH - cardW) / 2.0f, (viewH - cardH) / 2.0f, cardW, cardH };
      
      // Neon orange border glow
      DrawRectangleRounded((Rectangle){ cardRect.x - S(2), cardRect.y - S(2), cardRect.width + S(4), cardRect.height + S(4) }, 0.08f, 4, (Color){ 255, 121, 0, 80 });
      DrawRectangleRounded(cardRect, 0.08f, 4, ColorBGUtil);
      DrawRectangleRoundedLines(cardRect, 0.08f, 4, 1.5f, ColorOrange);
      
      // Header Title
      DrawCentredText("EDIT MIDI MAPPING", faceMd, cardRect.x, cardRect.width, cardRect.y + S(12), S(13), ColorOrange);
      DrawLine(cardRect.x, cardRect.y + S(28), cardRect.x + cardW, cardRect.y + S(28), ColorGray);
      
      // Details Info
      SettingItem *item = &r->State->Items[r->State->EditMappingItemIdx];
      char nameBuf[128], addrBuf[128], typeBuf[128];
      snprintf(nameBuf, sizeof(nameBuf), "Target Name: %s", item->Label);
      snprintf(addrBuf, sizeof(addrBuf), "MIDI Bind  : %s", item->Unit);
      snprintf(typeBuf, sizeof(typeBuf), "Action Type: %s", item->Options[0]);
      
      UIDrawText(nameBuf, faceSm, cardRect.x + S(20), cardRect.y + S(36), S(10.5f), ColorWhite);
      UIDrawText(addrBuf, faceSm, cardRect.x + S(20), cardRect.y + S(54), S(10.5f), ColorWhite);
      UIDrawText(typeBuf, faceSm, cardRect.x + S(20), cardRect.y + S(72), S(10.5f), ColorWhite);
      
      // 2x2 Buttons Grid
      Rectangle btn1 = { cardRect.x + S(15), cardRect.y + S(105), S(135), S(24) };
      DrawRectangleRounded(btn1, 0.2f, 4, ColorBlue);
      DrawCentredText("START MIDI LEARN", faceXS, btn1.x, btn1.width, btn1.y + S(7), S(9.5f), ColorWhite);
      
      Rectangle btn2 = { cardRect.x + S(170), cardRect.y + S(105), S(135), S(24) };
      DrawRectangleRounded(btn2, 0.2f, 4, ColorDark2);
      DrawRectangleRoundedLines(btn2, 0.2f, 4, 1.0f, ColorGray);
      DrawCentredText("CYCLE TYPE", faceXS, btn2.x, btn2.width, btn2.y + S(7), S(9.5f), ColorOrange);
      
      Rectangle btn3 = { cardRect.x + S(15), cardRect.y + S(140), S(135), S(24) };
      DrawRectangleRounded(btn3, 0.2f, 4, (Color){ 100, 30, 30, 255 });
      DrawRectangleRoundedLines(btn3, 0.2f, 4, 1.0f, ColorRed);
      DrawCentredText("CLEAR / RESET", faceXS, btn3.x, btn3.width, btn3.y + S(7), S(9.5f), ColorWhite);
      
      Rectangle btn4 = { cardRect.x + S(170), cardRect.y + S(140), S(135), S(24) };
      DrawRectangleRounded(btn4, 0.2f, 4, ColorDGreen);
      DrawCentredText("SAVE & CLOSE", faceXS, btn4.x, btn4.width, btn4.y + S(7), S(9.5f), ColorWhite);
  }
}

void SettingsRenderer_Init(SettingsRenderer *r, SettingsState *state) {
  r->base.Update = Settings_Update;
  r->base.Draw = Settings_Draw;
  r->State = state;
  r->State->IsEditMappingOpen = false;
  r->State->EditMappingItemIdx = 0;
  r->State->IsMappingListOpen = false;
  r->State->MappingListScroll = 0;
  r->State->MappingListCursorPos = 0;
  r->OnClose = NULL;
  r->OnApply = NULL;
  r->callbackCtx = NULL;
}
