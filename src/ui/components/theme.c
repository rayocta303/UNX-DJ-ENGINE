#include "ui/components/theme.h"

const Color ColorPaper  = {0xE0, 0xE0, 0xDA, 0xFF};
const Color ColorShadow = {0xA0, 0xA0, 0x90, 0xFF};
const Color ColorDGreen = {0x00, 0xCC, 0x00, 0xFF};
const Color ColorDark1  = {0x33, 0x33, 0x33, 0xFF};
const Color ColorDark2  = {0x1A, 0x1A, 0x1A, 0xFF};
const Color ColorDark3  = {0x0D, 0x0D, 0x0D, 0xFF};
const Color ColorBGUtil = {0x22, 0x22, 0x22, 0xFF};
const Color ColorBlack  = {0x00, 0x00, 0x00, 0xFF};
const Color ColorWhite  = {0xFF, 0xFF, 0xFF, 0xFF};
const Color ColorRed    = {0xFF, 0x00, 0x00, 0xFF};
const Color ColorOrange = {0xFF, 0x80, 0x00, 0xFF};
const Color ColorBlue   = {0x00, 0x78, 0xFF, 0xFF};
const Color ColorCue    = {255, 165, 0, 255};
const Color ColorGray   = {0x55, 0x55, 0x55, 0xFF};


float UI_CurrScale = 1.0f;
bool UI_BoldEnabled = true;

void UI_UpdateScale(void) {
    float scaleX = (float)GetScreenWidth() / REF_WIDTH;
    float scaleY = (float)GetScreenHeight() / REF_HEIGHT;
    UI_CurrScale = (scaleX < scaleY) ? scaleX : scaleY; // Keep 16:10 fit
}

float S(float v) {
    return v * UI_CurrScale;
}

int Si(int v) {
    return (int)(v * UI_CurrScale);
}
