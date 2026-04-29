#include "ui/player/deckinfo.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "ui/components/assets_bundle.h"
#include "core/logic/quantize.h"

static Texture2D crownTex = {0};
static Texture2D starTex = {0};

static int DeckInfo_Update(Component *base) {
    DeckInfoPanel *d = (DeckInfoPanel *)base;
    
    float deckInfoW = SIDE_PANEL_W;
    float deckInfoH = (SCREEN_HEIGHT - TOP_BAR_H - FX_BAR_H - DECK_STR_H) / 2.0f;
    float y = TOP_BAR_H + (d->ID * deckInfoH);
    float margin = S(4.0f);

    // Layout Constants (Must match Draw function)
    float headerH = S(14.0f);
    float contentY = y + headerH + S(6);
    float statusH = S(22);
    float utilY = contentY + statusH + S(6);
    float utilW = (deckInfoW - margin * 2 - S(8)) / 3.0f;
    float utilH = S(14);
    float btnH = S(26);
    float btnY = y + deckInfoH - btnH - S(6); // More breathing room from bottom
    float btnW = (deckInfoW - margin * 2 - S(6)) / 2.0f;

    // 1. Eject
    float ejectX = deckInfoW - S(16);
    float ejectY = y + S(3.5f);
    Rectangle ejectRect = { ejectX - S(4), ejectY - S(2), S(20), S(12) };
    
    if (d->State->LoadedTrack != NULL && CheckCollisionPointRec(UIGetMousePosition(), ejectRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        DeckAudio_Unload(&d->Engine->Decks[d->ID]);
        if (d->State->LoadedTrack) {
            TrackState *t = d->State->LoadedTrack;
            d->State->LoadedTrack = NULL;
            if (t->BeatGrid != NULL) free(t->BeatGrid);
            free(t);
        }
        d->State->TrackTitle[0] = '\0';
        d->State->ArtistName[0] = '\0';
        d->State->ArtworkPath[0] = '\0';
        d->State->TrackLengthMs = 0;
        d->State->PositionMs = 0;
        d->State->Position = 0;
        d->State->CurrentBPM = 0;
        d->State->OriginalBPM = 0;
        strcpy(d->State->TrackKey, "");
    }

    // 2. Utility Buttons
    Rectangle msRect = { margin, utilY, utilW, utilH };
    if (CheckCollisionPointRec(UIGetMousePosition(), msRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) d->State->IsMaster = !d->State->IsMaster;

    Rectangle mtRect = { margin + utilW + S(4), utilY, utilW, utilH };
    if (CheckCollisionPointRec(UIGetMousePosition(), mtRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) d->State->MasterTempo = !d->State->MasterTempo;

    Rectangle viRect = { margin + (utilW + S(4)) * 2, utilY, utilW, utilH };
    if (CheckCollisionPointRec(UIGetMousePosition(), viRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) d->State->VinylModeEnabled = !d->State->VinylModeEnabled;

    // 3. Main Controls
    Rectangle cueRect = { margin, btnY, btnW, btnH };
    
    if (CheckCollisionPointRec(UIGetMousePosition(), cueRect)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (d->State->IsPlaying) {
                // While playing: Instant stop and jump back to cue
                DeckAudio_InstantStop(&d->Engine->Decks[d->ID]);
                DeckAudio_JumpToMs(&d->Engine->Decks[d->ID], d->State->MainCueMs);
                d->State->IsPlaying = false;
                d->State->PositionMs = d->State->MainCueMs;
            } else {
                // While paused: Set new cue point (quantized if enabled)
                if (d->State->QuantizeEnabled && d->State->LoadedTrack) {
                    d->State->MainCueMs = Quantize_GetNearestBeatMs(d->State->LoadedTrack, d->State->PositionMs);
                } else {
                    d->State->MainCueMs = d->State->PositionMs;
                }
                
                // Immediately jump to and sync the newly set cue point
                DeckAudio_JumpToMs(&d->Engine->Decks[d->ID], d->State->MainCueMs);
                d->State->PositionMs = d->State->MainCueMs;

                // Hold behavior: Start playing from cue point instantly
                DeckAudio_InstantPlay(&d->Engine->Decks[d->ID]);
                d->State->IsPlaying = true;
                d->State->IsCueHeld = true;
            }
        }
    }
    
    if (d->State->IsCueHeld && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        // When releasing a held CUE: Instant stop and return to cue point
        DeckAudio_InstantStop(&d->Engine->Decks[d->ID]);
        DeckAudio_JumpToMs(&d->Engine->Decks[d->ID], d->State->MainCueMs);
        d->State->IsPlaying = false;
        d->State->IsCueHeld = false;
        d->State->PositionMs = d->State->MainCueMs;
    }

    Rectangle playRect = { margin + btnW + S(6), btnY, btnW, btnH };
    if (CheckCollisionPointRec(UIGetMousePosition(), playRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (d->State->IsCueHeld) {
            // CUE + PLAY interlock: keep playing after CUE is released
            d->State->IsCueHeld = false;
            d->State->IsPlaying = true;
            // Physical motor is already on from CUE hold
        } else {
            bool targetPlaying = !d->State->IsPlaying;
            DeckAudio_SetPlaying(&d->Engine->Decks[d->ID], targetPlaying);
            d->State->IsPlaying = targetPlaying;
        }
    }

    return 0;
}

static void DeckInfo_Draw(Component *base) {
    DeckInfoPanel *d = (DeckInfoPanel *)base;
    
    if (crownTex.id == 0) {
        Image img = LoadImageFromMemory(".png", icon_crown, icon_crown_size);
        if (img.data == NULL) img = LoadImage("assets/icons/crown.png");
        if (img.data != NULL) {
            ImageResize(&img, (int)S(9), (int)S(9));
            crownTex = LoadTextureFromImage(img);
            UnloadImage(img);
            SetTextureFilter(crownTex, TEXTURE_FILTER_BILINEAR);
        }
    }
    if (starTex.id == 0) {
        Image img = LoadImageFromMemory(".png", icon_star, icon_star_size);
        if (img.data == NULL) img = LoadImage("assets/icons/star.png");
        if (img.data != NULL) {
            ImageResize(&img, (int)S(7), (int)S(7));
            starTex = LoadTextureFromImage(img);
            UnloadImage(img);
            SetTextureFilter(starTex, TEXTURE_FILTER_BILINEAR);
        }
    }
    
    float deckInfoW = SIDE_PANEL_W;
    float deckInfoH = (SCREEN_HEIGHT - TOP_BAR_H - FX_BAR_H - DECK_STR_H) / 2.0f;
    float y = TOP_BAR_H + (d->ID * deckInfoH);
    float margin = S(4.0f);

    DrawRectangle(0, y, deckInfoW, deckInfoH, ColorDark2);
    
    float headerH = S(14.0f);
    DrawRectangle(0, y, deckInfoW, headerH, ColorShadow);
    DrawRectangle(0, y + headerH - S(1.0f), deckInfoW, S(1.0f), d->ID == 0 ? ColorBlue : ColorOrange);

    Font faceXXS = UIFonts_GetFace(S(7));
    Font faceXS = UIFonts_GetFace(S(8.5f));
    Font faceSm = UIFonts_GetFace(S(10));
    Font faceIcon = UIFonts_GetIcon(S(11));

    char deckLabel[32];
    sprintf(deckLabel, "DECK %d", d->ID + 1);
    UIDrawText(deckLabel, faceXXS, margin, y + S(3.5f), S(7), ColorWhite);

    // Eject
    float ejectX = deckInfoW - S(16);
    float ejectY = y + S(3.5f);
    Rectangle ejectRect = { ejectX - S(4), ejectY - S(2), S(20), S(12) };
    if (d->State->LoadedTrack != NULL) {
        bool hoverEject = CheckCollisionPointRec(UIGetMousePosition(), ejectRect);
        UIDrawText("\uf052", faceIcon, ejectX, ejectY, S(10), hoverEject ? ColorRed : ColorShadow);
    }

    float contentY = y + headerH + S(6);
    float col1X = margin;
    float col2X = deckInfoW / 2.0f + S(2);
    
    // --- Row 1: Status Grid (Key & Bars) ---
    float statusH = S(22);
    DrawRectangle(margin, contentY, deckInfoW - margin * 2, statusH, (Color){0,0,0,60});
    
    // Column 1: KEY
    UIDrawText("KEY", faceXXS, col1X + S(6), contentY + S(4), S(7), ColorShadow);
    char keyStr[16] = "---";
    if (d->State->LoadedTrack) strcpy(keyStr, d->State->TrackKey);
    UIDrawText(keyStr, faceSm, col1X + S(6), contentY + S(11), S(10), ColorBlue);

    // Column 2: BAR
    UIDrawText("BAR", faceXXS, col2X + S(2), contentY + S(4), S(7), ColorShadow);
    char barsVal[32] = "01.1";
    if (d->State->LoadedTrack) {
        long long posMs = d->State->PositionMs;
        int beatIdx = -1;
        for (int i = 0; i < 1024 && d->State->LoadedTrack->BeatGrid[i].Time != 0xFFFFFFFF; i++) {
            if (d->State->LoadedTrack->BeatGrid[i].Time <= posMs) beatIdx = i;
            else break;
        }
        if (beatIdx >= 0) {
            int currentBeat = d->State->LoadedTrack->BeatGrid[beatIdx].BeatNumber;
            int currentBar = 1;
            for (int i = 0; i <= beatIdx; i++) if (d->State->LoadedTrack->BeatGrid[i].BeatNumber == 1) currentBar++;
            sprintf(barsVal, "%02d.%d", currentBar, currentBeat);
        }
    }
    UIDrawText(barsVal, faceSm, col2X + S(2), contentY + S(11), S(10), d->ID == 0 ? ColorOrange : ColorWhite);

    // --- Row 2: Utility Buttons (Master, MT, Vinyl) ---
    float utilY = contentY + statusH + S(6);
    float utilW = (deckInfoW - margin * 2 - S(8)) / 3.0f;
    float utilH = S(14);

    // Master
    Rectangle msRect = { margin, utilY, utilW, utilH };
    DrawRectangleRec(msRect, d->State->IsMaster ? Fade(ColorOrange, 0.3f) : ColorDark1);
    DrawRectangleLinesEx(msRect, S(1), d->State->IsMaster ? ColorOrange : ColorShadow);
    DrawTextureEx(crownTex, (Vector2){msRect.x + (utilW - S(9))/2.0f, msRect.y + S(2.5f)}, 0.0f, 1.0f, d->State->IsMaster ? ColorOrange : ColorShadow);

    // MT
    Rectangle mtRect = { margin + utilW + S(4), utilY, utilW, utilH };
    DrawRectangleRec(mtRect, d->State->MasterTempo ? Fade(ColorBlue, 0.3f) : ColorDark1);
    DrawRectangleLinesEx(mtRect, S(1), d->State->MasterTempo ? ColorBlue : ColorShadow);
    UIDrawText("MT", faceXXS, mtRect.x + (utilW - S(10))/2.0f, mtRect.y + S(4), S(7), d->State->MasterTempo ? ColorWhite : ColorShadow);

    // Vinyl
    Rectangle viRect = { margin + (utilW + S(4)) * 2, utilY, utilW, utilH };
    DrawRectangleRec(viRect, d->State->VinylModeEnabled ? Fade(ColorBlue, 0.3f) : ColorDark1);
    DrawRectangleLinesEx(viRect, S(1), d->State->VinylModeEnabled ? ColorBlue : ColorShadow);
    UIDrawText(d->State->VinylModeEnabled ? "VINYL" : "CDJ", faceXXS, viRect.x + (utilW - (d->State->VinylModeEnabled?S(20):S(14)))/2.0f, viRect.y + S(4), S(7), d->State->VinylModeEnabled ? ColorWhite : ColorShadow);

    // --- Row 3: Main Controls (Cue, Play) ---
    float btnH = S(26);
    float btnY = y + deckInfoH - btnH - S(6);
    float btnW = (deckInfoW - margin * 2 - S(6)) / 2.0f;

    // Cue
    Rectangle cueRect = { margin, btnY, btnW, btnH };
    bool hoverCue = CheckCollisionPointRec(UIGetMousePosition(), cueRect);
    bool isCueing = d->State->IsCueActive;
    DrawRectangleRec(cueRect, isCueing ? Fade(ColorCue, 0.4f) : ColorDark1);
    DrawRectangleLinesEx(cueRect, S(1), isCueing ? ColorCue : ColorShadow);
    if (hoverCue) DrawRectangleLinesEx(cueRect, S(1.5f), ColorWhite);
    UIDrawText("CUE", faceXS, cueRect.x + (btnW - S(18))/2.0f, cueRect.y + S(9), S(8.5f), isCueing ? ColorWhite : ColorShadow);

    // Play/Pause
    Rectangle playRect = { margin + btnW + S(6), btnY, btnW, btnH };
    bool hoverPlay = CheckCollisionPointRec(UIGetMousePosition(), playRect);
    bool isPlaying = d->State->IsPlaying;
    DrawRectangleRec(playRect, isPlaying ? Fade(ColorGreen, 0.4f) : ColorDark1);
    DrawRectangleLinesEx(playRect, S(1), isPlaying ? ColorGreen : ColorShadow);
    if (hoverPlay) DrawRectangleLinesEx(playRect, S(1.5f), ColorWhite);
    UIDrawText(isPlaying ? "\uf04c" : "\uf04b", faceIcon, playRect.x + (btnW - S(10))/2.0f, playRect.y + S(8), S(11), isPlaying ? ColorWhite : ColorShadow);
}

void DeckInfoPanel_Init(DeckInfoPanel *p, int id, DeckState *state, AudioEngine *engine) {
    p->base.Update = DeckInfo_Update;
    p->base.Draw = DeckInfo_Draw;
    p->ID = id;
    p->State = state;
    p->Engine = engine;
}
