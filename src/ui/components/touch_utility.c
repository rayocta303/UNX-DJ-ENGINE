#include "ui/components/touch_utility.h"
#include "ui/components/theme.h"
#include <math.h>

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

    if (maxScroll < 0.0f) maxScroll = 0.0f;

    // Apply kinetic inertia velocity decay when released
    if (!ts->IsDragging) {
        if (fabsf(ts->Velocity) > 0.5f) {
            ts->Scroll += ts->Velocity * dt;
            ts->Velocity *= powf(ts->Friction, dt * 60.0f);
        } else {
            ts->Velocity = 0.0f;
        }

        // Overscroll Rubber-banding / Elastic Springback
        if (ts->Scroll < 0.0f) {
            ts->Scroll = ts->Scroll * (1.0f - 12.0f * dt);
            ts->Velocity *= 0.5f;
            if (fabsf(ts->Scroll) < 0.5f) ts->Scroll = 0.0f;
        } else if (ts->Scroll > maxScroll) {
            float diff = ts->Scroll - maxScroll;
            ts->Scroll = maxScroll + diff * (1.0f - 12.0f * dt);
            ts->Velocity *= 0.5f;
            if (fabsf(ts->Scroll - maxScroll) < 0.5f) ts->Scroll = maxScroll;
        }
    }

    // High-fps smooth visual scroll interpolation (Exponential lerp)
    float lerpSpeed = 22.0f * dt;
    if (lerpSpeed > 1.0f) lerpSpeed = 1.0f;
    ts->VisualScroll += (ts->Scroll - ts->VisualScroll) * lerpSpeed;
}

static double g_lastTouchClickTime = -1.0;

bool Touch_CheckClick(Rectangle rect, float padding) {
    if (padding <= 0.0f) padding = S(8.0f);
    Rectangle paddedRect = {
        rect.x - padding,
        rect.y - padding,
        rect.width + padding * 2.0f,
        rect.height + padding * 2.0f
    };

    double now = GetTime();
    if (g_lastTouchClickTime > 0.0 && (now - g_lastTouchClickTime) < 0.06) {
        return false;
    }

    Vector2 touchStart = UIGetTouchStartPos();
    Vector2 currPos = UIGetMousePosition();
    float dragDist = UIGetTouchDragDistance();

    bool isHitStart = CheckCollisionPointRec(touchStart, paddedRect);
    bool isHitCurr = CheckCollisionPointRec(currPos, paddedRect);

    // 1. Instant Press Trigger (Fast finger tap down)
    if (UI_IsPressed() && isHitCurr) {
        g_lastTouchClickTime = now;
        return true;
    }

    // 2. Tap Release Trigger (Finger release with relaxed drag threshold)
    if (UI_IsReleased() && (isHitStart || isHitCurr) && dragDist < S(32.0f)) {
        g_lastTouchClickTime = now;
        return true;
    }

    return false;
}

bool Touch_CheckPress(Rectangle rect, float padding) {
    if (!UI_IsPressed() && !UI_IsDown()) return false;
    if (padding <= 0.0f) padding = S(8.0f);
    Rectangle paddedRect = {
        rect.x - padding,
        rect.y - padding,
        rect.width + padding * 2.0f,
        rect.height + padding * 2.0f
    };

    Vector2 currPos = UIGetMousePosition();
    return CheckCollisionPointRec(currPos, paddedRect);
}

float Touch_GetSwipeVelocity(Vector2 startPos, Vector2 currentPos, float deltaTime) {
    if (deltaTime <= 0.001f) return 0.0f;
    float dy = currentPos.y - startPos.y;
    return dy / deltaTime;
}
