#include "ui/browser/browser.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include <rlgl.h>
#include <stdio.h>
#include <string.h>

// Mock rekordbox data for display
static const char* mock_tracks[] = {
    "Late Night Drive", "Midnight City", "Sunrise Melodies", "Electric Dreams", 
    "After Hours", "Shadow Dancer", "Neon Nights", "Golden Hour", 
    "Urban Jungle", "Lost In Sound", "Velvet Sky", "Digital Mirage"
};
static const char* mock_artists[] = {
    "Kavinsky", "M83", "Petit Biscuit", "Daft Punk", 
    "Justice", "Gessafelstein", "Tame Impala", "Kygo", 
    "Disclosure", "ZHU", "Flume", "Glass Animals"
};
static const int mock_track_count = 12;

static const char* mock_playlists[] = {
    "SUMMER HITS 2024", "DEEP HOUSE", "TECHNO VIBES", "CHILLOUT ZONE", "CLASSIC ANTHEMS"
};
static const int mock_playlist_count = 5;

static const char* categories[] = {"FILENAME", "FOLDER", "PLAYLIST", "TRACK", "SEARCH"};

static int Browser_Update(Component *base) {
    BrowserRenderer *r = (BrowserRenderer *)base;
    BrowserState *s = r->State;

    if (!s->IsActive) return 0;

    int totalVisible = 9;
    int totalItems = 0;

    if (s->IsTagList) {
        totalItems = s->TagListCount;
    } else {
        switch (s->BrowseLevel) {
            case 0: totalItems = mock_track_count; break;
            case 1: totalItems = mock_playlist_count; break;
            case 2: totalItems = 5; break;
            case 3: totalItems = s->StorageCount; break;
        }
    }

    if (IsKeyReleased(KEY_DOWN)) {
        if (s->CursorPos + s->ScrollOffset < totalItems - 1) {
            if (s->CursorPos < totalVisible - 1) s->CursorPos++;
            else s->ScrollOffset++;
        }
    }
    if (IsKeyReleased(KEY_UP)) {
        if (s->CursorPos > 0) s->CursorPos--;
        else if (s->ScrollOffset > 0) s->ScrollOffset--;
    }

    if (IsKeyReleased(KEY_ENTER)) {
        if (s->BrowseLevel == 3) {
            s->BrowseLevel = 2; // Source to Categories
            s->CursorPos = s->ScrollOffset = 0;
        } else if (s->BrowseLevel == 2) {
            if (s->CursorPos == 2) s->BrowseLevel = 1; // Categories to Playlists
            else if (s->CursorPos == 3 || s->CursorPos == 0) s->BrowseLevel = 0; // Categories to Tracks
            s->CursorPos = s->ScrollOffset = 0;
        } else if (s->BrowseLevel == 1) {
            s->BrowseLevel = 0; // Playlists to Tracks
            s->CursorPos = s->ScrollOffset = 0;
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
        if (s->BrowseLevel == 0) s->BrowseLevel = 2; // Simple back-to-categories
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

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);

    // Sidebar
    float sidebarW = S(38);
    DrawRectangle(0, TOP_BAR_H, sidebarW, SCREEN_HEIGHT - TOP_BAR_H, ColorDark2);
    DrawLine(sidebarW, TOP_BAR_H, sidebarW, SCREEN_HEIGHT, ColorDark1);

    Font faceXS = UIFonts_GetFace(S(10));
    Font faceSm = UIFonts_GetFace(S(13));
    Font faceMd = UIFonts_GetFace(S(15));
    Font faceIcon = UIFonts_GetIcon(S(6));
    Font faceBrand = UIFonts_GetIconBrand(S(12));

    // Vertical Sidebar text
    rlPushMatrix();
    rlTranslatef(S(24), TOP_BAR_H + S(24), 0);
    rlRotatef(90, 0, 0, 1);
    UIDrawText("PLAYLIST BANK", faceXS, 0, 0, S(10), ColorShadow);
    rlPopMatrix();

    // Header Color & Text
    Color headerClr = ColorBlue;
    const char* titleText = "TRACKS";
    char countText[32] = "";

    if (s->BrowseLevel == 1) { headerClr = ColorDGreen; titleText = "PLAYLIST"; sprintf(countText, "TOTAL %d", mock_playlist_count); }
    else if (s->BrowseLevel == 2) { headerClr = ColorOrange; titleText = "BROWSE"; }
    else if (s->BrowseLevel == 3) { headerClr = ColorBlue; titleText = "SOURCE"; sprintf(countText, "TOTAL %d", s->StorageCount); }
    else { sprintf(countText, "TOTAL %d", mock_track_count); }

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

        switch (s->BrowseLevel) {
            case 0:
                if (idx < mock_track_count) {
                    title = mock_tracks[idx];
                    artist = mock_artists[idx];
                }
                break;
            case 1:
                if (idx < mock_playlist_count) title = mock_playlists[idx];
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
             const char* icon = "\uf287"; // f287 usb
             if (strcmp(s->AvailableStorages[idx].Type, "SD") == 0) icon = "\uf7c2"; // f7c2 sd-card
             UIDrawText(icon, faceBrand, listX + S(11), ry + S(7), S(12), ColorWhite);
        }
    }

    // Scrollbar
    int maxItems = 0;
    if (s->BrowseLevel == 0) maxItems = mock_track_count;
    else if (s->BrowseLevel == 1) maxItems = mock_playlist_count;
    else if (s->BrowseLevel == 2) maxItems = 5;
    else if (s->BrowseLevel == 3) maxItems = s->StorageCount;

    if (maxItems > 9) {
        float listAreaH = SCREEN_HEIGHT - TOP_BAR_H;
        float thumbH = (9.0f / maxItems) * listAreaH;
        if (thumbH < S(10)) thumbH = S(10);
        float thumbY = TOP_BAR_H + ((float)s->ScrollOffset / maxItems) * listAreaH;
        DrawRectangle(SCREEN_WIDTH - S(4), thumbY, S(2), thumbH, ColorWhite);
    }
}

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state) {
    r->base.Update = Browser_Update;
    r->base.Draw = Browser_Draw;
    r->State = state;
}
