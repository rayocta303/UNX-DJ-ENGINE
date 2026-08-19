#include "ui/player/deckinfo.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "input/input.h"
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

    // 1. Eject Button (Touch Utility)
    float ejectW = S(12);
    float ejectH = S(12);
    float ejectX = deckInfoW - ejectW - S(3);
    float ejectY = y + (headerH - ejectH) / 2.0f;
    Rectangle ejectRect = { ejectX, ejectY, ejectW, ejectH };
    
    bool isEjectLocked = d->State->Waveform.LoadLock && d->State->IsPlaying;

    if (!isEjectLocked && d->State->LoadedTrack != NULL && Touch_CheckClick(ejectRect, S(8.0f))) {
        double now = GetTime();
        if (now - d->lastEjectTapTime < 2.0) { // 2 seconds window
            DeckAudio_Unload(&d->Engine->Decks[d->ID]);
            d->State->IsPlaying = false;
            d->State->IsCueActive = false;
            d->State->IsCueHeld = false;
            d->State->IsTouching = false;
            d->State->IsLooping = false;
            d->State->LoopAdjustIn = false;
            d->State->LoopAdjustOut = false;
            d->State->JogRate = 0.0f;
            d->State->JogDelta = 0.0;
            d->State->Position = 0;
            d->State->PositionMs = 0;
            d->State->MainCueMs = 0;
            d->State->TrackLengthMs = 0;
            d->State->CurrentBPM = 0.0f;
            d->State->OriginalBPM = 0.0f;
            d->State->IsPhaseDrifted = false;
            d->State->HasSeekRequest = false;
            d->State->IsMaster = false;
            if (d->State->LoadedTrack) {
                TrackState *t = d->State->LoadedTrack;
                d->State->LoadedTrack = NULL;
                if (t->Analysis.BeatGrid != NULL) free(t->Analysis.BeatGrid);
                if (t->Analysis.Phrases != NULL) free(t->Analysis.Phrases);
                if (t->Analysis.Cues != NULL) free(t->Analysis.Cues);
                if (t->Analysis.DynamicWaveform != NULL) free(t->Analysis.DynamicWaveform);
                free(t);
            }
            d->State->TrackTitle[0] = '\0';
            d->State->ArtistName[0] = '\0';
            d->State->ArtworkPath[0] = '\0';
            strcpy(d->State->TrackKey, "");
            d->lastEjectTapTime = 0.0;
        } else {
            d->lastEjectTapTime = now;
        }
    }

    // 2. Utility Buttons (2 Rows x 2 Columns Grid) - COMMENTED OUT
    /*
    float utilY = contentY + statusH + S(5);
    float utilGap = S(4.0f);
    float utilW = (deckInfoW - margin * 2 - utilGap) / 2.0f;
    float utilH = S(24); // Enlarged from S(14) to S(24)
    float utilY2 = utilY + utilH + S(6.0f); // Increased gap to match

    // Row 1: Master (Top-Left), Sync (Top-Right)
    Rectangle msRect = { margin, utilY, utilW, utilH };
    if (Touch_CheckClick(msRect, S(2.0f))) {
        d->State->IsMaster = true; // Exclusivity handled in main.c loop
    }

    Rectangle syRect = { margin + utilW + utilGap, utilY, utilW, utilH };
    if (Touch_CheckClick(syRect, S(2.0f))) {
        d->State->SyncMode = (d->State->SyncMode + 1) % 3;
    }

    // Row 2: MT (Bottom-Left), Vinyl / CDJ (Bottom-Right)
    Rectangle mtRect = { margin, utilY2, utilW, utilH };
    if (Touch_CheckClick(mtRect, S(2.0f))) {
        d->State->MasterTempo = !d->State->MasterTempo;
    }

    Rectangle viRect = { margin + utilW + utilGap, utilY2, utilW, utilH };
    if (Touch_CheckClick(viRect, S(2.0f))) {
        d->State->VinylModeEnabled = !d->State->VinylModeEnabled;
    }
    */

    // --- 3. Main Controls (CUE / PLAY) COMMENTED OUT ---
    /*
    Rectangle cueRect = { margin, btnY, btnW, btnH };
    
    if (CheckCollisionPointRec(Input_GetPointerPos(), cueRect)) {
        if (Input_IsPressed()) {
            if (d->State->IsPlaying) {
                // While playing: Instant stop and jump back to cue
                DeckAudio_InstantStop(&d->Engine->Decks[d->ID]);
                DeckAudio_ExitLoop(&d->Engine->Decks[d->ID]);
                DeckAudio_JumpToMs(&d->Engine->Decks[d->ID], d->State->MainCueMs);
                d->State->IsPlaying = false;
                d->State->PositionMs = d->State->MainCueMs;
            } else {
                // While paused: Set new cue point (quantized if enabled)
                if (d->State->QuantizeEnabled && d->State->LoadedTrack) {
                    d->State->MainCueMs = Quantize_GetNearestBeatMs(d->State->LoadedTrack, d->State->PositionMs, Quantize_GetDivisor(d->State->Waveform.QuantizeResolution));
                } else {
                    d->State->MainCueMs = d->State->PositionMs;
                }
                
                // Immediately jump to and sync the newly set cue point
                DeckAudio_ExitLoop(&d->Engine->Decks[d->ID]);
                DeckAudio_JumpToMs(&d->Engine->Decks[d->ID], d->State->MainCueMs);
                d->State->PositionMs = d->State->MainCueMs;

                // Hold behavior: Start playing from cue point instantly
                DeckAudio_InstantPlay(&d->Engine->Decks[d->ID]);
                d->State->IsPlaying = true;
                d->State->IsCueHeld = true;
            }
        }
    }
    
    if (d->State->IsCueHeld && Input_IsReleased()) {
        // When releasing a held CUE: Instant stop and return to cue point
        DeckAudio_InstantStop(&d->Engine->Decks[d->ID]);
        DeckAudio_ExitLoop(&d->Engine->Decks[d->ID]);
        DeckAudio_JumpToMs(&d->Engine->Decks[d->ID], d->State->MainCueMs);
        d->State->IsPlaying = false;
        d->State->IsCueHeld = false;
        d->State->PositionMs = d->State->MainCueMs;
    }

    Rectangle playRect = { margin + btnW + S(6), btnY, btnW, btnH };
    if (Input_CheckClick(playRect)) {
        if (d->State->IsCueHeld) {
            // CUE + PLAY interlock: keep playing after CUE is released
            d->State->IsCueHeld = false;
            d->State->IsPlaying = true;
            // Physical motor is already on from CUE hold
        } else {
            bool targetPlaying = !d->State->IsPlaying;
            if (targetPlaying) {
                DeckAudio_SetPlaying(&d->Engine->Decks[d->ID], true);
            } else {
                DeckAudio_InstantStop(&d->Engine->Decks[d->ID]);
            }
            d->State->IsPlaying = targetPlaying;
        }
    }
    */

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
    Font faceSm = UIFonts_GetFace(S(10));
    Font faceIcon = UIFonts_GetIcon(S(11));

    char deckLabel[32];
    sprintf(deckLabel, "DECK %d", d->ID + 1);
    UIDrawText(deckLabel, faceSm, margin, y + S(2.5f), S(10), ColorWhite);

    // Eject Button
    float ejectW = S(12);
    float ejectH = S(12);
    float ejectX = deckInfoW - ejectW - S(3);
    float ejectY = y + (headerH - ejectH) / 2.0f;
    Rectangle ejectRect = { ejectX, ejectY, ejectW, ejectH };

    // Visual Jog Platter Spinner & Load Animation Widget
    float jogCX = deckInfoW / 2.0f;
    float jogCY = y + headerH / 2.0f;
    float jogR = S(5.5f);

    // Platter background & outer ring
    Color jogRingColor = d->State->IsTouching ? ColorRed : (d->ID == 0 ? ColorBlue : ColorOrange);
    DrawCircle((int)jogCX, (int)jogCY, jogR, Theme.BgMain);
    DrawCircleLines((int)jogCX, (int)jogCY, jogR, Fade(jogRingColor, 0.6f));

    // Load Track Chaser Glow
    if (d->State->LoadAnimTimer > 0.0f) {
        float chaserAngle = d->State->JogPointerAngle;
        DrawCircleSectorLines((Vector2){jogCX, jogCY}, jogR + S(1.5f), chaserAngle - 60.0f, chaserAngle, 8, ColorBlue);
        DrawCircleSectorLines((Vector2){jogCX, jogCY}, jogR + S(1.0f), chaserAngle - 30.0f, chaserAngle, 8, ColorWhite);
    }

    // Rotating Pointer Needle
    float pRad = (d->State->JogPointerAngle - 90.0f) * (3.14159265f / 180.0f);
    Vector2 pStart = { jogCX + cosf(pRad) * S(1.5f), jogCY + sinf(pRad) * S(1.5f) };
    Vector2 pEnd = { jogCX + cosf(pRad) * (jogR - S(0.5f)), jogCY + sinf(pRad) * (jogR - S(0.5f)) };
    Color ptrColor = d->State->IsTouching ? ColorRed : (d->State->LoadAnimTimer > 0.0f ? ColorWhite : (d->State->IsPlaying ? ColorGreen : ColorWhite));
    DrawLineEx(pStart, pEnd, S(1.2f), ptrColor);

    bool isEjectLocked = d->State->Waveform.LoadLock && d->State->IsPlaying;

    if (!isEjectLocked && d->State->LoadedTrack != NULL) {
        bool hoverEject = CheckCollisionPointRec(Input_GetPointerPos(), ejectRect);
        bool isConfirming = (GetTime() - d->lastEjectTapTime < 2.0);
        
        // Button Background
        DrawRectangleRounded(ejectRect, 0.4f, 6, isConfirming ? ColorRed : (hoverEject ? ColorRed : Fade(ColorBlack, 0.4f)));
        DrawRectangleLinesEx(ejectRect, S(0.6f), isConfirming ? ColorWhite : (hoverEject ? ColorWhite : ColorShadow));
        
        // Icon (Eject)
        if (isConfirming) {
            UIDrawText("SURE?", faceXXS, ejectRect.x + S(1), ejectRect.y + S(2.5f), S(7), ColorWhite);
        } else {
            UIDrawText("\uf052", faceIcon, ejectRect.x + (ejectW - S(7))/2.0f, ejectRect.y + S(2.5f), S(7), ColorWhite);
        }
    }

    float contentY = y + headerH + S(6);
    float col1X = margin;
    float col2X = deckInfoW / 2.0f + S(2);
    
    // --- Row 1: Status Grid (Key & Bars) ---
    float statusH = S(22);
    DrawRectangle(margin, contentY, deckInfoW - margin * 2, statusH, Theme.BgOverlay);
    
    // Column 1: KEY
    UIDrawText("KEY", faceXXS, col1X + S(6), contentY + S(4), S(7), ColorShadow);
    char keyStr[32] = "---";
    if (d->State->LoadedTrack) {
        GetDynamicKey(d->State->TrackKey, d->State->TempoPercent, d->State->MasterTempo, keyStr);
    }
    Color keyCol = GetCamelotColor(keyStr);
    UIDrawText(keyStr, faceSm, col1X + S(6), contentY + S(11), S(10), keyCol);

    // Column 2: BAR
    UIDrawText("BAR", faceXXS, col2X + S(2), contentY + S(4), S(7), ColorShadow);
    char barsVal[32] = "01.1";
    if (d->State->LoadedTrack) {
        long long posMs = d->State->PositionMs;
        int beatIdx = -1;
        for (int i = 0; i < d->State->LoadedTrack->Analysis.BeatGridCount; i++) {
            if (d->State->LoadedTrack->Analysis.BeatGrid[i].Time <= posMs) beatIdx = i;
            else break;
        }
        if (beatIdx >= 0) {
            int currentBeat = d->State->LoadedTrack->Analysis.BeatGrid[beatIdx].BeatNumber;
            int currentBar = 0;
            for (int i = 0; i <= beatIdx; i++) if (d->State->LoadedTrack->Analysis.BeatGrid[i].BeatNumber == 1) currentBar++;
            sprintf(barsVal, "%02d.%d", currentBar, currentBeat);
        }
    }
    UIDrawText(barsVal, faceSm, col2X + S(2), contentY + S(11), S(10), d->ID == 0 ? ColorOrange : ColorWhite);

    // --- Row 2: Utility Buttons (2 Rows x 2 Columns Grid) - COMMENTED OUT ---
    /*
    float utilY = contentY + statusH + S(5);
    float utilGap = S(4.0f);
    float utilW = (deckInfoW - margin * 2 - utilGap) / 2.0f;
    float utilH = S(24); // Enlarged from S(14)
    float utilY2 = utilY + utilH + S(6.0f); // Increased gap

    // 1. Master (Top-Left)
    Rectangle msRect = { margin, utilY, utilW, utilH };
    DrawRectangleRec(msRect, d->State->IsMaster ? Fade(ColorOrange, 0.3f) : ColorDark1);
    DrawRectangleLinesEx(msRect, S(1), d->State->IsMaster ? ColorOrange : ColorShadow);
    const char *msLbl = "MASTER";
    Vector2 msSz = MeasureTextEx(faceSm, msLbl, S(9), 1);
    UIDrawText(msLbl, faceSm, msRect.x + (utilW - msSz.x)/2.0f, msRect.y + (utilH - S(9))/2.0f, S(9), d->State->IsMaster ? ColorOrange : ColorShadow);

    // 2. Sync (Top-Right)
    Rectangle syRect = { margin + utilW + utilGap, utilY, utilW, utilH };
    bool syncActive = d->State->SyncMode > 0;
    bool isBeatSync = d->State->SyncMode == 2;
    bool blink = (d->State->IsPhaseDrifted && ((int)(GetTime() * 4) % 2 == 0));
    
    Color syncColor = isBeatSync ? ColorBlue : ColorWhite;
    if (blink) syncColor = ColorOrange;

    DrawRectangleRec(syRect, syncActive ? Fade(syncColor, 0.3f) : ColorDark1);
    DrawRectangleLinesEx(syRect, S(1), syncActive ? syncColor : ColorShadow);
    const char *syncLbl = (d->State->SyncMode == 2) ? "BEAT" : ((d->State->SyncMode == 1) ? "BPM" : "SYNC");
    Vector2 syncSz = MeasureTextEx(faceSm, syncLbl, S(9), 1);
    UIDrawText(syncLbl, faceSm, syRect.x + (utilW - syncSz.x)/2.0f, syRect.y + (utilH - S(9))/2.0f, S(9), syncActive ? ColorWhite : ColorShadow);

    // 3. MT (Bottom-Left)
    Rectangle mtRect = { margin, utilY2, utilW, utilH };
    DrawRectangleRec(mtRect, d->State->MasterTempo ? Fade(ColorRed, 0.3f) : ColorDark1);
    DrawRectangleLinesEx(mtRect, S(1), d->State->MasterTempo ? ColorRed : ColorShadow);
    const char *mtLbl = d->State->MasterTempo ? "MT (ON)" : "MT (OFF)";
    Vector2 mtSz = MeasureTextEx(faceSm, mtLbl, S(9), 1);
    UIDrawText(mtLbl, faceSm, mtRect.x + (utilW - mtSz.x)/2.0f, mtRect.y + (utilH - S(9))/2.0f, S(9), d->State->MasterTempo ? ColorWhite : ColorShadow);

    // 4. Vinyl / CDJ Mode (Bottom-Right)
    Rectangle viRect = { margin + utilW + utilGap, utilY2, utilW, utilH };
    bool vinylOn = d->State->VinylModeEnabled;
    Color viColor = vinylOn ? ColorBlue : ColorRed;
    DrawRectangleRec(viRect, Fade(viColor, 0.3f));
    DrawRectangleLinesEx(viRect, S(1), viColor);
    const char *viLbl = vinylOn ? "VINYL" : "CDJ";
    Vector2 viSz = MeasureTextEx(faceSm, viLbl, S(9), 1);
    UIDrawText(viLbl, faceSm, viRect.x + (utilW - viSz.x)/2.0f, viRect.y + (utilH - S(9))/2.0f, S(9), ColorWhite);
    */

    // --- Row 3: Main Controls (Cue, Play) COMMENTED OUT ---
    /*
    float btnH = S(26);
    float btnY = y + deckInfoH - btnH - S(6);
    float btnW = (deckInfoW - margin * 2 - S(6)) / 2.0f;

    // Cue
    Rectangle cueRect = { margin, btnY, btnW, btnH };
    bool hoverCue = CheckCollisionPointRec(Input_GetPointerPos(), cueRect);
    bool isCueing = d->State->IsCueActive;
    DrawRectangleRec(cueRect, isCueing ? Fade(ColorCue, 0.4f) : ColorDark1);
    DrawRectangleLinesEx(cueRect, S(1), isCueing ? ColorCue : ColorShadow);
    if (hoverCue) DrawRectangleLinesEx(cueRect, S(1.5f), ColorWhite);
    UIDrawText("CUE", faceXS, cueRect.x + (btnW - S(18))/2.0f, cueRect.y + S(9), S(8.5f), isCueing ? ColorWhite : ColorShadow);

    // Play/Pause
    Rectangle playRect = { margin + btnW + S(6), btnY, btnW, btnH };
    bool hoverPlay = CheckCollisionPointRec(Input_GetPointerPos(), playRect);
    bool isPlaying = d->State->IsPlaying;
    DrawRectangleRec(playRect, isPlaying ? Fade(ColorGreen, 0.4f) : ColorDark1);
    DrawRectangleLinesEx(playRect, S(1), isPlaying ? ColorGreen : ColorShadow);
    if (hoverPlay) DrawRectangleLinesEx(playRect, S(1.5f), ColorWhite);
    UIDrawText(isPlaying ? "\uf04c" : "\uf04b", faceIcon, playRect.x + (btnW - S(10))/2.0f, playRect.y + S(8), S(11), isPlaying ? ColorWhite : ColorShadow);
    */
}

void DeckInfoPanel_Init(DeckInfoPanel *p, int id, DeckState *state, AudioEngine *engine) {
    p->base.Update = DeckInfo_Update;
    p->base.Draw = DeckInfo_Draw;
    p->ID = id;
    p->State = state;
    p->Engine = engine;
    p->lastEjectTapTime = 0.0;
}
