#ifndef DEBUG_IOS_H
#define DEBUG_IOS_H

#include "ui/components/component.h"

typedef struct {
    Component base;
    bool isPressed;
    int touchCount;
    Color bgColor;
} DebugIOSView;

void DebugIOS_Init(DebugIOSView *v);

#endif
