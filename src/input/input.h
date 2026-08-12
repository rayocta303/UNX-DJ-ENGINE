#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "input/mouse_utility.h"
#include "input/touch_utility.h"

#ifdef __cplusplus
extern "C" {
#endif

// Unified API for UI Components to completely avoid raw Raylib calls
// By default, we use Touch_CheckClick because our new Touch Engine flawlessly
// handles BOTH Mouse and Touch inputs.

static inline bool Input_CheckClick(Rectangle rect) {
  return Touch_CheckClick(rect, 0.0f);
}

static inline bool Input_CheckClickEx(Rectangle rect, float padding) {
  return Touch_CheckClick(rect, padding);
}

static inline bool Input_CheckPress(Rectangle rect) {
  return Touch_CheckPress(rect, 0.0f);
}

static inline Vector2 Input_GetPointerPos(void) {
  return TouchInput_GetPosition(); // Use touch engine cache to prevent (0,0) jump on release
}

// State Machine Abstractions (Replaces raw UI_IsPressed/Down/Released)
static inline bool Input_IsPressed(void) {
  return TouchInput_GetState() == TOUCH_STATE_PRESSED;
}

static inline bool Input_IsDown(void) {
  TouchState s = TouchInput_GetState();
  return s == TOUCH_STATE_PRESSED || s == TOUCH_STATE_DRAGGING;
}

static inline bool Input_IsReleased(void) {
  return TouchInput_GetState() == TOUCH_STATE_RELEASED;
}

static inline void Input_Consume(void) { Touch_ConsumeInput(); }

static inline float Input_GetDragDistance(void) {
  return Touch_GetDragDistance();
}

static inline Vector2 Input_GetStartPos(void) { return Touch_GetStartPos(); }

#ifdef __cplusplus
}
#endif

#endif // INPUT_MANAGER_H
