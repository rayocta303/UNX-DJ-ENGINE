#ifndef TOUCH_UTILITY_H
#define TOUCH_UTILITY_H

#include "raylib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Touch States
typedef enum {
    TOUCH_STATE_IDLE = 0,
    TOUCH_STATE_PRESSED,
    TOUCH_STATE_DRAGGING,
    TOUCH_STATE_RELEASED
} TouchState;

// Smooth Kinetic Touch Scroll State
typedef struct {
    float Scroll;            // Target visual scroll offset
    float VisualScroll;      // Smoothly interpolated scroll offset (for rendering)
    float Velocity;          // Kinetic inertia velocity
    float DragStartPos;      // Initial Y position on touch down
    float DragStartScroll;   // Initial Scroll on touch down
    float DragDistance;      // Accumulated drag distance
    bool  IsDragging;        // Active touch drag flag
    double LastTouchTime;    // Timestamp of last touch update
    float Friction;          // Inertia decay rate (0.88f - 0.95f)
    float BounceEffect;      // Elastic rubber-band displacement
} TouchScroll;

// Per-instance touch debounce state
typedef struct {
    double LastClickTime;
} TouchDebounce;

// Core Input System
void TouchInput_Update(void);
TouchState TouchInput_GetState(void);
Vector2 TouchInput_GetPosition(void);
Vector2 TouchInput_GetStartPos(void);
float TouchInput_GetDragDistance(void);

// Initialize touch scroll state
void TouchScroll_Init(TouchScroll *ts);

// Update scroll physics with kinetic momentum and overscroll bounds dampening
void TouchScroll_Update(TouchScroll *ts, float maxScroll, float dt);

// Reset touch scroll position and velocity
void TouchScroll_Reset(TouchScroll *ts);

// Expanded Hitbox Click Checker (with configurable touch padding using global debounce)
bool Touch_CheckClick(Rectangle rect, float padding);

// Expanded Hitbox Click Checker with custom per-instance debounce state
bool Touch_CheckClickEx(Rectangle rect, float padding, TouchDebounce *td, float debounceTimeSec);

// Click checker without time debounce, only enforcing drag-distance threshold and release inside bounds
bool Touch_CheckClickInArea(Rectangle rect, float padding);

// Check if touch inside padded hitbox on press (instant down response, with drag distance guard)
bool Touch_CheckPress(Rectangle rect, float padding);

// Reset global touch debounce timer
void Touch_ResetGlobalDebounce(void);
float Touch_GetDragDistance(void);
Vector2 Touch_GetStartPos(void);

// Reset instance touch debounce timer
void Touch_ResetDebounce(TouchDebounce *td);

// Calculate swipe velocity delta between drag updates
float Touch_GetSwipeVelocity(Vector2 startPos, Vector2 currentPos, float deltaTime);

#ifdef __cplusplus
}
#endif

#endif // TOUCH_UTILITY_H
