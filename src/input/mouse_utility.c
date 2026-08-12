#include "input/mouse_utility.h"
#include "ui/components/theme.h"

bool Mouse_CheckHover(Rectangle rect) {
    Vector2 m = Mouse_GetPos();
    return CheckCollisionPointRec(m, rect);
}

bool Mouse_CheckRightClick(Rectangle rect) {
    if (!IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) return false;
    return Mouse_CheckHover(rect);
}

bool Mouse_CheckMiddleClick(Rectangle rect) {
    if (!IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) return false;
    return Mouse_CheckHover(rect);
}

Vector2 Mouse_GetPos(void) {
    Vector2 m = GetMousePosition();
    return (Vector2){ m.x - UI_OffsetX, m.y - UI_OffsetY };
}

float Mouse_GetWheel(void) {
    return GetMouseWheelMove();
}

Vector2 Mouse_GetDelta(void) {
    return GetMouseDelta();
}
