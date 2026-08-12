#ifndef MOUSE_UTILITY_H
#define MOUSE_UTILITY_H

#include "raylib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Check if mouse is hovering over a rectangle
bool Mouse_CheckHover(Rectangle rect);

// Check if mouse is right-clicked inside a rectangle
bool Mouse_CheckRightClick(Rectangle rect);

// Check if mouse is middle-clicked inside a rectangle
bool Mouse_CheckMiddleClick(Rectangle rect);

// Get exact mouse position
Vector2 Mouse_GetPos(void);

// Get mouse wheel delta (Y axis)
float Mouse_GetWheel(void);

// Get mouse drag delta since last frame
Vector2 Mouse_GetDelta(void);

#ifdef __cplusplus
}
#endif

#endif // MOUSE_UTILITY_H
