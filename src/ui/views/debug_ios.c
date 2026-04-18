#include "debug_ios.h"
#include "ui/components/fonts.h"
#include "ui/components/theme.h"
#include <stdio.h>

static int DebugIOS_Update(Component *base) {
    DebugIOSView *v = (DebugIOSView *)base;
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || GetTouchPointCount() > 0) {
        v->isPressed = true;
        v->touchCount++;
        // Cycle colors
        if (v->touchCount % 3 == 0) v->bgColor = DARKGRAY;
        else if (v->touchCount % 3 == 1) v->bgColor = MAROON;
        else v->bgColor = DARKBLUE;
    } else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        v->isPressed = false;
    }
    
    return 0;
}

static void DebugIOS_Draw(Component *base) {
    DebugIOSView *v = (DebugIOSView *)base;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    
    ClearBackground(v->bgColor);
    
    // 1. Draw Screen Edge Lines (Border)
    DrawRectangleLines(0, 0, sw, sh, RED);
    DrawRectangleLines(2, 2, sw - 4, sh - 4, YELLOW); // Secondary safe line
    
    // 2. Draw Corner Color Boxes (50x50)
    DrawRectangle(0, 0, 50, 50, RED);           // Top-Left
    DrawRectangle(sw - 50, 0, 50, 50, GREEN);   // Top-Right
    DrawRectangle(0, sh - 50, 50, 50, BLUE);    // Bottom-Left
    DrawRectangle(sw - 50, sh - 50, 50, 50, YELLOW); // Bottom-Right
    
    // Header
    DrawText("XDJ-UNX iOS NATIVE TEST", 70, 40, 30, RAYWHITE);
    DrawLine(70, 80, sw - 70, 80, GRAY);
    
    // System Info
    char info[256];
    sprintf(info, "Resolution: %d x %d", sw, sh);
    DrawText(info, 70, 100, 20, LIGHTGRAY);
    
    sprintf(info, "FPS: %d", GetFPS());
    DrawText(info, 70, 130, 20, GREEN);
    
    sprintf(info, "Touch Count: %d", v->touchCount);
    DrawText(info, 70, 160, 20, ORANGE);
    
    // Interactive Test Area
    Rectangle btn = { (float)sw/2 - 150, (float)sh/2 - 50, 300, 100 };
    DrawRectangleRec(btn, v->isPressed ? SKYBLUE : DARKGRAY);
    DrawRectangleLinesEx(btn, 2, WHITE);
    
    const char* btnTxt = v->isPressed ? "TOUCHED!" : "TAP CENTER";
    int txtW = MeasureText(btnTxt, 25);
    DrawText(btnTxt, btn.x + (btn.width - txtW)/2, btn.y + 35, 25, WHITE);
    
    DrawText("Pemicu: Sentuh area mana pun untuk ganti warna background", 70, sh - 40, 15, GRAY);
}

void DebugIOS_Init(DebugIOSView *v) {
    v->base.Update = DebugIOS_Update;
    v->base.Draw = DebugIOS_Draw;
    v->isPressed = false;
    v->touchCount = 0;
    v->bgColor = BLACK;
}
