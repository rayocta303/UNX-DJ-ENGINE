#include "ui/components/theme.h"
#include "input/input.h"
#include <math.h>

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
const Color ColorGreen  = {0x00, 0xFF, 0x00, 0xFF};
const Color ColorYellow = {0xFF, 0xFF, 0x00, 0xFF};
const Color ColorPink   = {0xFF, 0x00, 0xFF, 0xFF};

float UI_CurrScale = 1.0f;
float UI_OffsetX = 0.0f;
float UI_OffsetY = 0.0f;
bool UI_BoldEnabled = true;

AppTheme Theme;

void Theme_InitDefaultDark(void) {
    Theme.BgMain = (Color){20, 20, 25, 255};
    Theme.BgPanel = (Color){28, 28, 35, 255};
    Theme.BgPanelAlt = (Color){45, 45, 55, 255};
    Theme.BgModal = (Color){10, 10, 15, 230};
    Theme.BgOverlay = (Color){0, 0, 0, 180};
    
    Theme.TextPrimary = (Color){255, 255, 255, 255};
    Theme.TextSecondary = (Color){160, 160, 160, 255};
    Theme.TextMuted = (Color){85, 85, 85, 255};
    Theme.TextActive = (Color){0, 120, 255, 255};
    
    Theme.AccentBlue = (Color){0, 120, 255, 255};
    Theme.AccentOrange = (Color){255, 121, 0, 255};
    Theme.AccentRed = (Color){240, 50, 50, 255};
    Theme.AccentGreen = (Color){0, 200, 80, 255};
    Theme.AccentYellow = (Color){255, 200, 0, 255};
    
    Theme.HoverSubtle = (Color){255, 255, 255, 15};
    Theme.HoverActive = (Color){45, 45, 55, 255};
    Theme.SelectedItem = (Color){0, 120, 255, 255};
    Theme.BorderDefault = (Color){85, 85, 85, 255};
    Theme.BorderActive = (Color){255, 255, 255, 255};
    
    Theme.WaveformBg = (Color){20, 20, 22, 255};
    Theme.Playhead = (Color){255, 255, 255, 255};
    Theme.LoopRegion = (Color){255, 220, 0, 255};
    Theme.CueMarker = (Color){255, 150, 0, 255};
    Theme.DeckActiveBg = (Color){255, 121, 0, 45};
}

void Theme_InitDefaultLight(void) {
    Theme.BgMain = (Color){245, 245, 248, 255};
    Theme.BgPanel = (Color){230, 230, 235, 255};
    Theme.BgPanelAlt = (Color){210, 210, 215, 255};
    Theme.BgModal = (Color){220, 220, 225, 230};
    Theme.BgOverlay = (Color){255, 255, 255, 150};
    
    Theme.TextPrimary = (Color){30, 30, 30, 255};
    Theme.TextSecondary = (Color){80, 80, 80, 255};
    Theme.TextMuted = (Color){150, 150, 150, 255};
    Theme.TextActive = (Color){0, 100, 220, 255};
    
    Theme.AccentBlue = (Color){0, 110, 240, 255};
    Theme.AccentOrange = (Color){240, 110, 0, 255};
    Theme.AccentRed = (Color){220, 40, 40, 255};
    Theme.AccentGreen = (Color){0, 180, 70, 255};
    Theme.AccentYellow = (Color){230, 180, 0, 255};
    
    Theme.HoverSubtle = (Color){0, 0, 0, 15};
    Theme.HoverActive = (Color){210, 210, 215, 255};
    Theme.SelectedItem = (Color){0, 110, 240, 255};
    Theme.BorderDefault = (Color){180, 180, 180, 255};
    Theme.BorderActive = (Color){40, 40, 40, 255};
    
    Theme.WaveformBg = (Color){235, 235, 238, 255};
    Theme.Playhead = (Color){50, 50, 50, 255};
    Theme.LoopRegion = (Color){255, 200, 0, 200};
    Theme.CueMarker = (Color){255, 130, 0, 255};
    Theme.DeckActiveBg = (Color){240, 110, 0, 60};
}

void UI_UpdateScale(void) {
    // Primary scaling anchor is height to maintain consistent vertical layout density.
    // Horizontal layout becomes responsive (waveforms stretch/contract).
    UI_CurrScale = (float)GetScreenHeight() / REF_HEIGHT;
    
    // In responsive mode, we occupy the full window native area.
    UI_OffsetX = 0;
    UI_OffsetY = 0;
}

void UI_UpdateTouchState(void) {
    // Call new unified Android-grade touch state machine
    TouchInput_Update();
}

float S(float v) {
    return v * UI_CurrScale;
}

int Si(int v) {
    return (int)(v * UI_CurrScale);
}
