#include "ui/player/deckinfo.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "ui/components/assets_bundle.h"

static Texture2D crownTex = {0};
static Texture2D starTex = {0};

static int DeckInfo_Update(Component *base) {
    DeckInfoPanel *d = (DeckInfoPanel *)base;
    
    float deckInfoW = SIDE_PANEL_W;
    float deckInfoH = (SCREEN_HEIGHT - TOP_BAR_H - FX_BAR_H - DECK_STR_H) / 2.0f;
    float y = TOP_BAR_H + (d->ID * deckInfoH);
    
    float ejectX = deckInfoW - S(16);
    float ejectY = y + S(1.5f);
    Rectangle ejectRect = { ejectX - S(4), ejectY - S(2), S(20), S(12) };
    
    if (d->State->LoadedTrack != NULL && CheckCollisionPointRec(UIGetMousePosition(), ejectRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Unload Engine
        DeckAudio_Unload(&d->Engine->Decks[d->ID]);
        
        // Unload UI State
        if (d->State->LoadedTrack) {
            TrackState *t = d->State->LoadedTrack;
            d->State->LoadedTrack = NULL; // Clear pointer FIRST
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
    float margin = 10.0f;

    DrawRectangle(0, y, deckInfoW, deckInfoH, ColorDark2);
    
    float headerH = 12.0f;
    DrawRectangle(0, y, deckInfoW, S(headerH), ColorShadow);
    DrawRectangle(0, y + S(headerH) - S(0.5f), deckInfoW, S(1.0f), ColorBlue);

    DrawLine(deckInfoW - 1, y, deckInfoW - 1, y + deckInfoH, ColorDark1);
    DrawLine(0, y + deckInfoH - 1, deckInfoW, y + deckInfoH - 1, ColorDark1);

    Font faceXXS = UIFonts_GetFace(S(7.5f));
    Font faceXS = UIFonts_GetFace(S(8.5f));
    Font faceSm = UIFonts_GetFace(S(10));
    Font iconBrand = UIFonts_GetIconBrand(S(10));

    char deckLabel[32];
    sprintf(deckLabel, "DECK %d", d->ID + 1);
    UIDrawText(deckLabel, faceXXS, S(margin), y + S(1.5f), S(7.5f), ColorWhite);

    // Eject Button (Upper right)
    Font faceIcon = UIFonts_GetIcon(S(10));
    float ejectX = deckInfoW - S(16);
    float ejectY = y + S(1.5f);
    Rectangle ejectRect = { ejectX - S(4), ejectY - S(2), S(20), S(12) };
    bool hoverEject = CheckCollisionPointRec(UIGetMousePosition(), ejectRect);
    
    if (d->State->LoadedTrack != NULL) {
        UIDrawText("\uf052", faceIcon, ejectX, ejectY, S(10), hoverEject ? ColorRed : ColorShadow);
    }

    float contentY = y + S(headerH);
    float contentH = deckInfoH - S(headerH);
    float rowH = contentH / 4.0f;

    for (int i = 1; i < 4; i++) {
        float dividerY = contentY + i * rowH;
        DrawLine(0, dividerY, deckInfoW, dividerY, (Color){0x22, 0x22, 0x22, 0xFF});
    }

    // 1. Source Row
    float sourceY = contentY + (rowH - S(10)) / 2.0f;
    UIDrawText("\uf287", iconBrand, S(margin), sourceY, S(10), ColorWhite); // uf287 usb
    UIDrawText(d->State->SourceName, faceXS, S(margin + 16), sourceY - S(1), S(8.5f), ColorWhite);

    // 2. Key Row
    float keyY = contentY + rowH + (rowH - S(10)) / 2.0f;
    DrawRectangleLines(S(margin), keyY, S(14), S(10), ColorShadow);
    UIDrawText("\uf7c2", faceXXS, S(margin + 2), keyY + S(1), S(7.5f), ColorShadow); // flat sharp
    
    const char* keyStr = "---";
    if (d->State->LoadedTrack != NULL) {
        keyStr = d->State->TrackKey;
    }
    UIDrawText(keyStr, faceXS, S(margin + 20), keyY, S(8.5f), ColorWhite);

    // Track Rating (Placeholder stars if loaded)
    if (d->State->LoadedTrack != NULL) {
        float starX = S(margin + 60);
        for (int i = 0; i < 5; i++) {
            DrawTextureEx(starTex, (Vector2){starX + i * S(8), keyY + S(1)}, 0.0f, 1.0f, (i < 3) ? ColorOrange : ColorShadow);
        }
    }

    // 3. Bars Row
    float barsY = contentY + rowH * 2.0f + (rowH - S(11)) / 2.0f;
    Color barsColor = ColorWhite;
    char barsVal[32] = "---.-";

    if (d->State->LoadedTrack != NULL) {
        long long posMs = d->State->PositionMs;
        int beatIdx = -1;

        for (int i = 0; i < 1024; i++) {
            long long bMs = (long long)d->State->LoadedTrack->BeatGrid[i].Time;
            if (bMs == 0xFFFFFFFF) break;
            if (bMs <= posMs) beatIdx = i;
            else break;
        }

        if (beatIdx >= 0) {
            int currentBeat = d->State->LoadedTrack->BeatGrid[beatIdx].BeatNumber;
            
            // To figure out currentBar efficiently: Count how many "Beat 1"s we've encountered
            int currentBar = 0;
            for (int i = 0; i <= beatIdx; i++) {
                if (d->State->LoadedTrack->BeatGrid[i].BeatNumber == 1) {
                    currentBar++;
                }
            }
            // If we somehow didn't hit a 1 yet, fallback to 1 
            if (currentBar == 0) currentBar = 1;
            
            sprintf(barsVal, "%02d.%d", currentBar, currentBeat);
        } else {
            // Negative position or before first beat
            if (posMs < 0) {
                // Determine BPM from first two beats to extrapolate lead-in bars
                double firstBeatMs = (double)d->State->LoadedTrack->BeatGrid[0].Time;
                double secondBeatMs = (double)d->State->LoadedTrack->BeatGrid[1].Time;
                double beatLen = secondBeatMs - firstBeatMs;
                if (beatLen <= 0) beatLen = 500.0; // 120 BPM fallback

                double msFromFirst = (double)posMs - firstBeatMs;
                int totalBeatsBefore = (int)floor(msFromFirst / beatLen); 
                // e.g. -500ms from start = -1 beat.
                // -1 beat relative to 1.1 -> 0.4? No, usually it counts as -01.1
                
                int absBeats = abs(totalBeatsBefore);
                int bars = (absBeats / 4) + 1;
                int beats = (absBeats % 4) + 1;
                sprintf(barsVal, "-%02d.%d", bars, beats);
            } else {
                sprintf(barsVal, "01.1");
            }
        }
    }

    if (d->ID == 0) barsColor = ColorOrange;

    UIDrawText(barsVal, faceSm, S(margin), barsY, S(10), barsColor);
    UIDrawText("Bars", faceXXS, S(margin + 32), barsY + S(2.5f), S(7.5f), barsColor);

    // 4. Buttons Row (MT, Vinyl, etc)
    float btnY = contentY + rowH * 3.0f + (rowH - S(10)) / 2.0f;
    float btnH = S(11);
    float btnS = S(3); // Spacing
    float x = S(margin);
    
    // MASTER Button (Crown Icon Only)
    float msW = S(20);
    Rectangle msRect = { x, btnY, msW, btnH };
    bool msClicked = CheckCollisionPointRec(UIGetMousePosition(), msRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (msClicked) d->State->IsMaster = !d->State->IsMaster;

    Color msColor = d->State->IsMaster ? ColorOrange : ColorShadow;
    DrawRectangleRounded(msRect, 0.2f, 4, msColor);
    
    Color iconColor = d->State->IsMaster ? ColorBlack : ColorWhite;
    Vector2 iconPos = {
        msRect.x + (msRect.width - crownTex.width) / 2.0f,
        msRect.y + (msRect.height - crownTex.height) / 2.0f
    };
    DrawTextureEx(crownTex, iconPos, 0.0f, 1.0f, iconColor);

    x += msW + btnS;

    // MT Button
    float mtW = S(18);
    Rectangle mtRect = { x, btnY, mtW, btnH };
    bool mtClicked = CheckCollisionPointRec(UIGetMousePosition(), mtRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (mtClicked) d->State->MasterTempo = !d->State->MasterTempo;
    
    Color mtColor = d->State->MasterTempo ? ColorBlue : ColorShadow;
    DrawRectangleRounded(mtRect, 0.2f, 4, mtColor);
    UIDrawText("MT", faceXXS, mtRect.x + (mtRect.width - S(10)) / 2.0f, mtRect.y + S(2), S(7.5f), ColorWhite);

    x += mtW + btnS;

    // Vinyl Button
    float viW = S(32);
    Rectangle viRect = { x, btnY, viW, btnH };
    bool viClicked = CheckCollisionPointRec(UIGetMousePosition(), viRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (viClicked) d->State->VinylModeEnabled = !d->State->VinylModeEnabled;

    Color viColor = d->State->VinylModeEnabled ? ColorBlue : ColorShadow;
    DrawRectangleRounded(viRect, 0.2f, 4, viColor);
    UIDrawText(d->State->VinylModeEnabled ? "VINYL" : "CDJ", faceXXS, viRect.x + (d->State->VinylModeEnabled ? S(4.5f) : S(7.5f)), viRect.y + S(2), S(7.5f), ColorWhite);
}

void DeckInfoPanel_Init(DeckInfoPanel *p, int id, DeckState *state, AudioEngine *engine) {
    p->base.Update = DeckInfo_Update;
    p->base.Draw = DeckInfo_Draw;
    p->ID = id;
    p->State = state;
    p->Engine = engine;
}
