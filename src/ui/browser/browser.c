#include "audio/engine.h"
#include "ui/player/player_state.h"
#include "ui/browser/browser.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define CloseWindow WinCloseWindow
#define ShowCursor WinShowCursor
#include <windows.h>
#include <direct.h>
#undef CloseWindow
#undef ShowCursor
#undef NOGDI
#else
#include <unistd.h>
#endif

static const char* categories[] = {"FILENAME", "FOLDER", "PLAYLIST", "TRACK", "SEARCH"};

static void Browser_UpdateActiveTracks(BrowserState *s) {
    if (!s->DB) {
        s->ActiveTrackCount = 0;
        return;
    }

    if (s->IsTagList) {
        s->ActiveTrackCount = s->TagListCount;
        for (int i = 0; i < s->TagListCount; i++) {
            // Find track by ID
            s->TrackPointers[i] = NULL;
            for (uint32_t j = 0; j < s->DB->TrackCount; j++) {
                if (s->DB->Tracks[j].ID == s->TagList[i]) {
                    s->TrackPointers[i] = &s->DB->Tracks[j];
                    break;
                }
            }
        }
    } else if (s->CurrentPlaylistIdx >= 0 && s->CurrentPlaylistIdx < (int)s->DB->PlaylistCount) {
        RBPlaylist *pl = &s->DB->Playlists[s->CurrentPlaylistIdx];
        s->ActiveTrackCount = pl->TrackCount;
        for (uint32_t i = 0; i < pl->TrackCount; i++) {
            uint32_t tid = pl->TrackIDs[i];
            s->TrackPointers[i] = NULL;
            // Map ID to pointer
            for (uint32_t j = 0; j < s->DB->TrackCount; j++) {
                if (s->DB->Tracks[j].ID == tid) {
                    s->TrackPointers[i] = &s->DB->Tracks[j];
                    break;
                }
            }
        }
    } else {
        s->ActiveTrackCount = s->DB->TrackCount;
        for (uint32_t i = 0; i < s->DB->TrackCount; i++) {
            s->TrackPointers[i] = &s->DB->Tracks[i];
        }
    }
}

void Browser_Back(BrowserState *s) {
    if (s->IsTagList) {
        s->IsTagList = false;
    } else if (s->BrowseLevel < 3) {
        s->BrowseLevel++;
        s->CursorPos = s->ScrollOffset = 0;
        Browser_UpdateActiveTracks(s);
    }
}

void Browser_RefreshStorages(BrowserState *s) {
    s->StorageCount = 0;
    
    // 1. Check for testing storage
    struct stat st;
    if (stat("usb_test/PIONEER/rekordbox/export.pdb", &st) == 0) {
        strcpy(s->AvailableStorages[s->StorageCount].Name, "Testing USB");
        strcpy(s->AvailableStorages[s->StorageCount].Path, "usb_test");
        strcpy(s->AvailableStorages[s->StorageCount].Type, "Testing");
        s->StorageCount++;
    }

#ifdef _WIN32
    // 2. Scan drive letters D..Z
    for (char drive = 'D'; drive <= 'Z'; drive++) {
        char path[32];
        sprintf(path, "%c:/PIONEER/rekordbox/export.pdb", drive);
        if (stat(path, &st) == 0) {
            sprintf(s->AvailableStorages[s->StorageCount].Name, "USB (%c:)", drive);
            sprintf(s->AvailableStorages[s->StorageCount].Path, "%c:/", drive);
            strcpy(s->AvailableStorages[s->StorageCount].Type, "USB");
            s->StorageCount++;
            if (s->StorageCount >= 8) break;
        }
    }
#endif
}

static int Browser_Update(Component *base) {
    BrowserRenderer *r = (BrowserRenderer *)base;
    BrowserState *s = r->State;

    if (!s->IsActive) return 0;

    // Mouse Interaction
    Vector2 mousePos = UIGetMousePosition();
    float sidebarW = S(40);
    float rowH = S(26);
    float listW = SCREEN_WIDTH - sidebarW - S(8);
    if (s->InfoEnabled) listW = SCREEN_WIDTH - sidebarW - S(160);

    // 1. Sidebar Clicking & Interaction
    for (int i = 0; i < 7; i++) {
        float boxY = TOP_BAR_H + i * sidebarW;
        Rectangle boxRect = {0, boxY, sidebarW, sidebarW};
        
        if (CheckCollisionPointRec(mousePos, boxRect)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (i < 4) {
                    // Navigate to categories (Playlist, Folder, Search, History)
                    s->BrowseLevel = 2; // Categories level
                    s->CursorPos = i; 
                    s->ScrollOffset = 0;
                    // Trigger "Enter" logic for the category
                    if (s->CursorPos == 2) { s->BrowseLevel = 1; } // Playlists
                    else if (s->CursorPos == 3 || s->CursorPos == 0) { 
                        s->BrowseLevel = 0; // Tracks
                        s->CurrentPlaylistIdx = -1;
                        Browser_UpdateActiveTracks(s);
                    }
                    s->CursorPos = 0;
                } else {
                    // Playlist Bank Jump
                    int bankIdx = i - 4;
                    if (s->PlaylistBankIdx[bankIdx] >= 0) {
                        s->CurrentPlaylistIdx = s->PlaylistBankIdx[bankIdx];
                        s->BrowseLevel = 0; // Tracks
                        Browser_UpdateActiveTracks(s);
                        s->CursorPos = s->ScrollOffset = 0;
                    }
                }
            }
            
            // Handle Dropping into Bank
            if (s->IsDragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                if (i >= 4) {
                    int bankIdx = i - 4;
                    if (s->DraggingType == 1) { // Only playlists can be banked
                        s->PlaylistBankIdx[bankIdx] = s->DraggingIdx;
                        printf("[BROWSER] Banked Playlist %d to Slot %d\n", s->DraggingIdx, bankIdx + 1);
                    }
                }
                s->IsDragging = false;
            }
        }
    }

    // 2. List Item Interaction
    int totalVisible = 9;
    int totalItems = 0;
    if (s->IsTagList) totalItems = s->TagListCount;
    else {
        switch (s->BrowseLevel) {
            case 0: totalItems = s->ActiveTrackCount; break;
            case 1: totalItems = s->DB ? s->DB->PlaylistCount : 0; break;
            case 2: totalItems = 5; break;
            case 3: totalItems = s->StorageCount; break;
        }
    }

    for (int i = 0; i < totalVisible; i++) {
        int idx = s->ScrollOffset + i;
        if (idx >= totalItems) break;

        Rectangle itemRect = {sidebarW, TOP_BAR_H + i * rowH, listW, rowH};
        if (CheckCollisionPointRec(mousePos, itemRect)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Drag start for Playlists
                if (s->BrowseLevel == 1) {
                    s->IsDragging = true;
                    s->DraggingIdx = idx;
                    s->DraggingType = 1;
                }
                
                // Click = Select + Execute (Mocking double-click check)
                static float lastClickTime = 0;
                float now = GetTime();
                if (s->CursorPos == i && (now - lastClickTime < 0.3f)) {
                    // Double Clicked -> Trigger Enter logic
                    // ... existing Enter logic ...
                    // We can reuse the IsKeyPressed(KEY_ENTER) logic block below or refactor
                }
                s->CursorPos = i;
                lastClickTime = now;
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) s->IsDragging = false;

    if (IsKeyPressed(KEY_DOWN)) {
        if (s->CursorPos + s->ScrollOffset < totalItems - 1) {
            if (s->CursorPos < totalVisible - 1) s->CursorPos++;
            else s->ScrollOffset++;
        }
    }
    if (IsKeyPressed(KEY_UP)) {
        if (s->CursorPos > 0) s->CursorPos--;
        else if (s->ScrollOffset > 0) s->ScrollOffset--;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (s->BrowseLevel == 3) {
            int idx = s->ScrollOffset + s->CursorPos;
            if (idx < s->StorageCount) {
                s->SelectedStorage = &s->AvailableStorages[idx];
                if (s->DB) RB_FreeDatabase(s->DB);
                s->DB = RB_LoadDatabase(s->SelectedStorage->Path);
                if (s->DB) {
                    if (s->TrackPointers) free(s->TrackPointers);
                    s->TrackPointers = (RBTrack**)malloc(s->DB->TrackCount * sizeof(RBTrack*));
                }
                
                s->BrowseLevel = 2; // Source to Categories
                s->CursorPos = s->ScrollOffset = 0;
            }
        } else if (s->BrowseLevel == 2) {
            if (s->CursorPos == 2) { 
                s->BrowseLevel = 1; // Categories to Playlists
            } else if (s->CursorPos == 3 || s->CursorPos == 0) { 
                s->BrowseLevel = 0; // Categories to Tracks
                s->CurrentPlaylistIdx = -1;
                Browser_UpdateActiveTracks(s);
            }
            s->CursorPos = s->ScrollOffset = 0;
        } else if (s->BrowseLevel == 1) {
            int idx = s->ScrollOffset + s->CursorPos;
            if (s->DB && idx < (int)s->DB->PlaylistCount) {
                s->CurrentPlaylistIdx = idx;
                s->BrowseLevel = 0; // Playlists to Tracks
                Browser_UpdateActiveTracks(s);
                s->CursorPos = s->ScrollOffset = 0;
            }
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        Browser_Back(s);
    }

    int loadToDeck = -1;
    if (IsKeyPressed(KEY_LEFT)) loadToDeck = 0;
    if (IsKeyPressed(KEY_RIGHT)) loadToDeck = 1;

    if (s->BrowseLevel == 0 && loadToDeck != -1) {
        // LOAD TRACK
        struct DeckState *targetDeck = loadToDeck == 0 ? s->DeckA : s->DeckB;
        if (targetDeck && targetDeck->Waveform.LoadLock && targetDeck->IsPlaying) {
            printf("[BROWSER] LOAD LOCKED: Deck %c is playing\n", loadToDeck == 0 ? 'A' : 'B');
            return 0; // Prevent load
        }

        int idx = s->ScrollOffset + s->CursorPos;
        if (idx < s->ActiveTrackCount && s->TrackPointers[idx]) {
            RBTrack *t = s->TrackPointers[idx];
            printf("[BROWSER] Loading track: %s to Deck %c\n", t->Title, loadToDeck == 0 ? 'A' : 'B');
            
            // Load analysis data (.DAT/.EXT)
            if (s->SelectedStorage) {
                RB_LoadTrackData(t, s->SelectedStorage->Path);
                printf("[BROWSER] Loaded %d cues and beatgrid for %s\n", t->CueCount, t->Title);
                
                if (s->AudioPlugin) {
                    char fullPath[1024];
                    const char* relPath = t->FilePath;
                    if (relPath[0] == '/' || relPath[0] == '\\') relPath++;
                    snprintf(fullPath, sizeof(fullPath), "%s/%s", s->SelectedStorage->Path, relPath);
                    printf("[BROWSER] Audio Path: %s\n", fullPath);
                    
                    DeckAudio_LoadTrack(&s->AudioPlugin->Decks[loadToDeck], fullPath);
                    // Do not auto-play, let user press Play/Pause
                }
                
                struct DeckState *targetDeck = loadToDeck == 0 ? s->DeckA : s->DeckB;
                if (targetDeck) {
                    strcpy(targetDeck->TrackTitle, t->Title);
                    strcpy(targetDeck->ArtistName, t->Artist);
                    strcpy(targetDeck->TrackKey, t->Key);

                    // Setup Artwork Path
                    if (t->ArtworkPath[0] != '\0') {
                        const char *artRel = t->ArtworkPath;
                        if (artRel[0] == '/' || artRel[0] == '\\') artRel++;
                        snprintf(targetDeck->ArtworkPath, sizeof(targetDeck->ArtworkPath), "%s/%s", s->SelectedStorage->Path, artRel);
                    } else {
                        targetDeck->ArtworkPath[0] = '\0';
                    }

                    targetDeck->OriginalBPM = t->BPM;
                    targetDeck->CurrentBPM = t->BPM;
                    
                    // Safe re-allocation for UI thread stability
                    TrackState *newTrack = (TrackState*)malloc(sizeof(TrackState));
                    if (!newTrack) {
                        printf("[BROWSER] Error: Failed to allocate TrackState\n");
                    } else {
                        memset(newTrack, 0, sizeof(TrackState));
                        
                        // Copy Static Waveform
                        newTrack->StaticWaveformLen = t->StaticWaveformLen;
                        int copyLen = t->StaticWaveformLen > 1024 ? 1024 : t->StaticWaveformLen;
                        memcpy(newTrack->StaticWaveform, t->StaticWaveform, copyLen);
                        
                        // Assign Dynamic Waveform Reference
                        newTrack->DynamicWaveform = t->DynamicWaveform;
                        newTrack->DynamicWaveformLen = t->DynamicWaveformLen;
                        
                        // Copy Beatgrid
                        newTrack->GridOffset = 0;
                        newTrack->BeatGridCount = t->BeatGridCount > 1024 ? 1024 : t->BeatGridCount;
                        for(int i=0; i<newTrack->BeatGridCount; i++) {
                            newTrack->BeatGrid[i] = t->BeatGrid[i];
                        }

                        // Copy Cues
                        for(uint32_t i=0; i<t->CueCount; i++) {
                            if (t->Cues[i].ID >= 1 && t->Cues[i].ID <= 8) {
                                if (newTrack->HotCuesCount < 8) {
                                    newTrack->HotCues[newTrack->HotCuesCount].ID = t->Cues[i].ID;
                                    newTrack->HotCues[newTrack->HotCuesCount].Start = t->Cues[i].Time;
                                    newTrack->HotCues[newTrack->HotCuesCount].Color[0] = t->Cues[i].Color[0];
                                    newTrack->HotCues[newTrack->HotCuesCount].Color[1] = t->Cues[i].Color[1];
                                    newTrack->HotCues[newTrack->HotCuesCount].Color[2] = t->Cues[i].Color[2];
                                    newTrack->HotCuesCount++;
                                }
                            } else if (t->Cues[i].ID == 0) {
                                if (newTrack->CuesCount < 32) {
                                    newTrack->Cues[newTrack->CuesCount].ID = 0;
                                    newTrack->Cues[newTrack->CuesCount].Start = t->Cues[i].Time;
                                    newTrack->Cues[newTrack->CuesCount].Color[0] = t->Cues[i].Color[0];
                                    newTrack->Cues[newTrack->CuesCount].Color[1] = t->Cues[i].Color[1];
                                    newTrack->Cues[newTrack->CuesCount].Color[2] = t->Cues[i].Color[2];
                                    newTrack->CuesCount++;
                                }
                            }
                        }

                        // Copy Phrases
                        newTrack->PhraseCount = 0;
                        if (t->Phrases && t->PhraseCount > 0) {
                            for (uint32_t i = 0; i < t->PhraseCount && i < 64; i++) {
                                newTrack->Phrases[i].Index = t->Phrases[i].Index;
                                newTrack->Phrases[i].Beat = t->Phrases[i].Beat;
                                newTrack->Phrases[i].KindID = t->Phrases[i].KindID;
                                strcpy(newTrack->Phrases[i].Kind, t->Phrases[i].Kind);
                                newTrack->PhraseCount++;
                            }
                        }

                        // Atomic-like swap
                        TrackState *oldTrack = targetDeck->LoadedTrack;
                        targetDeck->LoadedTrack = newTrack;
                        if (oldTrack) free(oldTrack);
                        
                        // Reset playhead for new track
                        targetDeck->Position = 0;
                        targetDeck->PositionMs = 0;
                        if (targetDeck->IsPlaying) {
                            targetDeck->IsPlaying = false;
                        }

                        // Auto-Cue: Find the absolute first cue point across all types
                        uint32_t firstMs = 0;
                        bool foundCue = false;

                        // Check Memory Cues
                        for (int i = 0; i < newTrack->CuesCount; i++) {
                            if (!foundCue || newTrack->Cues[i].Start < firstMs) {
                                firstMs = newTrack->Cues[i].Start;
                                foundCue = true;
                            }
                        }
                        // Check Hot Cues
                        for (int i = 0; i < newTrack->HotCuesCount; i++) {
                            if (!foundCue || newTrack->HotCues[i].Start < firstMs) {
                                firstMs = newTrack->HotCues[i].Start;
                                foundCue = true;
                            }
                        }
                        // Fallback to first BeatGrid if no cues
                        if (!foundCue && newTrack->BeatGridCount > 0) {
                            firstMs = newTrack->BeatGrid[0].Time;
                        }

                        targetDeck->PositionMs = firstMs;
                        if (s->AudioPlugin) {
                            DeckAudio_JumpToMs(&s->AudioPlugin->Decks[loadToDeck], firstMs);
                            
                            // Sync back the generated fractional frame position right away for the UI
                            double sr = (double)s->AudioPlugin->Decks[loadToDeck].SampleRate;
                            if (sr < 8000) sr = 44100.0;
                            targetDeck->Position = (s->AudioPlugin->Decks[loadToDeck].Position * 150.0) / sr;
                        }
                    }
                }
            }
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
        if (s->BrowseLevel == 0) {
            if (s->CurrentPlaylistIdx >= 0) s->BrowseLevel = 1;
            else s->BrowseLevel = 2;
        }
        else if (s->BrowseLevel == 1) s->BrowseLevel = 2;
        else if (s->BrowseLevel == 2) s->BrowseLevel = 3;
        s->CursorPos = s->ScrollOffset = 0;
    }

    return 0;
}

static void Browser_Draw(Component *base) {
    BrowserRenderer *r = (BrowserRenderer *)base;
    BrowserState *s = r->State;

    if (!s->IsActive) return;

    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    DrawRectangle(0, 0, SCREEN_WIDTH, viewH, ColorBlack);

    // Sidebar
    float sidebarW = S(40);
    DrawRectangle(0, TOP_BAR_H, sidebarW, viewH - TOP_BAR_H, ColorDark2);
    DrawLine(sidebarW, TOP_BAR_H, sidebarW, viewH, ColorDark1);

    Font faceXS = UIFonts_GetFace(S(10));
    Font faceSm = UIFonts_GetFace(S(13));
    Font faceMd = UIFonts_GetFace(S(15));
    Font faceIcon = UIFonts_GetIcon(S(6));
    Font faceBrand = UIFonts_GetIconBrand(S(12));
    (void)faceMd;
    (void)faceIcon;
    (void)faceBrand;

    // Sidebar Boxes (1:1 Squares)
    Vector2 mPos = UIGetMousePosition();
    for (int i = 0; i < 7; i++) {
        float boxY = TOP_BAR_H + i * sidebarW;
        Rectangle boxRect = {0, boxY, sidebarW, sidebarW};
        bool isHovered = CheckCollisionPointRec(mPos, boxRect);
        bool isBank = (i >= 4);
        bool isAssigned = isBank && (s->PlaylistBankIdx[i-4] >= 0);
        
        // Background
        Color bg = isHovered ? ColorBlue : (isBank ? (isAssigned ? ColorDGreen : ColorDark3) : ColorDark2);
        DrawRectangle(0, boxY, sidebarW, sidebarW, bg);
        
        // Inner separation lines
        DrawRectangleLinesEx(boxRect, 1.0f, ColorDark1);
        
        if (!isBank) {
            // Icon / Symbol for main navigation
            const char* sidIcons[] = {"\uf03c", "\uf07b", "\uf002", "\uf017"}; // Playlist, Folder, Search, History
            UIDrawText(sidIcons[i], faceIcon, S(12), boxY + S(12), S(16), ColorWhite);
        } else {
            // Playlist Bank Placeholders (1-3)
            char bankNum[4];
            sprintf(bankNum, "P%d", i - 3);
            // Draw a bookmark icon as a background for the bank spot
            UIDrawText("\uf02e", faceIcon, S(13), boxY + S(8), S(14), isAssigned ? ColorWhite : ColorShadow); 
            UIDrawText(bankNum, faceXS, S(14), boxY + S(24), S(10), ColorWhite);
        }
    }

    // Header Color & Text
    Color headerClr = ColorBlue;
    const char* titleText = "TRACKS";
    char countText[32] = "";

    if (s->BrowseLevel == 1) { 
        headerClr = ColorDGreen; titleText = "PLAYLIST"; 
        sprintf(countText, "TOTAL %d", s->DB ? s->DB->PlaylistCount : 0); 
    }
    else if (s->BrowseLevel == 2) { headerClr = ColorOrange; titleText = "BROWSE"; }
    else if (s->BrowseLevel == 3) { 
        Browser_RefreshStorages(s); // Refresh on Source screen
        headerClr = ColorBlue; titleText = "SOURCE"; 
        sprintf(countText, "TOTAL %d", s->StorageCount); 
    }
    else { sprintf(countText, "TOTAL %d", s->DB ? s->DB->TrackCount : 0); }

    UIDrawText(titleText, faceSm, sidebarW + S(8), TOP_BAR_H - S(14), S(13), headerClr);
    if (countText[0]) {
        UIDrawText(countText, faceXS, SCREEN_WIDTH - S(80), TOP_BAR_H - S(14), S(10), headerClr);
    }

    float listX = sidebarW + S(4);
    float listW = SCREEN_WIDTH - sidebarW - S(8);
    if (s->InfoEnabled) listW = SCREEN_WIDTH - sidebarW - S(160);
    float rowH = S(26);

    for (int i = 0; i < 9; i++) {
        int idx = s->ScrollOffset + i;
        const char* title = "";
        const char* artist = "";
        const char *bpmText = "124.0";
        const char *keyStr = "12A";
        bool isPlaying = false;
        (void)isPlaying;

        switch (s->BrowseLevel) {
            case 0:
                if (idx < s->ActiveTrackCount && s->TrackPointers[idx]) {
                    RBTrack *t = s->TrackPointers[idx];
                    title = t->Title;
                    artist = t->Artist;
                    static char bpmBuf[16];
                    sprintf(bpmBuf, "%.1f", t->BPM);
                    bpmText = bpmBuf;
                    keyStr = t->Key;
                }
                break;
            case 1:
                if (s->DB && idx >= 0 && (uint32_t)idx < s->DB->PlaylistCount) title = s->DB->Playlists[idx].Name;
                break;
            case 2:
                if (idx < 5) title = categories[idx];
                break;
            case 3:
                if (idx < s->StorageCount) title = s->AvailableStorages[idx].Name;
                break;
        }

        if (title[0] == '\0') continue;

        float ry = TOP_BAR_H + i * rowH;
        bool isCursor = (i == s->CursorPos);

        if (isCursor) {
            DrawRectangle(listX, ry + 1, listW, rowH - 2, ColorBlue);
            if (s->BrowseLevel > 0 && !s->IsTagList) {
                DrawSelectionTriangle(listX + S(2), ry + S(8), ColorWhite);
            }
        } else if (i % 2 != 0) {
            DrawRectangle(listX, ry + 1, listW, rowH - 2, ColorDark2);
        }

        float textX = listX + S(36);
        if (s->BrowseLevel == 3) textX = listX + S(38);
        else if (s->BrowseLevel > 0) textX = listX + S(20);

        float textY = ry + (artist[0] == '\0' ? S(6) : S(2));
        UIDrawText(title, faceSm, textX, textY, S(13), ColorWhite);

        if (artist[0] != '\0' && s->BrowseLevel == 0 && !s->InfoEnabled) {
            UIDrawText(artist, faceXS, textX, ry + S(15), S(10), isCursor ? ColorWhite : ColorShadow);
        }

        // BPM & Key
        if (s->BrowseLevel == 0 && !s->InfoEnabled) {
            UIDrawText(bpmText, faceXS, listX + listW - S(80), ry + S(4), S(10), isCursor ? ColorWhite : ColorShadow);
            UIDrawText(keyStr, faceXS, listX + listW - S(30), ry + S(4), S(10), isCursor ? ColorWhite : ColorShadow);
        }

        // Storage icons
        if (s->BrowseLevel == 3 && idx < s->StorageCount) {
             const char* icon = "\uf287"; // uf287 usb (UTF-8)
             if (strcmp(s->AvailableStorages[idx].Type, "SD") == 0) icon = "\uf7c2"; // uf7c2 sd-card
             UIDrawText(icon, faceBrand, listX + S(11), ry + S(7), S(12), ColorWhite);
        }
    }

    // Scrollbar
    int maxItems = 0;
    if (s->BrowseLevel == 0) maxItems = s->ActiveTrackCount;
    else if (s->BrowseLevel == 1) maxItems = s->DB ? s->DB->PlaylistCount : 0;
    else if (s->BrowseLevel == 2) maxItems = 5;
    else if (s->BrowseLevel == 3) maxItems = s->StorageCount;

    if (maxItems > 9) {
        float listAreaH = SCREEN_HEIGHT - TOP_BAR_H - DECK_STR_H;
        float thumbH = (9.0f / maxItems) * listAreaH;
        if (thumbH < S(10)) thumbH = S(10);
        float thumbY = TOP_BAR_H + ((float)s->ScrollOffset / maxItems) * listAreaH;
        DrawRectangle(SCREEN_WIDTH - S(4), thumbY, S(2), thumbH, ColorWhite);
    }

    // Drag and Drop Preview
    if (s->IsDragging) {
        const char *dragTitle = "Playlist Item";
        if (s->DraggingType == 1 && s->DB && s->DraggingIdx >= 0 && (uint32_t)s->DraggingIdx < s->DB->PlaylistCount) {
             dragTitle = s->DB->Playlists[s->DraggingIdx].Name;
        }
        DrawRectangle(mPos.x, mPos.y, S(120), rowH, Fade(ColorBlue, 0.6f));
        UIDrawText(dragTitle, faceSm, mPos.x + S(8), mPos.y + S(6), S(13), ColorWhite);
    }
}

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state) {
    r->base.Update = Browser_Update;
    r->base.Draw = Browser_Draw;
    r->State = state;
}
