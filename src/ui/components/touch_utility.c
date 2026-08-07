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

bool Touch_CheckClick(Rectangle rect, float padding) {
    if (padding <= 0.0f) padding = S(6.0f);
    Rectangle paddedRect = {
        rect.x - padding,
        rect.y - padding,
        rect.width + padding * 2.0f,
        rect.height + padding * 2.0f
    };
    return UICheckClick(paddedRect);
}

bool Touch_CheckPress(Rectangle rect, float padding) {
    if (!UI_IsPressed()) return false;
    if (padding <= 0.0f) padding = S(6.0f);
    Rectangle paddedRect = {
        rect.x - padding,
        rect.y - padding,
        rect.width + padding * 2.0f,
        rect.height + padding * 2.0f
    };
    return CheckCollisionPointRec(UIGetMousePosition(), paddedRect);
}

float Touch_GetSwipeVelocity(Vector2 startPos, Vector2 currentPos, float deltaTime) {
    if (deltaTime <= 0.001f) return 0.0f;
    float dy = currentPos.y - startPos.y;
    return dy / deltaTime;
}
