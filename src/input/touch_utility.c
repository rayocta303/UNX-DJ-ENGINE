#include "input/touch_utility.h"
#include "ui/components/theme.h"
#include <stddef.h>
#include <math.h>

static TouchState g_touchState = TOUCH_STATE_IDLE;
static Vector2 g_touchPos = {0};
static Vector2 g_touchStart = {0};
static float g_dragDist = 0.0f;
static double g_lastTouchClickTime = -1.0;
static float g_dragThreshold = 10.0f; // S() scaling applied during update

#if defined(PLATFORM_DRM) || (defined(__linux__) && !defined(__ANDROID__))
extern bool Evdev_IsTouchDown(void);
extern bool Evdev_IsTouchPressed(void);
extern bool Evdev_IsTouchReleased(void);
#endif

void TouchInput_Update(void) {
    g_dragThreshold = S(10.0f); // Responsive scaled threshold

    // Sync raw positions (adjusted for UI offset)
    Vector2 m = GetMousePosition();
    g_touchPos = (Vector2){ m.x - UI_OffsetX, m.y - UI_OffsetY };

    // Raw input polling
    bool pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsGestureDetected(GESTURE_TAP);
    bool down = IsMouseButtonDown(MOUSE_LEFT_BUTTON) || GetTouchPointCount() > 0;
    bool released = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

#if defined(PLATFORM_DRM) || (defined(__linux__) && !defined(__ANDROID__))
    if (Evdev_IsTouchPressed()) pressed = true;
    if (Evdev_IsTouchDown()) down = true;
    if (Evdev_IsTouchReleased()) released = true;
#endif

    if (pressed) {
        g_touchState = TOUCH_STATE_PRESSED;
        g_touchStart = g_touchPos;
        g_dragDist = 0.0f;
    } else if (down) {
        if (g_touchState == TOUCH_STATE_PRESSED || g_touchState == TOUCH_STATE_DRAGGING) {
            float dx = g_touchPos.x - g_touchStart.x;
            float dy = g_touchPos.y - g_touchStart.y;
            g_dragDist = sqrtf(dx * dx + dy * dy);
            
            if (g_dragDist > g_dragThreshold) {
                g_touchState = TOUCH_STATE_DRAGGING;
            }
        }
    } else if (released) {
        if (g_touchState == TOUCH_STATE_PRESSED || g_touchState == TOUCH_STATE_DRAGGING) {
            g_touchState = TOUCH_STATE_RELEASED;
        } else {
            g_touchState = TOUCH_STATE_IDLE;
        }
    } else {
        g_touchState = TOUCH_STATE_IDLE;
    }
}

TouchState TouchInput_GetState(void) {
    return g_touchState;
}

void Touch_ConsumeInput(void) {
    g_touchState = TOUCH_STATE_IDLE;
    Touch_ResetGlobalDebounce();
}

float Touch_GetDragDistance(void) {
    return g_dragDist;
}

Vector2 Touch_GetStartPos(void) {
    return g_touchStart;
}

Vector2 TouchInput_GetPosition(void) {
    return g_touchPos;
}

Vector2 TouchInput_GetStartPos(void) {
    return g_touchStart;
}

float TouchInput_GetDragDistance(void) {
    return g_dragDist;
}

void TouchScroll_Init(TouchScroll *ts) {
    if (!ts) return;
    ts->Scroll = 0.0f;
    ts->VisualScroll = 0.0f;
    ts->Velocity = 0.0f;
    ts->DragStartPos = 0.0f;
    ts->DragStartScroll = 0.0f;
    ts->DragDistance = 0.0f;
    ts->IsDragging = false;
    ts->LastTouchTime = 0.0;
    ts->Friction = 0.92f;
    ts->BounceEffect = 0.0f;
}

void TouchScroll_Reset(TouchScroll *ts) {
    if (!ts) return;
    ts->Scroll = 0.0f;
    ts->VisualScroll = 0.0f;
    ts->Velocity = 0.0f;
    ts->IsDragging = false;
    ts->BounceEffect = 0.0f;
}

void TouchScroll_Update(TouchScroll *ts, float maxScroll, float dt) {
    if (!ts) return;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;

    // Handle Mouse Wheel Scroll
    float wheelMove = GetMouseWheelMove();
    if (wheelMove != 0) {
        ts->Scroll -= wheelMove * 50.0f; // S() scaling can be applied by caller or here
        ts->Velocity = 0.0f; // Cancel inertia on mouse wheel
    }

    if (maxScroll < 0.0f) maxScroll = 0.0f;

    // Apply kinetic inertia velocity decay when released
    if (!ts->IsDragging) {
        if (fabsf(ts->Velocity) > 0.5f) {
            ts->Scroll -= ts->Velocity * dt; // Note: Velocity is negative for scrolling down
            ts->Velocity *= powf(ts->Friction, dt * 60.0f);
        } else {
            ts->Velocity = 0.0f;
        }

        // Strict Boundary Clamping (No overscroll bounce / sembul / pantul)
        if (ts->Scroll < 0.0f) {
            ts->Scroll = 0.0f;
            ts->Velocity = 0.0f;
        } else if (ts->Scroll > maxScroll) {
            ts->Scroll = maxScroll;
            ts->Velocity = 0.0f;
        }
    }

    // High-fps smooth visual scroll interpolation (Exponential lerp)
    float lerpSpeed = 22.0f * dt;
    if (lerpSpeed > 1.0f) lerpSpeed = 1.0f;
    ts->VisualScroll += (ts->Scroll - ts->VisualScroll) * lerpSpeed;
}

void Touch_ResetGlobalDebounce(void) {
    g_lastTouchClickTime = -1.0;
}

void Touch_ResetDebounce(TouchDebounce *td) {
    if (td) td->LastClickTime = -1.0;
}

bool Touch_CheckClickEx(Rectangle rect, float padding, TouchDebounce *td, float debounceTimeSec) {
    if (padding < 0.0f) padding = 0.0f;
    Rectangle paddedRect = {
        rect.x - padding,
        rect.y - padding,
        rect.width + padding * 2.0f,
        rect.height + padding * 2.0f
    };

    double now = GetTime();
    float dtThreshold = (debounceTimeSec > 0.0f) ? debounceTimeSec : 0.18f;

    if (td) {
        if (td->LastClickTime > 0.0 && (now - td->LastClickTime) < dtThreshold) {
            return false;
        }
    } else {
        if (g_lastTouchClickTime > 0.0 && (now - g_lastTouchClickTime) < dtThreshold) {
            return false;
        }
    }

    // Use our new strict state machine instead of raw UI_IsReleased
    if (g_touchState == TOUCH_STATE_RELEASED) {
        bool isHitStart = CheckCollisionPointRec(g_touchStart, paddedRect);
        bool isHitCurr = CheckCollisionPointRec(g_touchPos, paddedRect);

        // Tap condition: Started in rect, released in rect, and drag distance is small (not a swipe)
        if (isHitStart && isHitCurr && g_dragDist < g_dragThreshold) {
            if (td) td->LastClickTime = now;
            g_lastTouchClickTime = now;
            return true;
        }
    }

    return false;
}

bool Touch_CheckClick(Rectangle rect, float padding) {
    return Touch_CheckClickEx(rect, padding, NULL, 0.18f);
}

bool Touch_CheckClickInArea(Rectangle rect, float padding) {
    if (padding < 0.0f) padding = 0.0f;
    Rectangle paddedRect = {
        rect.x - padding,
        rect.y - padding,
        rect.width + padding * 2.0f,
        rect.height + padding * 2.0f
    };

    if (g_touchState == TOUCH_STATE_RELEASED) {
        bool isHitStart = CheckCollisionPointRec(g_touchStart, paddedRect);
        bool isHitCurr = CheckCollisionPointRec(g_touchPos, paddedRect);

        if ((isHitStart || isHitCurr) && g_dragDist < g_dragThreshold) {
            return true;
        }
    }

    return false;
}

bool Touch_CheckPress(Rectangle rect, float padding) {
    if (g_touchState != TOUCH_STATE_PRESSED && g_touchState != TOUCH_STATE_DRAGGING) return false;
    if (g_dragDist >= g_dragThreshold) return false;
    
    if (padding < 0.0f) padding = 0.0f;
    Rectangle paddedRect = {
        rect.x - padding,
        rect.y - padding,
        rect.width + padding * 2.0f,
        rect.height + padding * 2.0f
    };

    return CheckCollisionPointRec(g_touchPos, paddedRect);
}

float Touch_GetSwipeVelocity(Vector2 startPos, Vector2 currentPos, float deltaTime) {
    if (deltaTime <= 0.001f) return 0.0f;
    float dy = currentPos.y - startPos.y;
    return dy / deltaTime;
}
