#include "ui/views/settings.h"
#include "core/midi/midi_handler.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "input/input.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/audio_backend.h"

void Settings_RefreshAudioDevices(SettingsState *state) {
  AudioDeviceInfo devs[MAX_AUDIO_DEVICES];
  int devCount = AudioBackend_GetDevices(devs, MAX_AUDIO_DEVICES);
  state->Items[10].OptionsCount = devCount + 1;
  strcpy(state->Items[10].Options[0], "System Default");
  for (int i = 0; i < devCount && i < MAX_SETTING_OPTIONS - 1; i++) {
    snprintf(state->Items[10].Options[i + 1], 64, "%dCH %s",
             devs[i].NativeChannels, devs[i].Name);
  }
  // Prevent out of bounds if a device was disconnected
  if (state->Items[10].Current >= state->Items[10].OptionsCount) {
    state->Items[10].Current = 0;
  }
}

static double g_lastSettingsClickTime = 0.0;

static int Settings_Update(Component *base) {
  SettingsRenderer *r = (SettingsRenderer *)base;
  if (!r->State->IsActive)
    return 0;

  double now = GetTime();
  bool canClick = (now - g_lastSettingsClickTime) >= 0.12;
  Vector2 mouse = Input_GetPointerPos();

  // If a sub-window or modal popup is open, disable background item clicking
  if (r->State->IsDropdownOpen || r->State->IsEditMappingOpen ||
      r->State->IsMappingListOpen || r->State->IsSystemInfoOpen ||
      r->State->IsSliderModalOpen || r->State->IsConfirmPopupOpen) {
    canClick = false;
  }

  if (r->State->IsSystemInfoOpen) {
    if (Input_IsReleased() && Input_GetDragDistance() < S(10.0f)) {
      Vector2 mouse = Input_GetPointerPos();
      float winW = GetScreenWidth();
      float winH = GetScreenHeight() - DECK_STR_H;
      float modalW = S(360.0f);
      float modalH = S(265.0f);
      float modalX = (winW - modalW) / 2.0f;
      float modalY = (winH - modalH) / 2.0f;

      Rectangle closeBtn = {modalX + modalW - S(32.0f), modalY + S(3.0f),
                            S(28.0f), S(24.0f)};
      Rectangle okBtn = {modalX + (modalW - S(100.0f)) / 2.0f,
                         modalY + modalH - S(34.0f), S(100.0f), S(26.0f)};

      if (CheckCollisionPointRec(mouse, closeBtn) ||
          CheckCollisionPointRec(mouse, okBtn) ||
          !CheckCollisionPointRec(
              mouse, (Rectangle){modalX, modalY, modalW, modalH})) {
        r->State->IsSystemInfoOpen = false;
        Input_Consume();
        return 1;
      }
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) ||
        IsKeyPressed(KEY_ENTER)) {
      r->State->IsSystemInfoOpen = false;
      return 1;
    }
    return 1;
  }

  // --- CONFIRMATION POPUP HANDLER ---
  if (r->State->IsConfirmPopupOpen) {
    if (Input_IsReleased() && Input_GetDragDistance() < S(10.0f)) {
      Vector2 mouse = Input_GetPointerPos();
      float winW = GetScreenWidth();
      float winH = GetScreenHeight() - DECK_STR_H;
      float modalW = S(320.0f);
      float modalH = S(160.0f);
      float modalX = (winW - modalW) / 2.0f;
      float modalY = (winH - modalH) / 2.0f;

      Rectangle cancelBtn = {modalX + S(20), modalY + modalH - S(44), S(130), S(32)};
      Rectangle okBtn = {modalX + modalW - S(150), modalY + modalH - S(44), S(130), S(32)};

      if (CheckCollisionPointRec(mouse, cancelBtn)) {
        r->State->IsConfirmPopupOpen = false;
        Input_Consume();
        return 1;
      }
      if (CheckCollisionPointRec(mouse, okBtn)) {
        if (r->OnAction) {
           r->OnAction(r->callbackCtx, r->State->ConfirmActionIdx);
        }
        r->State->IsConfirmPopupOpen = false;
        Input_Consume();
        return 1;
      }
      if (!CheckCollisionPointRec(mouse, (Rectangle){modalX, modalY, modalW, modalH})) {
        r->State->IsConfirmPopupOpen = false;
        Input_Consume();
        return 1;
      }
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
      r->State->IsConfirmPopupOpen = false;
      return 1;
    }
    if (IsKeyPressed(KEY_ENTER)) {
      if (r->OnAction) {
         r->OnAction(r->callbackCtx, r->State->ConfirmActionIdx);
      }
      r->State->IsConfirmPopupOpen = false;
      return 1;
    }
    return 1;
  }

  // --- SLIDER MODAL HANDLER ---
  if (r->State->IsSliderModalOpen) {
    SettingItem *item = &r->State->Items[r->State->SliderItemIdx];
    float winW = GetScreenWidth();
    float winH = GetScreenHeight() - DECK_STR_H;
    float modalW = S(400.0f);
    float modalH = S(160.0f);
    float modalX = (winW - modalW) / 2.0f;
    float modalY = (winH - modalH) / 2.0f;

    // Handle encoder / keyboard
    float step = (item->Step > 0) ? item->Step : (item->Max - item->Min) / 40.0f;
    if (r->State->MidiBrowseDelta != 0) {
      item->Value += r->State->MidiBrowseDelta * step;
      if (item->Value < item->Min) item->Value = item->Min;
      if (item->Value > item->Max) item->Value = item->Max;
      r->State->MidiBrowseDelta = 0;
      if (r->OnApply) r->OnApply(r->callbackCtx);
    }
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

    if (Input_IsReleased()) {
      Vector2 mouse = Input_GetPointerPos();
      Rectangle closeBtn = {modalX + modalW - S(32), modalY + S(3), S(28), S(24)};
      Rectangle okBtn = {modalX + (modalW - S(120))/2.0f, modalY + modalH - S(44), S(120), S(32)};
      
      // Allow touching slider track
      Rectangle sliderRect = {modalX + S(30), modalY + S(70), modalW - S(60), S(30)};
      bool inSlider = CheckCollisionPointRec(mouse, sliderRect);
      
      if (CheckCollisionPointRec(mouse, closeBtn) || CheckCollisionPointRec(mouse, okBtn)) {
        r->State->IsSliderModalOpen = false;
        Input_Consume();
        return 1;
      }
      if (!inSlider && !CheckCollisionPointRec(mouse, (Rectangle){modalX, modalY, modalW, modalH})) {
        r->State->IsSliderModalOpen = false;
        Input_Consume();
        return 1;
      }
    }
    
    // Touch drag on slider
    if (Input_IsDown()) {
       Vector2 mouse = Input_GetPointerPos();
       Rectangle sliderRect = {modalX + S(30), modalY + S(60), modalW - S(60), S(50)}; // wider touch area
       if (CheckCollisionPointRec(mouse, sliderRect)) {
          float relX = mouse.x - sliderRect.x;
          float pct = relX / sliderRect.width;
          if (pct < 0) pct = 0;
          if (pct > 1) pct = 1;
          item->Value = item->Min + pct * (item->Max - item->Min);
          if (r->OnApply) r->OnApply(r->callbackCtx);
       }
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ENTER) || r->State->MidiRequestEnter) {
      r->State->IsSliderModalOpen = false;
      r->State->MidiRequestEnter = false;
      return 1;
    }
    return 1;
  }

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

    Rectangle dropRect = {dropdownX, dropdownY, dropdownW, dropdownH};

    // Scroll handling & Encoder navigation for dropdown
    if (r->State->MidiBrowseDelta != 0) {
      if (r->State->MidiBrowseDelta > 0) {
        item->Current = (item->Current + 1) % item->OptionsCount;
      } else if (r->State->MidiBrowseDelta < 0) {
        item->Current =
            (item->Current - 1 + item->OptionsCount) % item->OptionsCount;
      }
      r->State->MidiBrowseDelta = 0;
      if (r->OnValueChanged)
        r->OnValueChanged(r->callbackCtx, r->State->DropdownItemIdx);
      if (r->OnApply)
        r->OnApply(r->callbackCtx);
    }

    if (r->State->MidiRequestEnter) {
      r->State->IsDropdownOpen = false;
      r->State->MidiRequestEnter = false;
      if (r->OnValueChanged)
        r->OnValueChanged(r->callbackCtx, r->State->DropdownItemIdx);
      if (r->OnApply)
        r->OnApply(r->callbackCtx);
      return 1;
    }

    // BUG-13 FIX: Use drag accumulator to avoid dropdown jump on initial tap
    static float dropDragAccum = 0.0f;
    if (Input_IsPressed()) {
      dropDragAccum = 0.0f;
      r->State->TouchDragAccumulator = 0.0f;
    }
    if (Input_IsDown()) {
      float dy = Mouse_GetDelta().y;
      dropDragAccum += fabsf(dy);
      if (dropDragAccum > S(3.0f)) {
        r->State->DropdownScroll -= dy;
      }
    }

    float maxScroll = contentH - dropdownH;
    if (maxScroll < 0)
      maxScroll = 0;
    if (r->State->DropdownScroll < 0)
      r->State->DropdownScroll = 0;
    if (r->State->DropdownScroll > maxScroll)
      r->State->DropdownScroll = maxScroll;

    if (Input_IsReleased()) {
      g_lastSettingsClickTime = now;
      if (!CheckCollisionPointRec(mouse, dropRect)) {
        r->State->IsDropdownOpen = false;
        r->State->TouchDragAccumulator = 0.0f;
        Input_Consume();
      } else {
        float cy = dropdownY - r->State->DropdownScroll;
        for (int i = 0; i < item->OptionsCount; i++) {
          Rectangle opRect = {dropdownX, cy, dropdownW, opHeight};
          if (CheckCollisionPointRec(mouse, opRect) && cy >= dropdownY &&
              (cy + opHeight) <= (dropdownY + dropdownH)) {
            if (dropDragAccum < S(10.0f)) {
              item->Current = i;
              r->State->IsDropdownOpen = false;
              r->State->TouchDragAccumulator = 0.0f;
              if (r->OnValueChanged)
                r->OnValueChanged(r->callbackCtx, r->State->DropdownItemIdx);
              if (r->OnApply)
                r->OnApply(r->callbackCtx);
              Input_Consume();
            }
          }
          cy += opHeight;
        }
      }
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
      r->State->IsDropdownOpen = false;
      r->State->TouchDragAccumulator = 0.0f;
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
        snprintf(r->State->Items[idx].Unit, 16, "0x%02X:0x%02X", status,
                 midino);
        if (r->OnApply)
          r->OnApply(r->callbackCtx);
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
    Rectangle btn1 = {modalX + S(15), modalY + S(105), S(135), S(24)};
    Rectangle btn2 = {modalX + S(170), modalY + S(105), S(135), S(24)};
    Rectangle btn3 = {modalX + S(15), modalY + S(140), S(135), S(24)};
    Rectangle btn4 = {modalX + S(170), modalY + S(140), S(135), S(24)};

    if (Input_IsReleased()) {
      Vector2 mousePos = Input_GetPointerPos();
      if (CheckCollisionPointRec(mousePos, btn1)) {
        g_lastSettingsClickTime = now;
        r->State->IsLearningMidi = true;
        r->State->LearningItemIdx = r->State->EditMappingItemIdx;
        uint8_t s, m;
        while (MIDI_GetLastMessage(&s, &m))
          ; // Flush
        Input_Consume();
      } else if (CheckCollisionPointRec(mousePos, btn2)) {
        g_lastSettingsClickTime = now;
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
          if (r->OnApply)
            r->OnApply(r->callbackCtx);
        }
        Input_Consume();
      } else if (CheckCollisionPointRec(mousePos, btn3)) {
        g_lastSettingsClickTime = now;
        if (mapIdx >= 0 && mapIdx < map->count) {
          map->entries[mapIdx].status = 0;
          map->entries[mapIdx].midino = 0;
          strcpy(item->Unit, "0x00:0x00");
          if (r->OnApply)
            r->OnApply(r->callbackCtx);
        }
        Input_Consume();
      } else if (CheckCollisionPointRec(mousePos, btn4)) {
        g_lastSettingsClickTime = now;
        r->State->IsEditMappingOpen = false;
        Input_Consume();
      } else {
        Input_Consume();
      }
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) ||
        IsKeyPressed(KEY_ENTER)) {
      r->State->IsEditMappingOpen = false;
    }
    return 1;
  }

  if (r->State->IsMappingListOpen) {
    // Gather all actual mapping items
    int mapIndices[MAX_SETTINGS_ITEMS];
    int mapCount = 0;
    for (int i = 0; i < r->State->ItemsCount; i++) {
      if (r->State->Items[i].Category == SETTING_CAT_CONTROLLERS &&
          strcmp(r->State->Items[i].Label, "CONNECTED DEVICE") != 0 &&
          strcmp(r->State->Items[i].Label, "MAPPING PRESET") != 0) {
        mapIndices[mapCount++] = i;
      }
    }

    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    float listY = TOP_BAR_H + S(28.0f);
    float rowH = S(32.0f);
    float bottomH = (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(46.0f) : 0.0f;
    float divY = viewH - bottomH;
    int visibleRows = (int)((divY - listY) / rowH);

    // Mouse click list item inside mapping list sub-window
    if (Input_IsReleased()) {
      Vector2 mousePos = Input_GetPointerPos();

      // Back Button at the bottom
      Rectangle backRect = {S(15), divY + S(23), S(90), S(18)};
      if (CheckCollisionPointRec(mousePos, backRect)) {
        g_lastSettingsClickTime = now;
        r->State->IsMappingListOpen = false;
        Input_Consume();
        return 1;
      }

      // CREATE NEW Button
      Rectangle createRect = {SCREEN_WIDTH - S(250), divY + S(23), S(110),
                              S(18)};
      if (CheckCollisionPointRec(mousePos, createRect)) {
        g_lastSettingsClickTime = now;
        if (r->OnAction)
          r->OnAction(r->callbackCtx, 20);
        Input_Consume();
        return 1;
      }

      // SAVE AS CUSTOM Button
      Rectangle saveRect = {SCREEN_WIDTH - S(130), divY + S(23), S(115), S(18)};
      if (CheckCollisionPointRec(mousePos, saveRect)) {
        g_lastSettingsClickTime = now;
        if (r->OnAction)
          r->OnAction(r->callbackCtx, 21);
        Input_Consume();
        return 1;
      }

      // Check list clicks
      for (int i = 0; i < visibleRows; i++) {
        int idx_f = r->State->MappingListScroll + i;
        if (idx_f >= mapCount)
          break;

        float ry = listY + (i * rowH);
        Rectangle rowRect = {0, ry, SCREEN_WIDTH, rowH};
        if (CheckCollisionPointRec(mousePos, rowRect)) {
          g_lastSettingsClickTime = now;
          r->State->MappingListCursorPos = i;
          int actualIdx = mapIndices[idx_f];
          r->State->IsEditMappingOpen = true;
          r->State->EditMappingItemIdx = actualIdx;
          return 1;
        }
      }
    }

    // Mouse Wheel Scroll inside sub-window
    float wheel = Mouse_GetWheel();
    if (wheel != 0) {
      if (wheel > 0) {
        if (r->State->MappingListScroll > 0)
          r->State->MappingListScroll--;
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
          r->State->MappingListScroll + r->State->MappingListCursorPos <
              mapCount - 1) {
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

    // BUG-14 FIX: Touch swipe scroll for mapping list
    static float mapDragStartY = 0.0f;
    static float mapDragAccum = 0.0f;
    static bool mapIsDragging = false;
    Vector2 mapMouse = Input_GetPointerPos();
    if (Input_IsPressed()) {
      mapDragStartY = mapMouse.y;
      mapDragAccum = 0.0f;
      mapIsDragging = false;
    }
    if (Input_IsDown()) {
      float dy = mapMouse.y - mapDragStartY;
      mapDragAccum += fabsf(mapMouse.y - mapDragStartY);
      mapDragStartY = mapMouse.y;
      if (mapDragAccum > S(4.0f))
        mapIsDragging = true;
      if (mapIsDragging) {
        float scrollPx = -dy;
        int scrollRows = (int)(scrollPx / rowH);
        if (scrollRows != 0) {
          r->State->MappingListScroll += scrollRows;
          if (r->State->MappingListScroll < 0)
            r->State->MappingListScroll = 0;
          int maxMapScroll = mapCount - visibleRows;
          if (maxMapScroll < 0)
            maxMapScroll = 0;
          if (r->State->MappingListScroll > maxMapScroll)
            r->State->MappingListScroll = maxMapScroll;
        }
      }
    }
    if (Input_IsReleased()) {
      mapIsDragging = false;
    }

    return 1; // block background interaction
  }

  // Get filtered indices (Hides individual mapping entries in main view)
  int filteredIndices[MAX_SETTINGS_ITEMS];
  int filteredCount = 0;
  for (int i = 0; i < r->State->ItemsCount; i++) {
    if (r->State->Items[i].Category == (SettingCategory)r->State->SelectedTab) {
      if (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) {
        if (strcmp(r->State->Items[i].Label, "CONNECTED DEVICE") == 0 ||
            strcmp(r->State->Items[i].Label, "MAPPING PRESET") == 0) {
          filteredIndices[filteredCount++] = i;
        }
      } else {
        filteredIndices[filteredCount++] = i;
      }
    }
  }

  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  float tabH = S(28.0f);
  float rowH =
      (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(38.0f) : S(32.0f);
  float bottomH = (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(46.0f) : 0.0f;
  float listY = TOP_BAR_H + tabH;
  float effectiveListY = listY;
  int visibleRows = (int)((viewH - effectiveListY - bottomH) / rowH);
  if (visibleRows <= 0)
    visibleRows = 1;

  // Handle 3-Tier MIDI Rotary Encoder Navigation for Settings
  // Level 0 = Tab/Category selection
  // Level 1 = Sub-menu/List item selection
  // Level 2 = Value editing mode
  // Handle Modal Rotary Encoder Navigation
  if (r->State->IsConfirmPopupOpen || r->State->IsSystemInfoOpen || r->State->IsSliderModalOpen || r->State->IsEditMappingOpen) {
      if (r->State->MidiBrowseDelta != 0) {
          int maxPos = 1; // 2 buttons: Cancel, OK (0, 1)
          if (r->State->IsSystemInfoOpen) maxPos = 0; // Only Close button (0)
          if (r->State->IsEditMappingOpen) maxPos = 3; // 4 buttons (0,1,2,3)
          if (r->State->IsSliderModalOpen) maxPos = 1; // Slider (0), OK (1)

          r->State->ModalCursorPos += r->State->MidiBrowseDelta;
          if (r->State->ModalCursorPos < 0) r->State->ModalCursorPos = 0;
          if (r->State->ModalCursorPos > maxPos) r->State->ModalCursorPos = maxPos;
          r->State->MidiBrowseDelta = 0;
      }
      
      if (r->State->IsSliderModalOpen && r->State->ModalCursorPos == 0 && r->State->MidiBrowseDelta != 0) {
          // If on slider, let MidiBrowseDelta pass through?
          // Actually MidiBrowseDelta is already consumed. 
      }
  } else if (r->State->MidiBrowseDelta != 0) {
    int delta = r->State->MidiBrowseDelta;
    r->State->MidiBrowseDelta = 0;

    if (r->State->FocusLevel == 0) {
      int nextTab = r->State->SelectedTab + (delta > 0 ? 1 : -1);
      if (nextTab < 0)
        nextTab = SETTING_CAT_COUNT - 1;
      if (nextTab >= SETTING_CAT_COUNT)
        nextTab = 0;
      r->State->SelectedTab = nextTab;
      r->State->CursorPos = 0;
      r->State->Scroll = 0;
      r->State->VisualScroll = 0;
    } else if (r->State->FocusLevel == 1) {
      r->State->CursorPos += delta;
      if (r->State->CursorPos < 0) {
        if (r->State->Scroll > 0) {
          r->State->Scroll += r->State->CursorPos;
          if (r->State->Scroll < 0)
            r->State->Scroll = 0;
        }
        r->State->CursorPos = 0;
      } else if (r->State->CursorPos >= visibleRows) {
        int maxOffset = filteredCount - visibleRows;
        if (maxOffset < 0)
          maxOffset = 0;
        r->State->Scroll += (r->State->CursorPos - (visibleRows - 1));
        if (r->State->Scroll > maxOffset)
          r->State->Scroll = maxOffset;
        r->State->CursorPos = visibleRows - 1;
      }
      r->State->VisualScroll = r->State->Scroll * rowH;
    } else if (r->State->FocusLevel == 2) {
      int idx_f = r->State->Scroll + r->State->CursorPos;
      if (idx_f >= 0 && idx_f < filteredCount) {
        int idx = filteredIndices[idx_f];
        SettingItem *item = &r->State->Items[idx];
        if (item->Type == SETTING_TYPE_KNOB) {
          float step =
              (item->Step > 0) ? item->Step : (item->Max - item->Min) / 20.0f;
          item->Value += (delta > 0 ? step : -step);
          if (item->Value < item->Min)
            item->Value = item->Min;
          if (item->Value > item->Max)
            item->Value = item->Max;
          if (r->OnApply)
            r->OnApply(r->callbackCtx);
        }
      }
    }
  }

  // Handle 3-Tier MIDI Rotary Encoder Click for Settings
  if (r->State->MidiRequestEnter) {
    r->State->MidiRequestEnter = false;

    if (r->State->FocusLevel == 0) {
      r->State->FocusLevel = 1;
      r->State->CursorPos = 0;
      r->State->Scroll = 0;
      r->State->VisualScroll = 0;
    } else if (r->State->FocusLevel == 1) {
      int idx_f = r->State->Scroll + r->State->CursorPos;
      if (idx_f >= 0 && idx_f < filteredCount) {
        int idx = filteredIndices[idx_f];
        SettingItem *item = &r->State->Items[idx];
        if (item->Type == SETTING_TYPE_LIST) {
          if (strcmp(item->Label, "AUDIO DEVICE") == 0) {
            Settings_RefreshAudioDevices(r->State);
          }
          r->State->IsDropdownOpen = true;
          r->State->DropdownItemIdx = idx;
          r->State->DropdownScroll = 0;
          r->State->FocusLevel = 2;
        } else if (item->Type == SETTING_TYPE_KNOB) {
          // Double click to open slider modal
          if (now - g_lastSettingsClickTime < 0.35) {
            r->State->IsSliderModalOpen = true;
            r->State->SliderItemIdx = idx;
          }
        } else if (item->Type == SETTING_TYPE_ACTION) {
          if (item->Category == SETTING_CAT_CONTROLLERS &&
              strcmp(item->Label, "CONNECTED DEVICE") != 0 &&
              strcmp(item->Label, "MAPPING PRESET") != 0) {
            if (strcmp(item->Label, "EDIT MAPPING LIST") == 0) {
              r->State->IsMappingListOpen = true;
              r->State->MappingListScroll = 0;
              r->State->MappingListCursorPos = 0;
            } else {
              r->State->IsEditMappingOpen = true;
              r->State->EditMappingItemIdx = idx;
            }
          } else {
            // Confirm popup for jogwheel reset
            if (strcmp(item->Label, "LOAD DEFAULT JOGWHEEL SETTINGS") == 0) {
                r->State->IsConfirmPopupOpen = true;
                r->State->ConfirmActionIdx = idx;
                strncpy(r->State->ConfirmMessage, "Are you sure you want to load default jogwheel settings?", 127);
            } else {
                if (r->OnAction) r->OnAction(r->callbackCtx, idx);
            }
          }
        }
      }
    } else if (r->State->FocusLevel == 2) {
      r->State->FocusLevel = 1;
      if (r->State->IsDropdownOpen) {
        r->State->IsDropdownOpen = false;
        if (r->OnValueChanged)
          r->OnValueChanged(r->callbackCtx, r->State->DropdownItemIdx);
      }
      if (r->OnApply)
        r->OnApply(r->callbackCtx);
    }
  }

  float maxScroll = (filteredCount - visibleRows) * rowH;
  if (maxScroll < 0)
    maxScroll = 0;

  // Touch Drag & Kinetic Swipe Scrubbing
  Vector2 mousePos = Input_GetPointerPos();
  if (Input_IsPressed()) {
    r->State->LastTouchY = mousePos.y;
    r->State->TouchDragAccumulator = 0;
    r->State->IsDragging = false;
    r->State->ScrollVelocity = 0;
  }

  if (Input_IsDown()) {
    float frameTime = GetFrameTime();
    if (frameTime < 0.001f)
      frameTime = 0.016f;
    float dy = mousePos.y - r->State->LastTouchY;
    r->State->LastTouchY = mousePos.y;
    r->State->TouchDragAccumulator += fabsf(dy);

    if (!r->State->IsDragging && r->State->TouchDragAccumulator > S(4.0f)) {
      r->State->IsDragging = true;
    }

    if (r->State->IsDragging || r->State->TouchDragAccumulator > S(2.0f)) {
      r->State->VisualScroll -= dy;
      if (r->State->VisualScroll < 0.0f) {
        r->State->VisualScroll = 0.0f;
        r->State->TouchVelocityY = 0.0f;
      } else if (r->State->VisualScroll > maxScroll) {
        r->State->VisualScroll = maxScroll;
        r->State->TouchVelocityY = 0.0f;
      } else {
        float instantVel = -dy / frameTime;
        r->State->TouchVelocityY =
            r->State->TouchVelocityY * 0.3f + instantVel * 0.7f;
      }
    }
  } else {
    // Kinetic Inertia Decay (Smooth Flick)
    r->State->VisualScroll += r->State->ScrollVelocity * GetFrameTime();
    r->State->ScrollVelocity *= 0.95f;
    if (fabsf(r->State->ScrollVelocity) < 5.0f)
      r->State->ScrollVelocity = 0.0f;

    // Strict Boundary Clamping (No overscroll bounce / sembul / pantul)
    if (r->State->VisualScroll < 0.0f) {
      r->State->VisualScroll = 0.0f;
      r->State->ScrollVelocity = 0.0f;
    } else if (r->State->VisualScroll > maxScroll) {
      r->State->VisualScroll = maxScroll;
      r->State->ScrollVelocity = 0.0f;
    }
  }

  if (Input_IsReleased()) {
    if (r->State->IsDragging && fabsf(r->State->TouchVelocityY) > 60.0f) {
      r->State->ScrollVelocity = r->State->TouchVelocityY;
    }
    r->State->IsDragging = false;
  }

  // Sync to discrete scroll offset
  r->State->Scroll = (int)(r->State->VisualScroll / rowH);
  if (r->State->Scroll < 0)
    r->State->Scroll = 0;
  int maxOffset = filteredCount - visibleRows;
  if (maxOffset < 0)
    maxOffset = 0;
  if (r->State->Scroll > maxOffset)
    r->State->Scroll = maxOffset;

  // Calculate hovered item row and row Y position
  int hoveredItemIdx = -1;
  float hoveredRowY = 0.0f;
  if (mousePos.y >= effectiveListY && mousePos.y < (viewH - bottomH)) {
    int rowIdx = (int)((mousePos.y - effectiveListY) / rowH);
    if (rowIdx >= 0 && rowIdx < visibleRows) {
      int idx_f = r->State->Scroll + rowIdx;
      if (idx_f < filteredCount) {
        hoveredItemIdx = filteredIndices[idx_f];
        hoveredRowY = effectiveListY + (rowIdx * rowH);
      }
    }
  }

  // Mouse Wheel Controls (Restricted Knob wheel area to prevent scroll interference)
  float wheel = Mouse_GetWheel();
  if (wheel != 0) {
    bool consumedByKnob = false;
    if (hoveredItemIdx >= 0 &&
        r->State->Items[hoveredItemIdx].Type == SETTING_TYPE_KNOB) {
      Rectangle knobArea = { SCREEN_WIDTH - S(140), hoveredRowY, S(140), rowH };
      if (CheckCollisionPointRec(mousePos, knobArea)) {
        consumedByKnob = true;
        SettingItem *kItem = &r->State->Items[hoveredItemIdx];
        float step =
            (kItem->Step > 0) ? kItem->Step : (kItem->Max - kItem->Min) / 50.0f;
        if (step < 1.0f && (kItem->Max - kItem->Min) > 50.0f)
          step = 10.0f;
        kItem->Value += (wheel > 0 ? step : -step);
        if (kItem->Value < kItem->Min)
          kItem->Value = kItem->Min;
        if (kItem->Value > kItem->Max)
          kItem->Value = kItem->Max;
        if (r->OnApply)
          r->OnApply(r->callbackCtx);
      }
    }
    
    if (!consumedByKnob) {
      r->State->VisualScroll -= wheel * rowH * 3.0f;
      r->State->ScrollVelocity = 0;
      if (r->State->VisualScroll < 0.0f)
        r->State->VisualScroll = 0.0f;
      if (r->State->VisualScroll > maxScroll)
        r->State->VisualScroll = maxScroll;
    }
  }

  // Mouse Drag Knob Controls (Restricted to knob control area only)
  if (!r->State->IsDropdownOpen && Input_IsDown()) {
    Vector2 mouseDelta = Mouse_GetDelta();
    if (hoveredItemIdx >= 0 &&
        r->State->Items[hoveredItemIdx].Type == SETTING_TYPE_KNOB) {
      Rectangle knobArea = { SCREEN_WIDTH - S(140), hoveredRowY, S(140), rowH };
      Vector2 touchStartPos = Input_GetStartPos();
      // Require touch start OR current mouse position to be in knob area
      if (CheckCollisionPointRec(mousePos, knobArea) || CheckCollisionPointRec(touchStartPos, knobArea)) {
        if (fabsf(mouseDelta.x) > 0.001f || fabsf(mouseDelta.y) > 0.001f) {
          SettingItem *kItem = &r->State->Items[hoveredItemIdx];
          float deltaVal = (mouseDelta.x - mouseDelta.y);
          float range = (kItem->Max - kItem->Min);
          float sensitivity =
              (range > 100.0f) ? (range / 150.0f) : (range / 80.0f);
          kItem->Value += deltaVal * sensitivity;
          if (kItem->Value < kItem->Min)
            kItem->Value = kItem->Min;
          if (kItem->Value > kItem->Max)
            kItem->Value = kItem->Max;
          if (r->OnApply)
            r->OnApply(r->callbackCtx);
        }
      }
    }
  }

  // OSK-style Touch Release & Debounced Tap Actions
  if (canClick && Input_IsReleased() && !r->State->IsDragging) {
    if (r->State->TouchDragAccumulator < S(10.0f) &&
        fabsf(r->State->ScrollVelocity) < 40.0f) {
      float divY = viewH - bottomH;

      // Tab Switching Logic
      float tabW = SCREEN_WIDTH / (float)SETTING_CAT_COUNT;
      for (int tIdx = 0; tIdx < SETTING_CAT_COUNT; tIdx++) {
        Rectangle tabRect = {tIdx * tabW, TOP_BAR_H, tabW, tabH};
        if (Touch_CheckClick(tabRect, S(4.0f))) {
          if (r->State->SelectedTab != tIdx) {
            g_lastSettingsClickTime = now;
            r->State->SelectedTab = tIdx;
            r->State->Scroll = 0;
            r->State->VisualScroll = 0;
            r->State->ScrollVelocity = 0;
            r->State->CursorPos = 0;
          }
          return 1;
        }
      }

      // List Item Selection & Action Clicking with pixelOffset
      float pixelOffset = fmodf(r->State->VisualScroll, rowH);
      for (int i = 0; i < visibleRows + 1; i++) {
        int idx_f = r->State->Scroll + i;
        if (idx_f >= filteredCount)
          break;

        int idx = filteredIndices[idx_f];
        float ry = effectiveListY - pixelOffset + (i * rowH);
        if (ry < effectiveListY - S(5.0f) || ry > (divY - S(5.0f)))
          continue;
        Rectangle rowRect = {0, ry, SCREEN_WIDTH, rowH};

        if (Input_CheckClick(rowRect)) {
          g_lastSettingsClickTime = now;
          r->State->CursorPos = i;

          SettingItem *clickedItem = &r->State->Items[idx];
          if (clickedItem->Category == SETTING_CAT_CONTROLLERS &&
              strcmp(clickedItem->Label, "CONNECTED DEVICE") != 0 &&
              strcmp(clickedItem->Label, "MAPPING PRESET") != 0) {
            if (strcmp(clickedItem->Label, "EDIT MAPPING LIST") == 0) {
              r->State->IsMappingListOpen = true;
              r->State->MappingListScroll = 0;
              r->State->MappingListCursorPos = 0;
            } else {
              r->State->IsEditMappingOpen = true;
              r->State->EditMappingItemIdx = idx;
            }
            return 1;
          } else if (clickedItem->Type == SETTING_TYPE_ACTION) {
            if (strcmp(clickedItem->Label, "LOAD DEFAULT JOGWHEEL SETTINGS") == 0) {
                r->State->IsConfirmPopupOpen = true;
                r->State->ConfirmActionIdx = idx;
                strncpy(r->State->ConfirmMessage, "Are you sure you want to load default jogwheel settings?", 127);
            } else {
                if (r->OnAction) r->OnAction(r->callbackCtx, idx);
            }
          } else if (clickedItem->Type == SETTING_TYPE_KNOB) {
            // Double click to open slider modal
            if (now - g_lastSettingsClickTime < 0.35) {
                r->State->IsSliderModalOpen = true;
                r->State->SliderItemIdx = idx;
            }
          } else if (clickedItem->Type == SETTING_TYPE_LIST) {
            if (strcmp(clickedItem->Label, "AUDIO DEVICE") == 0) {
              Settings_RefreshAudioDevices(r->State);
            }
            r->State->IsDropdownOpen = true;
            r->State->DropdownItemIdx = idx;
            r->State->DropdownScroll = 0;
            return 1;
          }
        }
      }
    }
  }

  // BUG-11 FIX: KEY_UP/DOWN only move cursor when FocusLevel >= 1 (item mode)
  if (r->State->FocusLevel >= 1) {
    if (IsKeyPressed(KEY_UP)) {
      if (r->State->CursorPos > 0) {
        r->State->CursorPos--;
      } else if (r->State->Scroll > 0) {
        r->State->Scroll--;
      }
    }
    // BUG-12 FIX: Use the same visibleRows already calculated above, not a
    // duplicate
    if (IsKeyPressed(KEY_DOWN)) {
      if (r->State->CursorPos < visibleRows - 1 &&
          r->State->Scroll + r->State->CursorPos < filteredCount - 1) {
        r->State->CursorPos++;
      } else if (r->State->Scroll + visibleRows < filteredCount) {
        r->State->Scroll++;
      }
    }
  } else {
    // FocusLevel == 0: UP/DOWN cycle through tabs
    if (IsKeyPressed(KEY_UP)) {
      int nextTab = r->State->SelectedTab - 1;
      if (nextTab < 0)
        nextTab = SETTING_CAT_COUNT - 1;
      r->State->SelectedTab = nextTab;
      r->State->Scroll = 0;
      r->State->CursorPos = 0;
      r->State->VisualScroll = 0;
    }
    if (IsKeyPressed(KEY_DOWN)) {
      int nextTab = (r->State->SelectedTab + 1) % SETTING_CAT_COUNT;
      r->State->SelectedTab = nextTab;
      r->State->Scroll = 0;
      r->State->CursorPos = 0;
      r->State->VisualScroll = 0;
    }
  }

  int idx_f = r->State->Scroll + r->State->CursorPos;
  if (idx_f < filteredCount && r->State->FocusLevel >= 1) {
    int idx = filteredIndices[idx_f];
    SettingItem *item = &r->State->Items[idx];
    // BUG-09 FIX: KEY_LEFT/RIGHT value editing only when FocusLevel >= 1
    if (item->Type == SETTING_TYPE_LIST) {
      if (IsKeyPressed(KEY_LEFT)) {
        if (item->Current > 0)
          item->Current--;
        else
          item->Current = item->OptionsCount - 1;
        if (r->OnValueChanged)
          r->OnValueChanged(r->callbackCtx, idx);
        if (r->OnApply)
          r->OnApply(r->callbackCtx);
      }
      if (IsKeyPressed(KEY_RIGHT)) {
        if (item->Current < item->OptionsCount - 1)
          item->Current++;
        else
          item->Current = 0;
        if (r->OnValueChanged)
          r->OnValueChanged(r->callbackCtx, idx);
        if (r->OnApply)
          r->OnApply(r->callbackCtx);
      }
      if (IsKeyPressed(KEY_ENTER)) {
        if (strcmp(item->Label, "AUDIO DEVICE") == 0) {
          Settings_RefreshAudioDevices(r->State);
        }
        r->State->IsDropdownOpen = true;
        r->State->DropdownItemIdx = idx;
        r->State->DropdownScroll = 0;
        r->State->FocusLevel = 2;
        return 0; // BUG-10 FIX: return here to prevent global ENTER OnApply
                  // double-fire
      }
    } else if (item->Type == SETTING_TYPE_KNOB) {
      float step =
          (item->Step > 0) ? item->Step : (item->Max - item->Min) / 20.0f;

      // Discrete step on press
      if (IsKeyPressed(KEY_LEFT)) {
        item->Value -= step;
        if (item->Value < item->Min)
          item->Value = item->Min;
        if (r->OnApply)
          r->OnApply(r->callbackCtx);
      }
      if (IsKeyPressed(KEY_RIGHT)) {
        item->Value += step;
        if (item->Value > item->Max)
          item->Value = item->Max;
        if (r->OnApply)
          r->OnApply(r->callbackCtx);
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
        if (r->OnApply)
          r->OnApply(r->callbackCtx);
      }
      
      if (IsKeyPressed(KEY_ENTER)) {
        r->State->IsSliderModalOpen = true;
        r->State->SliderItemIdx = idx;
        return 0; // Prevent fall-through
      }
    } else if (item->Type == SETTING_TYPE_ACTION) {
      if (IsKeyPressed(KEY_ENTER)) {
        if (item->Category == SETTING_CAT_CONTROLLERS &&
            idx >= MIDI_MAPPING_START_IDX) {
          r->State->IsEditMappingOpen = true;
          r->State->EditMappingItemIdx = idx;
        } else if (idx == 20) {
          r->State->IsMappingListOpen = true;
          r->State->MappingListScroll = 0;
          r->State->MappingListCursorPos = 0;
        } else {
          if (strcmp(item->Label, "LOAD DEFAULT JOGWHEEL SETTINGS") == 0) {
              r->State->IsConfirmPopupOpen = true;
              r->State->ConfirmActionIdx = idx;
              strncpy(r->State->ConfirmMessage, "Are you sure you want to load default jogwheel settings?", 127);
          } else {
              if (r->OnAction) r->OnAction(r->callbackCtx, idx);
          }
        }
        return 0; // Prevent fall-through to global Enter/Apply handler
      }
    }
  }

  // BUG-17 FIX: Reset FocusLevel and scroll on tab switch
  if (IsKeyPressed(KEY_TAB)) {
    r->State->SelectedTab = (r->State->SelectedTab + 1) % SETTING_CAT_COUNT;
    r->State->Scroll = 0;
    r->State->CursorPos = 0;
    r->State->VisualScroll = 0;
    r->State->FocusLevel = 0;
  }

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
    if (r->State->FocusLevel > 0) {
      // BUG-10 FIX: Backspace steps back through focus levels before closing
      r->State->FocusLevel--;
      if (r->State->IsDropdownOpen) {
        r->State->IsDropdownOpen = false;
      }
    } else {
      if (r->OnClose)
        r->OnClose(r->callbackCtx);
    }
  }

  // BUG-10 FIX: Global ENTER only applies when FocusLevel == 0 (tab mode)
  if (r->State->FocusLevel == 0 && IsKeyPressed(KEY_ENTER)) {
    r->State->FocusLevel = 1;
    r->State->CursorPos = 0;
    r->State->Scroll = 0;
    r->State->VisualScroll = 0;
  }
  return 0;
}

static void Settings_Draw(Component *base) {
  SettingsRenderer *r = (SettingsRenderer *)base;
  if (!r->State->IsActive)
    return;

  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  DrawRectangle(0, 0, SCREEN_WIDTH, viewH, Theme.BgMain);

  Font faceXS = UIFonts_GetFace(S(9));
  Font faceSm = UIFonts_GetFace(S(11));
  Font faceMd = UIFonts_GetFace(S(13));
  Font faceIcon = UIFonts_GetIcon(S(12));
  Font faceIconSm = UIFonts_GetIcon(S(10));

  float bottomH = (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(46.0f) : 0.0f;
  float divY = viewH - bottomH;
  float tabH = S(28.0f);
  float rowH =
      (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) ? S(38.0f) : S(32.0f);

  // Draw Tabs with state-of-the-art visual style
  DrawRectangle(0, TOP_BAR_H, SCREEN_WIDTH, tabH, Theme.BgPanelAlt);
  const char *tabs[] = {"DECK",   "AUDIO",       "VIEW",
                        "SYSTEM", "CONTROLLERS", "JOG"};
  float tabW = SCREEN_WIDTH / (float)SETTING_CAT_COUNT;
  for (int i = 0; i < SETTING_CAT_COUNT; i++) {
    Rectangle tRect = {i * tabW, TOP_BAR_H, tabW, tabH};
    if (r->State->SelectedTab == i) {
      bool isTabFocused = (r->State->FocusLevel == 0);
      Color fillClr =
          isTabFocused ? Fade(Theme.AccentOrange, 0.3f) : Fade(Theme.AccentOrange, 0.15f);
      DrawRectangleRec(tRect, fillClr);
      DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8), S(11),
                      Theme.AccentOrange);
      DrawRectangle(i * tabW, TOP_BAR_H + tabH - S(3), tabW, S(3), Theme.AccentOrange);
      if (isTabFocused) {
        DrawRectangleLinesEx((Rectangle){i * tabW + S(2), TOP_BAR_H + S(2),
                                         tabW - S(4), tabH - S(4)},
                             S(1.5f), Theme.AccentOrange);
      }
    } else {
      Vector2 mouse = Input_GetPointerPos();
      if (CheckCollisionPointRec(mouse, tRect)) {
        DrawRectangleRec(tRect, Theme.HoverSubtle);
        DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8),
                        S(11), Theme.TextPrimary);
      } else {
        DrawCentredText(tabs[i], faceSm, i * tabW, tabW, TOP_BAR_H + S(8),
                        S(11), Theme.TextSecondary);
      }
    }
    if (i > 0)
      DrawLine(i * tabW, TOP_BAR_H + S(4), i * tabW, TOP_BAR_H + tabH - S(4),
               Theme.TextSecondary);
  }
  DrawLine(0, TOP_BAR_H + tabH, SCREEN_WIDTH, TOP_BAR_H + tabH, Theme.AccentOrange);

  float listY = TOP_BAR_H + tabH;

  // Get filtered indices (Hides individual mapping entries in main view)
  int filteredIndices[MAX_SETTINGS_ITEMS];
  int filteredCount = 0;
  for (int i = 0; i < r->State->ItemsCount; i++) {
    if (r->State->Items[i].Category == (SettingCategory)r->State->SelectedTab) {
      if (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) {
        if (strcmp(r->State->Items[i].Label, "CONNECTED DEVICE") == 0 ||
            strcmp(r->State->Items[i].Label, "MAPPING PRESET") == 0) {
          filteredIndices[filteredCount++] = i;
        }
      } else {
        filteredIndices[filteredCount++] = i;
      }
    }
  }

  float effectiveListY = listY;
  int visibleRows = (int)((divY - effectiveListY) / rowH);

  // Layout params
  float labelX = S(20);
  float valueWidth = S(180);
  float valueX = SCREEN_WIDTH - valueWidth - S(20);

  BeginScissorMode(0, (int)effectiveListY, (int)SCREEN_WIDTH,
                   (int)(divY - effectiveListY));

  float pixelOffset = fmodf(r->State->VisualScroll, rowH);

  for (int i = 0; i < visibleRows + 1; i++) {
    int idx_f = r->State->Scroll + i;
    if (idx_f >= filteredCount)
      break;

    int idx = filteredIndices[idx_f];
    SettingItem *item = &r->State->Items[idx];
    float ry = effectiveListY - pixelOffset + (i * rowH);

    bool selected = (r->State->FocusLevel >= 1 && i == r->State->CursorPos);
    bool isEditingThis = (r->State->FocusLevel == 2 && selected);

    bool isHover = false;
    if (Input_IsDown() && CheckCollisionPointRec(Input_GetPointerPos(), (Rectangle){S(5), ry, SCREEN_WIDTH - S(10), rowH})) {
      isHover = true;
    }

    if (isEditingThis) {
      DrawRectangleRounded(
          (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
          0.15f, 4, Fade(Theme.AccentYellow, 0.35f));
      DrawRectangleRoundedLines(
          (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
          0.15f, 4, 2.0f, Theme.AccentYellow);
      DrawCircle(S(14), ry + (rowH / 2.0f), S(3.5f), Theme.AccentYellow);
    } else if (selected) {
      DrawRectangleRounded(
          (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
          0.15f, 4, Theme.DeckActiveBg);
      DrawRectangleRoundedLines(
          (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
          0.15f, 4, 1.0f, Theme.AccentOrange);
      DrawCircle(S(14), ry + (rowH / 2.0f), S(3.5f), Theme.AccentOrange);
    } else if (isHover) {
      DrawRectangleRounded(
          (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
          0.15f, 4, Theme.HoverActive);
    } else if (idx_f % 2 == 0) {
      DrawRectangleRounded(
          (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
          0.15f, 4, Theme.BorderDefault);
    } else {
      DrawRectangleRounded(
          (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
          0.15f, 4, Theme.BgMain);
    }

    // Label with clipping (using software truncation to avoid nested scissor
    // bugs)
    float labelLimit = valueX - S(10);
    UIDrawTextTruncated(item->Label, faceMd, labelX + S(12),
                        ry + (rowH / 2.0f) - S(7), S(13), Theme.TextPrimary,
                        labelLimit - (labelX + S(12)));

    if (item->Type == SETTING_TYPE_LIST) {
      const char *valStr = "";
      if (item->Current < item->OptionsCount)
        valStr = item->Options[item->Current];

      float innerValX = valueX + S(25);
      float innerValW = valueWidth - S(50);

      if (selected && item->OptionsCount > 1) {
        DrawSelectionTriangleEx(valueX + S(10), ry + (rowH / 2.0f), S(7), 1,
                                Theme.AccentOrange);
        DrawSelectionTriangleEx(valueX + valueWidth - S(10), ry + (rowH / 2.0f),
                                S(7), 0, Theme.AccentOrange);
      }

      if (item->Category == SETTING_CAT_CONTROLLERS) {
        // Highlight the preset selection row more prominently
        DrawRectangle(valueX, ry + S(4), valueWidth, rowH - S(8), Theme.BgPanel);
        DrawCentredText(valStr, faceMd, valueX, valueWidth,
                        ry + (rowH / 2.0f) - S(7), S(12), Theme.AccentOrange);
      } else {
        // DrawCentredText internally uses UIDrawTextTruncated, so we don't need
        // scissor mode
        DrawCentredText(valStr, faceMd, innerValX, innerValW,
                        ry + (rowH / 2.0f) - S(7), S(13), Theme.AccentOrange);
      }

    } else if (item->Type == SETTING_TYPE_KNOB) {
      UIDrawKnob(valueX + valueWidth - S(95), ry + (rowH / 2.0f), S(9),
                 item->Value, item->Min, item->Max, NULL, Theme.AccentOrange, false);

      char valBuf[32];
      if (item->Value == (int)item->Value)
        sprintf(valBuf, "%d", (int)item->Value);
      else if (item->Step < 0.01f || fabsf(item->Value) < 0.1f)
        sprintf(valBuf, "%.3f", item->Value);
      else if (fabsf(item->Value * 4.0f - roundf(item->Value * 4.0f)) < 0.01f ||
               item->Step < 0.1f)
        sprintf(valBuf, "%.2f", item->Value);
      else
        sprintf(valBuf, "%.1f", item->Value);

      if (item->Unit[0] != '\0') {
        char fullBuf[64];
        const char *uStr =
            (strcmp(item->Unit, "Bar") == 0 && item->Value > 1.0f) ? "Bars"
                                                                   : item->Unit;
        sprintf(fullBuf, "%s %s", valBuf, uStr);
        UIDrawText(fullBuf, faceMd, valueX + valueWidth - S(80),
                   ry + (rowH / 2.0f) - S(7), S(13), Theme.AccentOrange);
      } else {
        UIDrawText(valBuf, faceMd, valueX + valueWidth - S(80),
                   ry + (rowH / 2.0f) - S(7), S(13), Theme.AccentOrange);
      }
    } else if (item->Type == SETTING_TYPE_ACTION) {
      if (item->Category == SETTING_CAT_CONTROLLERS) {
        // Table Layout for Controllers with column lines
        if (idx >= MIDI_MAPPING_START_IDX) { // Mapping entries
          // Column Dividers
          DrawLine(SCREEN_WIDTH - S(160), ry + S(2), SCREEN_WIDTH - S(160),
                   ry + rowH - S(2), Theme.TextSecondary);
          DrawLine(SCREEN_WIDTH - S(75), ry + S(2), SCREEN_WIDTH - S(75),
                   ry + rowH - S(2), Theme.TextSecondary);

          bool isLearning =
              (r->State->IsLearningMidi && r->State->LearningItemIdx == idx);

          if (isLearning) {
            float alpha = (sinf(GetTime() * 12.0f) * 0.4f + 0.6f);
            Color pulseColor = {0, 121, 241, (unsigned char)(alpha * 255)};
            DrawRectangleRounded(
                (Rectangle){S(6), ry + S(3), SCREEN_WIDTH - S(12), rowH - S(6)},
                0.2f, 4, Fade(Theme.AccentBlue, 0.8f));
            DrawRectangleRoundedLines(
                (Rectangle){S(6), ry + S(3), SCREEN_WIDTH - S(12), rowH - S(6)},
                0.2f, 4, 2.0f, pulseColor);
            DrawCentredText("WAITING FOR MIDI INPUT...", faceSm, 0,
                            SCREEN_WIDTH, ry + (rowH / 2.0f) - S(6), S(11),
                            pulseColor);
          } else {
            // Channel:Msg as a "badge"
            Rectangle badgeRect = {SCREEN_WIDTH - S(160) + S(10),
                                   ry + (rowH - S(20)) / 2.0f, S(65), S(20)};
            DrawRectangleRounded(badgeRect, 0.4f, 4, Theme.BgPanel);
            DrawRectangleRoundedLines(badgeRect, 0.4f, 4, 1.0f, Theme.TextSecondary);
            DrawCentredText(item->Unit, faceSm, badgeRect.x, badgeRect.width,
                            ry + (rowH / 2.0f) - S(6), S(10), Theme.AccentOrange);

            // Type Badge
            Color typeColor = Theme.TextSecondary;
            const char *typeIcon = "";
            if (strcmp(item->Options[0], "SCRIPT") == 0) {
              typeColor = Theme.AccentBlue;
              typeIcon = "\uf121";
            } else if (strcmp(item->Options[0], "REL") == 0) {
              typeColor = Theme.AccentOrange;
              typeIcon = "\uf01e";
            } else if (strcmp(item->Options[0], "14BIT") == 0) {
              typeColor = Theme.AccentGreen;
              typeIcon = "\uf0c9";
            }

            // Type Icon & Text
            UIDrawText(typeIcon, faceIconSm, SCREEN_WIDTH - S(72),
                       ry + (rowH / 2.0f) - S(5), S(10), typeColor);
            UIDrawText(item->Options[0], faceSm, SCREEN_WIDTH - S(55),
                       ry + (rowH / 2.0f) - S(6), S(9), typeColor);
          }

        } else { // Preset Selection Actions (CREATE/SAVE)
          UIDrawText(item->Unit, faceMd, valueX + valueWidth - S(90),
                     ry + (rowH / 2.0f) - S(7), S(13), Theme.AccentOrange);
          UIDrawText("\uf35a", faceIcon, valueX + valueWidth - S(25),
                     ry + (rowH / 2.0f) - S(6), S(12), Theme.TextSecondary);
        }
      } else if (strcmp(item->Label, "ABOUT") != 0 &&
                 strcmp(item->Label, "CREDITS") != 0 &&
                 strcmp(item->Label, "EXIT APPLICATION") != 0) {
        UIDrawText("\uf2f5", faceIcon, valueX + valueWidth - S(35),
                   ry + (rowH / 2.0f) - S(6), S(12), Theme.AccentOrange);
      }
    }
  }

  EndScissorMode();

  DrawScrollbar(SCREEN_WIDTH - S(2.5f), listY, S(2), divY - listY,
                filteredCount, r->State->Scroll, visibleRows);

  // List extending to bottom
  if (r->State->SelectedTab == SETTING_CAT_CONTROLLERS) {
    uint8_t s, m;
    Rectangle monRect = {S(115), divY + S(24), S(135), S(16)};
    DrawRectangleRounded(monRect, 0.2f, 4, Theme.BgMain);
    DrawRectangleRoundedLines(monRect, 0.2f, 4, 1.0f, Theme.BgPanel);

    if (MIDI_PeekLastMessage(&s, &m)) {
      char monBuf[64];
      snprintf(monBuf, 64, "MIDI: 0x%02X : 0x%02X", s, m);
      UIDrawText(monBuf, faceXS, monRect.x + S(18), divY + S(27), S(9),
                 Theme.AccentGreen);
      DrawCircle(monRect.x + S(10), divY + S(32), S(3), Theme.AccentGreen);
    } else {
      UIDrawText("MIDI IDLE", faceXS, monRect.x + S(18), divY + S(27), S(9),
                 Theme.BgPanel);
      DrawCircle(monRect.x + S(10), divY + S(32), S(3), Theme.BgPanel);
    }

    UIDrawText("Hardware Auto-Mapping Active", faceXS, S(265), divY + S(27),
               S(9), Theme.TextSecondary);
  }

  if (r->State->IsDropdownOpen) {
    DrawRectangle(0, 0, SCREEN_WIDTH, viewH, Theme.BgOverlay);
    SettingItem *item = &r->State->Items[r->State->DropdownItemIdx];
    float dropdownW = S(240.0f);
    float opHeight = S(40.0f);
    float contentH = item->OptionsCount * opHeight;
    float dropdownH = contentH > (viewH * 0.7f) ? (viewH * 0.7f) : contentH;
    float dropdownX = (SCREEN_WIDTH - dropdownW) / 2.0f;
    float dropdownY = (viewH - dropdownH) / 2.0f;

    Rectangle dropRect = {dropdownX, dropdownY, dropdownW, dropdownH};
    BeginScissorMode((int)dropRect.x, (int)dropRect.y, (int)dropRect.width,
                     (int)dropRect.height);
    DrawRectangleRec(dropRect, Theme.BgMain);
    float cy = dropdownY - r->State->DropdownScroll;
    for (int i = 0; i < item->OptionsCount; i++) {
      Rectangle opRect = {dropdownX, cy, dropdownW, opHeight};
      if (cy + opHeight > dropdownY && cy < dropdownY + dropdownH) {
        if (item->Current == i)
          DrawRectangleRec(opRect, Theme.TextSecondary);
        else
          DrawRectangleRec(opRect, Theme.BorderDefault);
        DrawRectangleLinesEx(opRect, 1, Theme.BorderDefault);
        UIDrawTextTruncated(
            item->Options[i], faceMd, dropdownX + S(20), cy + S(12), S(15),
            (item->Current == i) ? Theme.AccentOrange : Theme.TextPrimary, dropdownW - S(40));
      }
      cy += opHeight;
    }
    EndScissorMode();
    DrawRectangleLinesEx(dropRect, 2, Theme.AccentOrange);
    if (contentH > dropdownH) {
      float sbY = dropdownY + (r->State->DropdownScroll / contentH) * dropdownH;
      float sbH = (dropdownH / contentH) * dropdownH;
      DrawRectangle((int)(dropdownX + dropdownW - S(4)), (int)sbY, (int)S(4),
                    (int)sbH, Theme.AccentOrange);
    }
  }

  if (r->State->IsLearningMidi) {
    // Dark glassmorphic backdrop
    DrawRectangle(0, 0, SCREEN_WIDTH, viewH, Theme.BgModal);

    // Dialog Card layout
    float cardW = S(320);
    float cardH = S(180);
    Rectangle cardRect = {(SCREEN_WIDTH - cardW) / 2.0f, (viewH - cardH) / 2.0f,
                          cardW, cardH};

    // Sharp non-rounded modal window frame
    DrawRectangleRec(cardRect, Theme.BgMain);
    DrawRectangleLinesEx(cardRect, 2.0f, Theme.AccentOrange);

    // Bouncing MIDI connection icon
    float bounce = sinf(GetTime() * 8.0f) * S(4.0f);
    UIDrawText("\uf121", UIFonts_GetIcon(S(28)),
               cardRect.x + cardW / 2.0f - S(14), cardRect.y + S(20) + bounce,
               S(28), Theme.AccentOrange);

    // Texts
    DrawCentredText("MIDI LEARN ACTIVE", faceMd, cardRect.x, cardRect.width,
                    cardRect.y + S(65), S(14), Theme.TextPrimary);
    DrawCentredText("Move a knob, fader, or press a pad...", faceSm, cardRect.x,
                    cardRect.width, cardRect.y + S(95), S(11), Theme.TextSecondary);

    // Bouncing signal pulse status
    float alpha = (sinf(GetTime() * 10.0f) * 0.3f + 0.7f);
    Color pulseColor = {0, 220, 100, (unsigned char)(alpha * 255)};
    DrawCentredText("LISTENING FOR MIDI EVENT...", faceXS, cardRect.x,
                    cardRect.width, cardRect.y + S(125), S(10), pulseColor);

    DrawCentredText("Press [ESC] to cancel", faceXS, cardRect.x, cardRect.width,
                    cardRect.y + S(150), S(9), Theme.TextSecondary);
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
    DrawRectangle(0, 0, SCREEN_WIDTH, viewH, Theme.BgMain);

    // Draw Window Title
    DrawRectangle(0, TOP_BAR_H, SCREEN_WIDTH, S(28.0f), Theme.BgPanelAlt);
    UIDrawText("EDIT PRESET MAPPINGS", faceSm, S(20), TOP_BAR_H + S(8), S(11),
               Theme.AccentOrange);
    DrawLine(0, TOP_BAR_H + S(28.0f), SCREEN_WIDTH, TOP_BAR_H + S(28.0f),
             Theme.AccentOrange);

    float listY = TOP_BAR_H + S(28.0f);
    float rowH = S(32.0f);
    int visibleRows = (int)((divY - listY) / rowH);

    // Render mapping items in a beautiful list without table head
    BeginScissorMode(0, (int)listY, (int)SCREEN_WIDTH, (int)(divY - listY));

    if (mapCount == 0) {
      // Show empty state
      float cx = SCREEN_WIDTH / 2.0f;
      float cy = listY + (divY - listY) / 2.0f;
      UIDrawText("\uf05a", UIFonts_GetIcon(S(30)), cx - S(15), cy - S(35),
                 S(30), Theme.TextSecondary);
      DrawCentredText("NO MAPPING DATA LOADED", faceSm, 0, SCREEN_WIDTH,
                      cy - S(2), S(12), Theme.TextSecondary);
      DrawCentredText("Select a Preset from the Controllers tab first.", faceXS,
                      0, SCREEN_WIDTH, cy + S(14), S(10), Theme.TextSecondary);
    }

    for (int i = 0; i < visibleRows; i++) {
      int idx_f = r->State->MappingListScroll + i;
      if (idx_f >= mapCount)
        break;

      int idx = mapIndices[idx_f];
      SettingItem *item = &r->State->Items[idx];
      float ry = listY + (i * rowH);

      bool selected = (i == r->State->MappingListCursorPos);
      if (selected) {
        DrawRectangleRounded(
            (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
            0.15f, 4, Theme.DeckActiveBg);
        DrawRectangleRoundedLines(
            (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
            0.15f, 4, 1.0f, Theme.AccentOrange);
        DrawCircle(S(14), ry + (rowH / 2.0f), S(3.5f), Theme.AccentOrange);
      } else if (idx_f % 2 == 0) {
        DrawRectangleRounded(
            (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
            0.15f, 4, Theme.BorderDefault);
      } else {
        DrawRectangleRounded(
            (Rectangle){S(5), ry + S(2), SCREEN_WIDTH - S(10), rowH - S(4)},
            0.15f, 4, Theme.BgMain);
      }

      // Label
      UIDrawText(item->Label, faceMd, S(32), ry + (rowH / 2.0f) - S(7), S(13),
                 Theme.TextPrimary);

      // Mapped Address Badge
      Rectangle badgeRect = {SCREEN_WIDTH - S(160) + S(10),
                             ry + (rowH - S(20)) / 2.0f, S(65), S(20)};
      DrawRectangleRounded(badgeRect, 0.4f, 4, Theme.BgPanel);
      DrawRectangleRoundedLines(badgeRect, 0.4f, 4, 1.0f, Theme.TextSecondary);
      DrawCentredText(item->Unit, faceSm, badgeRect.x, badgeRect.width,
                      ry + (rowH / 2.0f) - S(6), S(10), Theme.AccentOrange);

      // Type Badge
      Color typeColor = Theme.TextSecondary;
      const char *typeIcon = "";
      if (strcmp(item->Options[0], "SCRIPT") == 0) {
        typeColor = Theme.AccentBlue;
        typeIcon = "\uf121";
      } else if (strcmp(item->Options[0], "REL") == 0) {
        typeColor = Theme.AccentOrange;
        typeIcon = "\uf01e";
      } else if (strcmp(item->Options[0], "14BIT") == 0) {
        typeColor = Theme.AccentGreen;
        typeIcon = "\uf0c9";
      }

      UIDrawText(typeIcon, faceIconSm, SCREEN_WIDTH - S(72),
                 ry + (rowH / 2.0f) - S(5), S(10), typeColor);
      UIDrawText(item->Options[0], faceSm, SCREEN_WIDTH - S(55),
                 ry + (rowH / 2.0f) - S(6), S(9), typeColor);
    }
    EndScissorMode();

    // Scrollbar
    DrawScrollbar(SCREEN_WIDTH - S(2.5f), listY, S(2), divY - listY, mapCount,
                  r->State->MappingListScroll, visibleRows);

    // Bottom Background & Details Banner
    DrawRectangle(0, divY, SCREEN_WIDTH, bottomH, Theme.BorderDefault);
    DrawLine(0, divY, SCREEN_WIDTH, divY, Theme.TextSecondary);

    // Details Banner for currently selected sub-item
    SettingItem *selectedItem = NULL;
    int sIdx = r->State->MappingListScroll + r->State->MappingListCursorPos;
    if (sIdx >= 0 && sIdx < mapCount) {
      selectedItem = &r->State->Items[mapIndices[sIdx]];
    }
    char detailBuf[128] = "";
    if (selectedItem) {
      const char *mapType = (selectedItem->OptionsCount > 0)
                                ? selectedItem->Options[0]
                                : "UNKNOWN";
      const char *mapAddress =
          (selectedItem->Unit[0] != '\0') ? selectedItem->Unit : "UNMAPPED";
      snprintf(detailBuf, sizeof(detailBuf),
               "Selected Target: %s  |  MIDI Address: %s  |  Type: %s",
               selectedItem->Label, mapAddress, mapType);
    }

    DrawRectangle(0, divY, SCREEN_WIDTH, S(18), Theme.BgPanel);
    UIDrawText(detailBuf[0] != '\0' ? detailBuf : "No target selected", faceXS,
               S(15), divY + S(5), S(9.5f), Theme.AccentOrange);
    DrawLine(0, divY + S(18), SCREEN_WIDTH, divY + S(18), Theme.TextSecondary);

    // BACK Button
    Rectangle backRect = {S(15), divY + S(23), S(90), S(18)};
    DrawRectangleRounded(backRect, 0.5f, 4, Theme.AccentBlue);
    DrawRectangleRoundedLines(backRect, 0.5f, 4, 1.0f, Theme.TextPrimary);
    DrawCentredText("BACK", faceSm, backRect.x, backRect.width, divY + S(26),
                    S(11), Theme.TextPrimary);

    // CREATE NEW Action Button
    Rectangle createRect = {SCREEN_WIDTH - S(250), divY + S(23), S(110), S(18)};
    DrawRectangleRounded(createRect, 0.5f, 4, Theme.BgPanel);
    DrawRectangleRoundedLines(createRect, 0.5f, 4, 1.0f, Theme.TextSecondary);
    DrawCentredText("CREATE NEW TEMPLATE", faceXS, createRect.x,
                    createRect.width, divY + S(27), S(8.5f), Theme.AccentOrange);

    // SAVE AS CUSTOM Action Button
    Rectangle saveRect = {SCREEN_WIDTH - S(130), divY + S(23), S(115), S(18)};
    DrawRectangleRounded(saveRect, 0.5f, 4, Theme.BgPanel);
    DrawRectangleRoundedLines(saveRect, 0.5f, 4, 1.0f, Theme.TextSecondary);
    DrawCentredText("SAVE AS CUSTOM XML", faceXS, saveRect.x, saveRect.width,
                    divY + S(27), S(8.5f), Theme.AccentOrange);

    // Item count string in sub-window
    char countStr[32];
    sprintf(countStr, "%d / %d", sIdx + 1, mapCount);
    UIDrawText(countStr, faceXS, SCREEN_WIDTH / 2.0f - S(80.0f), divY + S(26),
               S(9), Theme.TextSecondary);

    // Tip label in sub-window
    UIDrawText("Tip: Click any target row to edit mapping.", faceXS,
               SCREEN_WIDTH / 2.0f - S(25.0f), divY + S(26), S(9), Theme.TextSecondary);

    // Still draw Edit Modal and MIDI Learn dialog on top if active!
    if (r->State->IsEditMappingOpen) {
      // Handled next
    }
  }

  if (r->State->IsEditMappingOpen) {
    UI_DrawModalBackdrop();

    // Dialog Card layout
    float cardW = S(340.0f);
    float cardH = S(195.0f);
    Rectangle cardRect = {(SCREEN_WIDTH - cardW) / 2.0f, (viewH - cardH) / 2.0f,
                          cardW, cardH};

    Rectangle body = UI_DrawModalFrame(cardRect, "EDIT MIDI MAPPING");

    // Details Info
    SettingItem *item = &r->State->Items[r->State->EditMappingItemIdx];
    char nameBuf[128], addrBuf[128], typeBuf[128];
    snprintf(nameBuf, sizeof(nameBuf), "Target Name: %s", item->Label);
    snprintf(addrBuf, sizeof(addrBuf), "MIDI Bind  : %s", item->Unit);
    snprintf(typeBuf, sizeof(typeBuf), "Action Type: %s", item->Options[0]);

    UIDrawText(nameBuf, faceSm, cardRect.x + S(20), cardRect.y + S(36),
               S(10.5f), Theme.TextPrimary);
    UIDrawText(addrBuf, faceSm, cardRect.x + S(20), cardRect.y + S(54),
               S(10.5f), Theme.TextPrimary);
    UIDrawText(typeBuf, faceSm, cardRect.x + S(20), cardRect.y + S(72),
               S(10.5f), Theme.TextPrimary);

    // 2x2 Larger Sharp Buttons Grid
    float btnW = S(150.0f), btnH = S(28.0f);
    
    Rectangle btn1 = {cardRect.x + S(15), cardRect.y + S(110), btnW, btnH};
    bool btn1Hover = CheckCollisionPointRec(Input_GetPointerPos(), btn1) || r->State->ModalCursorPos == 0;
    DrawRectangleRec(btn1, btn1Hover ? Theme.AccentBlue : Theme.BgPanel);
    DrawRectangleLinesEx(btn1, 1.0f, btn1Hover ? Theme.TextPrimary : Theme.BorderDefault);
    DrawCentredText("START MIDI LEARN", faceXS, btn1.x, btn1.width, btn1.y + S(9), S(9.5f), Theme.TextPrimary);

    Rectangle btn2 = {cardRect.x + S(175), cardRect.y + S(110), btnW, btnH};
    bool btn2Hover = CheckCollisionPointRec(Input_GetPointerPos(), btn2) || r->State->ModalCursorPos == 1;
    DrawRectangleRec(btn2, btn2Hover ? Theme.BgPanel : Theme.BorderDefault);
    DrawRectangleLinesEx(btn2, 1.0f, btn2Hover ? Theme.AccentOrange : Theme.BorderDefault);
    DrawCentredText("CYCLE TYPE", faceXS, btn2.x, btn2.width, btn2.y + S(9), S(9.5f), Theme.AccentOrange);

    Rectangle btn3 = {cardRect.x + S(15), cardRect.y + S(150), btnW, btnH};
    bool btn3Hover = CheckCollisionPointRec(Input_GetPointerPos(), btn3) || r->State->ModalCursorPos == 2;
    DrawRectangleRec(btn3, btn3Hover ? Theme.AccentRed : Fade(Theme.AccentRed, 0.3f));
    DrawRectangleLinesEx(btn3, 1.0f, btn3Hover ? Theme.AccentRed : Theme.BorderDefault);
    DrawCentredText("CLEAR / RESET", faceXS, btn3.x, btn3.width, btn3.y + S(9), S(9.5f), Theme.TextPrimary);

    Rectangle btn4 = {cardRect.x + S(175), cardRect.y + S(150), btnW, btnH};
    bool btn4Hover = CheckCollisionPointRec(Input_GetPointerPos(), btn4) || r->State->ModalCursorPos == 3;
    DrawRectangleRec(btn4, btn4Hover ? Theme.AccentGreen : Fade(Theme.AccentGreen, 0.3f));
    DrawRectangleLinesEx(btn4, 1.0f, btn4Hover ? Theme.TextPrimary : Theme.BorderDefault);
    DrawCentredText("SAVE & CLOSE", faceXS, btn4.x, btn4.width, btn4.y + S(9), S(9.5f), Theme.TextPrimary);
    
    bool enterPressed = r->State->MidiRequestEnter;
    if (Input_CheckPress(btn1) || (enterPressed && r->State->ModalCursorPos == 0)) {
        g_lastSettingsClickTime = GetTime();
        r->State->IsLearningMidi = true;
        r->State->LearningItemIdx = r->State->EditMappingItemIdx;
        uint8_t s, m;
        while (MIDI_GetLastMessage(&s, &m)); // Flush
        r->State->MidiRequestEnter = false;
    } else if (Input_CheckPress(btn2) || (enterPressed && r->State->ModalCursorPos == 1)) {
        g_lastSettingsClickTime = GetTime();
        MidiMapping *map = MIDI_GetGlobalMapping();
        int mapIdx = r->State->EditMappingItemIdx - MIDI_MAPPING_START_IDX;
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
        r->State->MidiRequestEnter = false;
    } else if (Input_CheckPress(btn3) || (enterPressed && r->State->ModalCursorPos == 2)) {
        g_lastSettingsClickTime = GetTime();
        MidiMapping *map = MIDI_GetGlobalMapping();
        int mapIdx = r->State->EditMappingItemIdx - MIDI_MAPPING_START_IDX;
        if (mapIdx >= 0 && mapIdx < map->count) {
          map->entries[mapIdx].status = 0;
          map->entries[mapIdx].midino = 0;
          strcpy(item->Unit, "0x00:0x00");
          if (r->OnApply) r->OnApply(r->callbackCtx);
        }
        r->State->MidiRequestEnter = false;
    } else if (Input_CheckPress(btn4) || (enterPressed && r->State->ModalCursorPos == 3)) {
        g_lastSettingsClickTime = GetTime();
        r->State->IsEditMappingOpen = false;
        r->State->MidiRequestEnter = false;
    }
  }

  // Render System Info Modal Popup
  if (r->State->IsSystemInfoOpen) {
    float winW = SCREEN_WIDTH;
    float viewH = SCREEN_HEIGHT - DECK_STR_H;

    DrawRectangle(0, 0, (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT,
                  Theme.BgOverlay);

    float modalW = S(380.0f);
    float modalH = S(240.0f);
    float modalX = (winW - modalW) / 2.0f;
    float modalY = (viewH - modalH) / 2.0f;

    Rectangle modalRect = {modalX, modalY, modalW, modalH};
    // Sharp non-rounded modal window frame
    DrawRectangleRec(modalRect, Theme.BorderDefault);
    DrawRectangleLinesEx(modalRect, 2.0f, Theme.AccentOrange);

    // Header
    DrawRectangle(modalX, modalY, modalW, S(30.0f), Theme.BgPanel);
    UIDrawText("\uf085", faceIconSm, modalX + S(12.0f), modalY + S(8.0f), S(14),
               Theme.AccentOrange);
    UIDrawText("SYSTEM USAGE & SPECIFICATIONS", faceSm, modalX + S(34.0f),
               modalY + S(8.0f), S(11), Theme.AccentOrange);
    DrawLine(modalX, modalY + S(30.0f), modalX + modalW, modalY + S(30.0f),
             Theme.AccentOrange);

    // Close Button 'X'
    Rectangle closeBtn = {modalX + modalW - S(32.0f), modalY + S(3.0f), S(28.0f), S(24.0f)};
    bool closeHover = CheckCollisionPointRec(Input_GetPointerPos(), closeBtn) || r->State->ModalCursorPos == 0;
    DrawRectangleRec(closeBtn, closeHover ? Theme.AccentRed : BLANK);
    UIDrawText("\uf00d", faceIconSm, closeBtn.x + S(8.0f), closeBtn.y + S(5.0f), S(14), closeHover ? Theme.TextPrimary : Theme.TextSecondary);
    
    if (Input_CheckPress(closeBtn) || (r->State->MidiRequestEnter && r->State->ModalCursorPos == 0)) {
        r->State->IsSystemInfoOpen = false;
        r->State->MidiRequestEnter = false;
    }

    // 1. CPU Load
    float cpuUsage = r->State->CPUUsage;
    if (cpuUsage < 0.0f)
      cpuUsage = 0.0f;
    if (cpuUsage > 1.0f)
      cpuUsage = 1.0f;

    float row1Y = modalY + S(36.0f);
    UIDrawText("CPU Load:", faceSm, modalX + S(16.0f), row1Y, S(11),
               Theme.TextPrimary);

    float cpuBarX = modalX + S(98.0f);
    float cpuBarW = S(190.0f);
    float cpuBarH = S(11.0f);
    DrawRectangle(cpuBarX, row1Y + S(1.0f), cpuBarW, cpuBarH, Theme.BgPanel);
    DrawRectangleLinesEx((Rectangle){cpuBarX, row1Y + S(1.0f), cpuBarW, cpuBarH}, 1.0f, Theme.BorderDefault);

    float cpuFill = (cpuBarW - S(2.0f)) * cpuUsage;
    if (cpuFill > S(1.0f)) {
      Color cpuCol = (cpuUsage > 0.85f)
                         ? Theme.AccentRed
                         : ((cpuUsage > 0.60f) ? Theme.AccentOrange : Theme.AccentGreen);
      DrawRectangle(cpuBarX + S(1.0f), row1Y + S(2.0f), cpuFill, cpuBarH - S(2.0f), cpuCol);
    }
    char cpuBuf[32];
    snprintf(cpuBuf, sizeof(cpuBuf), "%d%%", (int)(cpuUsage * 100.0f));
    UIDrawText(cpuBuf, faceSm, cpuBarX + cpuBarW + S(6.0f), row1Y, S(11),
               Theme.AccentOrange);

    // 2. CPU Specs
    float row2Y = modalY + S(54.0f);
    char cpuSpecText[96];
    int cores = r->State->CPUCores > 0 ? r->State->CPUCores : 4;
    float ghz = r->State->CPUMhz > 0 ? (r->State->CPUMhz > 10.0f ? r->State->CPUMhz / 1000.0f : r->State->CPUMhz) : 2.0f;
    snprintf(cpuSpecText, sizeof(cpuSpecText), "Processor: %d Cores @ %.2f GHz", cores, ghz);
    UIDrawText("\uf2db", faceIconSm, modalX + S(16.0f), row2Y, S(11), Theme.AccentOrange);
    UIDrawText(cpuSpecText, faceSm, modalX + S(34.0f), row2Y, S(10.5f), Theme.TextPrimary);

    // 3. RAM Memory (Global System RAM)
    float ramUsed = r->State->RAMUsageMB;
    float ramTotal = r->State->RAMTotalMB > 0 ? r->State->RAMTotalMB : 1024.0f;
    float ramFree =
        (r->State->RAMFreeMB > 0) ? r->State->RAMFreeMB : (ramTotal - ramUsed);
    if (ramFree < 0)
      ramFree = 0;
    float ramRatio = ramUsed / ramTotal;
    if (ramRatio > 1.0f)
      ramRatio = 1.0f;

    float row3Y = modalY + S(72.0f);
    UIDrawText("RAM (Global):", faceSm, modalX + S(16.0f), row3Y, S(11), Theme.TextPrimary);

    float ramBarX = modalX + S(98.0f);
    float ramBarW = S(190.0f);
    float ramBarH = S(11.0f);
    DrawRectangle(ramBarX, row3Y + S(1.0f), ramBarW, ramBarH, Theme.BgPanel);
    DrawRectangleLinesEx((Rectangle){ramBarX, row3Y + S(1.0f), ramBarW, ramBarH}, 1.0f, Theme.BorderDefault);

    float ramFill = (ramBarW - S(2.0f)) * ramRatio;
    if (ramFill > S(1.0f)) {
      Color ramCol = (ramRatio > 0.85f) ? Theme.AccentRed : Theme.AccentOrange;
      DrawRectangle(ramBarX + S(1.0f), row3Y + S(2.0f), ramFill, ramBarH - S(2.0f), ramCol);
    }
    char ramPct[32];
    snprintf(ramPct, sizeof(ramPct), "%d%%", (int)(ramRatio * 100.0f));
    UIDrawText(ramPct, faceSm, ramBarX + ramBarW + S(6.0f), row3Y, S(11),
               Theme.AccentOrange);

    char ramText[128];
    snprintf(ramText, sizeof(ramText),
             "Used: %d MB   Free: %d MB   Total: %d MB", (int)ramUsed,
             (int)ramFree, (int)ramTotal);
    UIDrawText(ramText, faceXS, modalX + S(16.0f), row3Y + S(15.0f), S(9.5f),
               Theme.TextSecondary);

    // 4. RAM (App Only)
    float row4Y = modalY + S(106.0f);
    char appRamText[64];
    snprintf(appRamText, sizeof(appRamText),
             "App Memory Usage: %d MB (Process)", (int)r->State->RAMAppMB);
    UIDrawText("\uf538", faceIconSm, modalX + S(16.0f), row4Y, S(11),
               Theme.AccentOrange);
    UIDrawText(appRamText, faceSm, modalX + S(34.0f), row4Y, S(10.5f),
               Theme.TextPrimary);

    // 5. Audio Engine Metrics
    float row5Y = modalY + S(126.0f);
    char audioText[128];
    int sr = r->State->AudioSampleRate > 0 ? r->State->AudioSampleRate : 44100;
    int buf = r->State->AudioBufferSize > 0 ? r->State->AudioBufferSize : 512;
    float lat = r->State->AudioLatencyMs > 0.0f ? r->State->AudioLatencyMs : (((float)buf / (float)sr) * 1000.0f);
    snprintf(audioText, sizeof(audioText),
             "Audio Engine: %d Hz  |  Buffer: %d frames (%.1f ms)",
             sr, buf, lat);
    UIDrawText("\uf028", faceIconSm, modalX + S(16.0f), row5Y, S(11),
               Theme.AccentBlue);
    UIDrawText(audioText, faceSm, modalX + S(34.0f), row5Y, S(10.5f),
               Theme.TextPrimary);

    // 6. Platform Specifications
    float row6Y = modalY + S(146.0f);
    char specText[128];
    snprintf(specText, sizeof(specText), "OS Platform: %s",
             r->State->OSPlatformStr[0] != '\0' ? r->State->OSPlatformStr
                                                : "Embedded Linux");
    UIDrawText("\uf108", faceIconSm, modalX + S(16.0f), row6Y, S(11),
               Theme.AccentGreen);
    UIDrawText(specText, faceSm, modalX + S(34.0f), row6Y, S(10.5f),
               Theme.TextPrimary);

    // Larger Sharp OK / Close Button
    Rectangle okBtn = {modalX + (modalW - S(120.0f)) / 2.0f,
                       modalY + modalH - S(36.0f), S(120.0f), S(28.0f)};
    DrawRectangleRec(okBtn, Theme.BgPanel);
    DrawRectangleLinesEx(okBtn, 1.5f, Theme.AccentOrange);
    DrawCentredText("CLOSE", faceSm, okBtn.x, okBtn.width, okBtn.y + S(8.0f),
                    S(11), Theme.TextPrimary);
  }

  // Render Slider Modal Popup
  if (r->State->IsSliderModalOpen) {
    SettingItem *item = &r->State->Items[r->State->SliderItemIdx];
    float winW = SCREEN_WIDTH;
    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    DrawRectangle(0, 0, winW, viewH, Theme.BgOverlay);

    float modalW = S(400.0f);
    float modalH = S(160.0f);
    float modalX = (winW - modalW) / 2.0f;
    float modalY = (viewH - modalH) / 2.0f;

    Rectangle modalRect = {modalX, modalY, modalW, modalH};
    DrawRectangleRec(modalRect, Theme.BorderDefault);
    DrawRectangleLinesEx(modalRect, 2.0f, Theme.AccentOrange);

    // Header
    DrawRectangle(modalX, modalY, modalW, S(30.0f), Theme.BgPanel);
    DrawCentredText(item->Label, faceMd, modalX, modalW, modalY + S(8.0f), S(12), Theme.AccentOrange);

    // Slider UI
    float sliderW = modalW - S(60);
    float sliderX = modalX + S(30);
    float sliderY = modalY + S(75);
    
    // Draw track
    DrawRectangle(sliderX, sliderY, sliderW, S(12), Theme.BgPanelAlt);
    DrawRectangleLines(sliderX, sliderY, sliderW, S(12), Theme.BorderDefault);

    // Draw value fill
    float pct = (item->Value - item->Min) / (item->Max - item->Min);
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    float fillW = pct * sliderW;
    DrawRectangle(sliderX, sliderY, fillW, S(12), Theme.AccentOrange);

    // Draw handle
    float hX = sliderX + fillW - S(6);
    DrawRectangle(hX, sliderY - S(8), S(12), S(28), Theme.TextPrimary);

    // Draw value text
    char valBuf[64];
    if (item->Step < 0.01f || fabsf(item->Value) < 0.1f) sprintf(valBuf, "%.3f", item->Value);
    else if (fabsf(item->Value * 4.0f - roundf(item->Value * 4.0f)) < 0.01f || item->Step < 0.1f) sprintf(valBuf, "%.2f", item->Value);
    else sprintf(valBuf, "%.1f", item->Value);
    
    if (item->Unit[0] != '\0') {
      char fBuf[128];
      sprintf(fBuf, "%s %s", valBuf, item->Unit);
      DrawCentredText(fBuf, faceSm, modalX, modalW, modalY + S(45), S(12), Theme.TextPrimary);
    } else {
      DrawCentredText(valBuf, faceSm, modalX, modalW, modalY + S(45), S(12), Theme.TextPrimary);
    }

    // OK Button
    Rectangle okBtn = {modalX + (modalW - S(120))/2.0f, modalY + modalH - S(44), S(120), S(32)};
    bool okHover = CheckCollisionPointRec(Input_GetPointerPos(), okBtn) || r->State->ModalCursorPos == 1;
    DrawRectangleRec(okBtn, okHover ? Theme.BgPanel : Theme.BorderDefault);
    DrawRectangleLinesEx(okBtn, 1.5f, okHover ? Theme.AccentOrange : Theme.BorderDefault);
    DrawCentredText("APPLY", faceSm, okBtn.x, okBtn.width, okBtn.y + S(10), S(11), okHover ? Theme.TextPrimary : Theme.TextSecondary);
    
    if (Input_CheckPress(okBtn) || (r->State->MidiRequestEnter && r->State->ModalCursorPos == 1)) {
        r->State->IsSliderModalOpen = false;
        r->State->MidiRequestEnter = false;
    }
  }

  // Render Confirmation Popup
  if (r->State->IsConfirmPopupOpen) {
    float winW = SCREEN_WIDTH;
    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    DrawRectangle(0, 0, winW, viewH, Theme.BgOverlay);

    float modalW = S(320.0f);
    float modalH = S(160.0f);
    float modalX = (winW - modalW) / 2.0f;
    float modalY = (viewH - modalH) / 2.0f;

    Rectangle modalRect = {modalX, modalY, modalW, modalH};
    DrawRectangleRec(modalRect, Theme.BorderDefault);
    DrawRectangleLinesEx(modalRect, 2.0f, Theme.AccentRed);

    // Header
    DrawRectangle(modalX, modalY, modalW, S(30.0f), Theme.AccentRed);
    DrawCentredText("CONFIRMATION", faceMd, modalX, modalW, modalY + S(8.0f), S(12), Theme.TextPrimary);

    // Message
    // Split message into two lines if needed
    DrawCentredText("Are you sure you want to load", faceSm, modalX, modalW, modalY + S(55), S(10), Theme.TextPrimary);
    DrawCentredText("default jogwheel settings?", faceSm, modalX, modalW, modalY + S(75), S(10), Theme.TextPrimary);

    // Cancel / OK Buttons
    Rectangle cancelBtn = {modalX + S(20), modalY + modalH - S(44), S(130), S(32)};
    bool cancelHover = CheckCollisionPointRec(Input_GetPointerPos(), cancelBtn) || r->State->ModalCursorPos == 0;
    DrawRectangleRec(cancelBtn, cancelHover ? Theme.BgPanel : Theme.BorderDefault);
    DrawRectangleLinesEx(cancelBtn, 1.5f, Theme.BorderDefault);
    DrawCentredText("CANCEL", faceSm, cancelBtn.x, cancelBtn.width, cancelBtn.y + S(10), S(11), Theme.TextPrimary);

    Rectangle okBtn = {modalX + modalW - S(150), modalY + modalH - S(44), S(130), S(32)};
    bool okHover = CheckCollisionPointRec(Input_GetPointerPos(), okBtn) || r->State->ModalCursorPos == 1;
    DrawRectangleRec(okBtn, okHover ? Theme.AccentRed : Fade(Theme.AccentRed, 0.3f));
    DrawRectangleLinesEx(okBtn, 1.5f, okHover ? Theme.AccentRed : Theme.BorderDefault);
    DrawCentredText("OK", faceSm, okBtn.x, okBtn.width, okBtn.y + S(10), S(11), Theme.TextPrimary);
    
    if (Input_CheckPress(cancelBtn) || (r->State->MidiRequestEnter && r->State->ModalCursorPos == 0)) {
        r->State->IsConfirmPopupOpen = false;
        r->State->MidiRequestEnter = false;
    } else if (Input_CheckPress(okBtn) || (r->State->MidiRequestEnter && r->State->ModalCursorPos == 1)) {
        r->State->IsConfirmPopupOpen = false;
        if (r->OnAction) r->OnAction(r->callbackCtx, r->State->ConfirmActionIdx);
        r->State->MidiRequestEnter = false;
    }
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
  r->State->IsConfirmPopupOpen = false;
  r->State->IsSliderModalOpen = false;
  r->State->IsSystemInfoOpen = false;
  r->OnClose = NULL;
  r->OnApply = NULL;
  r->callbackCtx = NULL;
}
