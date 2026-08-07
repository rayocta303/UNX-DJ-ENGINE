#ifndef TOUCH_UTILITY_H
#define TOUCH_UTILITY_H

#include "raylib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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

// Initialize touch scroll state
void TouchScroll_Init(TouchScroll *ts);

// Update scroll physics with kinetic momentum and overscroll bounds dampening
void TouchScroll_Update(TouchScroll *ts, float maxScroll, float dt);

// Reset touch scroll position and velocity
void TouchScroll_Reset(TouchScroll *ts);

// Expanded Hitbox Click Checker (with configurable touch padding)
bool Touch_CheckClick(Rectangle rect, float padding);

// Check if touch inside padded hitbox on press (instant down response)
bool Touch_CheckPress(Rectangle rect, float padding);

// Calculate swipe velocity delta between drag updates
float Touch_GetSwipeVelocity(Vector2 startPos, Vector2 currentPos, float deltaTime);

#ifdef __cplusplus
}
#endif

#endif // TOUCH_UTILITY_H
