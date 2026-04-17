#include "ui/browser/browser.h"
#include "audio/engine.h"
#include "rlgl.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/player_state.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define CloseWindow WinCloseWindow
#define ShowCursor WinShowCursor
#include <direct.h>
#include <windows.h>

#undef CloseWindow
#undef ShowCursor
#undef NOGDI
#else
#include <dirent.h>
#include <unistd.h>

#endif

static const char *categories[] = {"FILENAME", "FOLDER", "PLAYLIST", "TRACK",
                                   "SEARCH"};

static void Browser_UpdateActiveTracks(BrowserState *s) {
  if (s->DatabaseType == 0) { // Rekordbox
    if (!s->DB) {
      s->ActiveTrackCount = 0;
      return;
    }

    if (s->IsTagList) {
      s->ActiveTrackCount = s->TagListCount;
      for (int i = 0; i < s->TagListCount; i++) {
        s->TrackPointers[i] = NULL;
        for (uint32_t j = 0; j < s->DB->TrackCount; j++) {
          if (s->DB->Tracks[j].ID == s->TagList[i]) {
            s->TrackPointers[i] = &s->DB->Tracks[j];
            break;
          }
        }
      }
    } else if (s->CurrentPlaylistIdx >= 0 &&
               s->CurrentPlaylistIdx < (int)s->DB->PlaylistCount) {
      RBPlaylist *pl = &s->DB->Playlists[s->CurrentPlaylistIdx];
      s->ActiveTrackCount = pl->TrackCount;
      for (uint32_t i = 0; i < pl->TrackCount; i++) {
        uint32_t tid = pl->TrackIDs[i];
        s->TrackPointers[i] = NULL;
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
  } else { // Serato
    if (!s->SeratoDB) {
      s->ActiveTrackCount = 0;
      return;
    }

    if (s->IsTagList) {
      // Tags not implemented for Serato yet
      s->ActiveTrackCount = 0;
    } else if (s->CurrentPlaylistIdx >= 0 &&
               s->CurrentPlaylistIdx < (int)s->SeratoDB->PlaylistCount) {
      SeratoPlaylist *pl = &s->SeratoDB->Playlists[s->CurrentPlaylistIdx];
      s->ActiveTrackCount = pl->TrackCount;
      for (uint32_t i = 0; i < pl->TrackCount; i++) {
        uint32_t tid = pl->TrackIDs[i];
        s->SeratoTrackPointers[i] = NULL;
        for (uint32_t j = 0; j < s->SeratoDB->TrackCount; j++) {
          if (s->SeratoDB->Tracks[j].ID == tid) {
            s->SeratoTrackPointers[i] = &s->SeratoDB->Tracks[j];
            break;
          }
        }
      }
    } else {
      s->ActiveTrackCount = s->SeratoDB->TrackCount;
      for (uint32_t i = 0; i < s->SeratoDB->TrackCount; i++) {
        s->SeratoTrackPointers[i] = &s->SeratoDB->Tracks[i];
      }
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
    strcpy(s->AvailableStorages[s->StorageCount].Name, "Testing USB (RB)");
    strcpy(s->AvailableStorages[s->StorageCount].Path, "usb_test");
    strcpy(s->AvailableStorages[s->StorageCount].Type, "Rekordbox");
    s->StorageCount++;
  } else if (stat("usb_test/_Serato_/database V2", &st) == 0) {
    strcpy(s->AvailableStorages[s->StorageCount].Name, "Testing USB (Serato)");
    strcpy(s->AvailableStorages[s->StorageCount].Path, "usb_test");
    strcpy(s->AvailableStorages[s->StorageCount].Type, "Serato");
    s->StorageCount++;
  }

#ifdef _WIN32
  // 2. Scan drive letters D..Z
  for (char drive = 'D'; drive <= 'Z'; drive++) {
    char path[32];
    sprintf(path, "%c:/PIONEER/rekordbox/export.pdb", drive);
    if (stat(path, &st) == 0) {
      sprintf(s->AvailableStorages[s->StorageCount].Name, "USB (%c:) RB", drive);
      sprintf(s->AvailableStorages[s->StorageCount].Path, "%c:/", drive);
      strcpy(s->AvailableStorages[s->StorageCount].Type, "Rekordbox");
      s->StorageCount++;
      if (s->StorageCount >= 16) break;
    }
    sprintf(path, "%c:/_Serato_/database V2", drive);
    if (stat(path, &st) == 0) {
      sprintf(s->AvailableStorages[s->StorageCount].Name, "USB (%c:) Serato", drive);
      sprintf(s->AvailableStorages[s->StorageCount].Path, "%c:/", drive);
      strcpy(s->AvailableStorages[s->StorageCount].Type, "Serato");
      s->StorageCount++;
      if (s->StorageCount >= 16) break;
    }
  }
#else
  // 2. Scan Android / Linux /storage directory for OTG / SD Cards
  if (s->StorageCount < 8) {
    strcpy(s->AvailableStorages[s->StorageCount].Name, "Internal Storage");
    strcpy(s->AvailableStorages[s->StorageCount].Path, "/storage/emulated/0");
    strcpy(s->AvailableStorages[s->StorageCount].Type, "Internal");
    s->StorageCount++;
  }

  const char *scanDirs[] = {"/storage", "/mnt", "/media", "/run/media"};
  for (int i = 0; i < 4; i++) {
    DIR *d = opendir(scanDirs[i]);
    if (d) {
      struct dirent *dir;
      while ((dir = readdir(d)) != NULL) {
        if (s->StorageCount >= 16)
          break;

        if (dir->d_name[0] == '.')
          continue;
        
        // Skip system folders
        if (strcmp(dir->d_name, "self") == 0 ||
            strcmp(dir->d_name, "emulated") == 0 ||
            strcmp(dir->d_name, "knox-emulated") == 0 ||
            strcmp(dir->d_name, "container") == 0 ||
            strcmp(dir->d_name, "secure") == 0 ||
            strcmp(dir->d_name, "asec") == 0 ||
            strcmp(dir->d_name, "obb") == 0)
          continue;

        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", scanDirs[i], dir->d_name);

        struct stat st_dir;
        if (stat(fullPath, &st_dir) == 0 && S_ISDIR(st_dir.st_mode)) {
          bool exists = false;
          for (int j = 0; j < s->StorageCount; j++) {
            if (strcmp(s->AvailableStorages[j].Path, fullPath) == 0) {
              exists = true;
              break;
            }
          }
          if (exists) continue;

          // Check for DB type
          char dbPath[1024];
          bool hasRB = false;
          bool hasSerato = false;
          
          snprintf(dbPath, sizeof(dbPath), "%s/PIONEER/rekordbox/export.pdb", fullPath);
          if (stat(dbPath, &st) == 0) hasRB = true;
          
          snprintf(dbPath, sizeof(dbPath), "%s/_Serato_/database V2", fullPath);
          if (stat(dbPath, &st) == 0) hasSerato = true;

          const char *type = "USB";
          if (hasRB && hasSerato) type = "RB/Serato";
          else if (hasRB) type = "Rekordbox";
          else if (hasSerato) type = "Serato";
          else if (strchr(dir->d_name, '-') != NULL) type = "SD";

          snprintf(s->AvailableStorages[s->StorageCount].Name,
                   sizeof(s->AvailableStorages[0].Name), "%s", dir->d_name);
          snprintf(s->AvailableStorages[s->StorageCount].Path,
                   sizeof(s->AvailableStorages[0].Path), "%s", fullPath);
          strcpy(s->AvailableStorages[s->StorageCount].Type, type);
          s->StorageCount++;
        }
      }
      closedir(d);
    }
  }
#endif
}

static int Browser_Update(Component *base) {
  BrowserRenderer *r = (BrowserRenderer *)base;
  BrowserState *s = r->State;

  if (!s->IsActive)
    return 0;

  int loadToDeck = -1;
  int targetIdx = s->ScrollOffset + s->CursorPos;
  Vector2 mousePos = UIGetMousePosition();

  // Load Popup Dialog Interaction handled FIRST to prevent same-frame double
  // triggers
  bool wasPopupOpen = s->ShowLoadPopup;
  if (wasPopupOpen) {
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      float pw = S(240);
      float ph = S(120);
      float viewH = SCREEN_HEIGHT - DECK_STR_H;
      float px = (SCREEN_WIDTH - pw) / 2.0f;
      float py = (viewH - ph) / 2.0f;

      Rectangle deckARect = {px, py + ph / 2.0f, pw / 2.0f, ph / 2.0f};
      Rectangle deckBRect = {px + pw / 2.0f, py + ph / 2.0f, pw / 2.0f,
                             ph / 2.0f};

      if (CheckCollisionPointRec(mousePos, deckARect)) {
        loadToDeck = 0;
        targetIdx = s->PopupTrackIdx;
        s->ShowLoadPopup = false;
      } else if (CheckCollisionPointRec(mousePos, deckBRect)) {
        loadToDeck = 1;
        targetIdx = s->PopupTrackIdx;
        s->ShowLoadPopup = false;
      } else if (!CheckCollisionPointRec(mousePos,
                                         (Rectangle){px, py, pw, ph})) {
        // Backdrop click
        s->ShowLoadPopup = false;
      }
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
      s->ShowLoadPopup = false;
    }

    // If we made a choice, continue to the loading logic at the bottom
    // Otherwise, block other inputs for this frame
    if (loadToDeck == -1)
      return 0;
  } else {
    if (IsKeyPressed(KEY_LEFT))
      loadToDeck = 0;
    if (IsKeyPressed(KEY_RIGHT))
      loadToDeck = 1;
  }

  // Mouse Interaction
  float sidebarW = S(40);
  float rowH = S(28.0f);
  int totalVisible = 10;
  float listW = SCREEN_WIDTH - sidebarW - S(8);
  if (s->InfoEnabled)
    listW = SCREEN_WIDTH - sidebarW - S(160);

  // 1. Sidebar Clicking & Interaction
  for (int i = 0; i < 7; i++) {
    float boxY = TOP_BAR_H + i * sidebarW;
    Rectangle boxRect = {0, boxY, sidebarW, sidebarW};

    if (CheckCollisionPointRec(mousePos, boxRect)) {
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (i < 4) {
          s->ScrollOffset = 0;
          if (i == 3) {
            // Navigate to Drive / Source
            s->BrowseLevel = 3;
            s->CursorPos = 0;
          } else {
            // Navigate to categories (Playlist, Folder, Search)
            s->BrowseLevel = 2; // Categories level
            s->CursorPos = i;
            // Trigger "Enter" logic for the category
            if (s->CursorPos == 2) {
              s->BrowseLevel = 1;
            } // Playlists
            else if (s->CursorPos == 0) {
              s->BrowseLevel = 0; // Tracks
              s->CurrentPlaylistIdx = -1;
              Browser_UpdateActiveTracks(s);
            }
            s->CursorPos = 0;
          }
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
            printf("[BROWSER] Banked Playlist %d to Slot %d\n", s->DraggingIdx,
                   bankIdx + 1);
          }
        }
        s->IsDragging = false;
      }
    }
  }

  // Mouse Wheel Scroll
  float wheel = GetMouseWheelMove();
  if (wheel != 0 && !s->ShowLoadPopup) {
    if (wheel > 0) {
      if (s->CursorPos > 0)
        s->CursorPos--;
      else if (s->ScrollOffset > 0)
        s->ScrollOffset--;
    } else {
      int total = 0;
      if (s->BrowseLevel == 3)
        total = s->StorageCount;
      else if (s->BrowseLevel == 2)
        total = 5;
      else if (s->BrowseLevel == 1)
        total = s->DB ? s->DB->PlaylistCount : 0;
      else
        total = s->ActiveTrackCount;

      if (s->CursorPos < totalVisible - 1 &&
          s->CursorPos + s->ScrollOffset < total - 1) {
        s->CursorPos++;
      } else if (s->ScrollOffset + totalVisible < total) {
        s->ScrollOffset++;
      }
    }
  }

  // 2. List Item Interaction
  int totalItems = 0;
  if (s->IsTagList)
    totalItems = s->TagListCount;
  else {
    switch (s->BrowseLevel) {
    case 0:
      totalItems = s->ActiveTrackCount;
      break;
    case 1:
      totalItems = s->DB ? s->DB->PlaylistCount : 0;
      break;
    case 2:
      totalItems = 5;
      break;
    case 3:
      totalItems = s->StorageCount;
      break;
    }
  }

  bool triggerEnter = false;

  if (!s->ShowLoadPopup) {
    for (int i = 0; i < totalVisible; i++) {
      int idx = s->ScrollOffset + i;
      if (idx >= totalItems)
        break;

      Rectangle itemRect = {sidebarW, TOP_BAR_H + i * rowH, listW, rowH};
      if (CheckCollisionPointRec(mousePos, itemRect)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          s->TouchDragAccumulator = 0;
          if (s->BrowseLevel == 1) {
            s->IsDragging = true;
            s->DraggingIdx = idx;
            s->DraggingType = 1;
          }
          if (s->CursorPos != i) {
            s->CursorPos = i;
            s->MarqueeScrollX = 0; // Reset marquee on selection change
          }
        }

        // Check if internal "LOAD" button area was clicked (only for tracks)
        float loadBtnW = S(45);
        Rectangle loadBtnRect = {sidebarW + listW - loadBtnW - S(5),
                                 TOP_BAR_H + i * rowH + S(4), loadBtnW,
                                 rowH - S(8)};
        bool isLoadClick = (s->BrowseLevel == 0) &&
                           CheckCollisionPointRec(mousePos, loadBtnRect);

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !s->IsDragging) {
          if (fabsf(s->TouchDragAccumulator) < 10.0f) { // Not a drag
            if (isLoadClick) {
              s->ShowLoadPopup = true;
              s->PopupTrackIdx = idx;
            } else if (!s->IsTagList) {
              triggerEnter = true;
            } else {
              // Just select (already handled in Pressed)
            }
          }
        }
      }
    }

    // Drag Scrolling logic
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !s->IsDragging) {
      Vector2 delta = GetMouseDelta();
      s->TouchDragAccumulator += delta.y;

      float threshold = S(20.0f);
      if (s->TouchDragAccumulator < -threshold) { // Dragged up -> Scroll down
        if (s->CursorPos + s->ScrollOffset < totalItems - 1) {
          if (s->CursorPos < totalVisible - 1)
            s->CursorPos++;
          else
            s->ScrollOffset++;
        }
        s->TouchDragAccumulator = 0;
      } else if (s->TouchDragAccumulator >
                 threshold) { // Dragged down -> Scroll up
        if (s->CursorPos > 0)
          s->CursorPos--;
        else if (s->ScrollOffset > 0)
          s->ScrollOffset--;
        s->TouchDragAccumulator = 0;
      }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
      s->IsDragging = false;

    if (IsKeyPressed(KEY_DOWN)) {
      if (s->CursorPos + s->ScrollOffset < totalItems - 1) {
        if (s->CursorPos < totalVisible - 1)
          s->CursorPos++;
        else
          s->ScrollOffset++;
      }
    }
    if (IsKeyPressed(KEY_UP)) {
      if (s->CursorPos > 0)
        s->CursorPos--;
      else if (s->ScrollOffset > 0)
        s->ScrollOffset--;
    }
  }

  if (IsKeyPressed(KEY_ENTER) || triggerEnter) {
    if (s->BrowseLevel == 3) {
      int idx = s->ScrollOffset + s->CursorPos;
      if (idx < s->StorageCount) {
        s->SelectedStorage = &s->AvailableStorages[idx];
        if (s->DB) RB_FreeDatabase(s->DB);
        if (s->SeratoDB) Serato_FreeDatabase(s->SeratoDB);
        s->DB = NULL;
        s->SeratoDB = NULL;

        // Try Rekordbox first
        s->DB = RB_LoadDatabase(s->SelectedStorage->Path);
        if (s->DB) {
          s->DatabaseType = 0;
          if (s->TrackPointers) free(s->TrackPointers);
          s->TrackPointers = (RBTrack **)malloc(s->DB->TrackCount * sizeof(RBTrack *));
          s->BrowseLevel = 2;
          s->CursorPos = s->ScrollOffset = 0;
        } else {
          // Try Serato
          s->SeratoDB = Serato_LoadDatabase(s->SelectedStorage->Path);
          if (s->SeratoDB) {
            s->DatabaseType = 1;
            if (s->SeratoTrackPointers) free(s->SeratoTrackPointers);
            s->SeratoTrackPointers = (SeratoTrack **)malloc(s->SeratoDB->TrackCount * sizeof(SeratoTrack *));
            s->BrowseLevel = 2;
            s->CursorPos = s->ScrollOffset = 0;
          } else {
            printf("[BROWSER] Failed to load database from %s\n", s->SelectedStorage->Path);
          }
        }
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
      if (s->DatabaseType == 0) {
        if (s->DB && idx < (int)s->DB->PlaylistCount) {
          s->CurrentPlaylistIdx = idx;
          s->BrowseLevel = 0;
          Browser_UpdateActiveTracks(s);
          s->CursorPos = s->ScrollOffset = 0;
        }
      } else {
        if (s->SeratoDB && idx < (int)s->SeratoDB->PlaylistCount) {
          s->CurrentPlaylistIdx = idx;
          s->BrowseLevel = 0;
          Browser_UpdateActiveTracks(s);
          s->CursorPos = s->ScrollOffset = 0;
        }
      }
    } else if (s->BrowseLevel == 0) {
      // Track selected -> Show Load Popup
      int idx = s->ScrollOffset + s->CursorPos;
      if (idx < s->ActiveTrackCount) {
        s->ShowLoadPopup = true;
        s->PopupTrackIdx = idx;
      }
    }
  }

  if (IsKeyPressed(KEY_BACKSPACE)) {
    Browser_Back(s);
  }

  if (s->BrowseLevel == 0 && loadToDeck != -1) {
    // LOAD TRACK
    struct DeckState *targetDeck = loadToDeck == 0 ? s->DeckA : s->DeckB;
    if (targetDeck && targetDeck->Waveform.LoadLock && targetDeck->IsPlaying) {
      printf("[BROWSER] LOAD LOCKED: Deck %c is playing\n",
             loadToDeck == 0 ? 'A' : 'B');
      return 0; // Prevent load
    }

    int idx = targetIdx;
    if (s->DatabaseType == 0) { // Rekordbox
      if (idx < s->ActiveTrackCount && s->TrackPointers[idx]) {
        RBTrack *t = s->TrackPointers[idx];
        printf("[BROWSER] Loading RB track: %s to Deck %c\n", t->Title, loadToDeck == 0 ? 'A' : 'B');
        
        if (s->SelectedStorage) {
          RB_LoadTrackData(t, s->SelectedStorage->Path);
          
          if (s->AudioPlugin) {
            char fullPath[1024];
            const char *relPath = t->FilePath;
            if (relPath[0] == '/' || relPath[0] == '\\') relPath++;
            snprintf(fullPath, sizeof(fullPath), "%s/%s", s->SelectedStorage->Path, relPath);
            DeckAudio_LoadTrack(&s->AudioPlugin->Decks[loadToDeck], fullPath);
          }

          struct DeckState *targetDeck = loadToDeck == 0 ? s->DeckA : s->DeckB;
          if (targetDeck) {
            strcpy(targetDeck->TrackTitle, t->Title);
            strcpy(targetDeck->ArtistName, t->Artist);
            strcpy(targetDeck->TrackKey, t->Key);
            targetDeck->OriginalBPM = t->BPM;
            targetDeck->CurrentBPM = t->BPM;
            
            // Artwork
            if (t->ArtworkPath[0] != '\0') {
                const char *artRel = t->ArtworkPath;
                if (artRel[0] == '/' || artRel[0] == '\\') artRel++;
                snprintf(targetDeck->ArtworkPath, sizeof(targetDeck->ArtworkPath), "%s/%s", s->SelectedStorage->Path, artRel);
            } else targetDeck->ArtworkPath[0] = '\0';

            // Allocate and setup TrackState
            TrackState *newTrack = (TrackState *)malloc(sizeof(TrackState));
            if (newTrack) {
                memset(newTrack, 0, sizeof(TrackState));
                newTrack->StaticWaveformLen = t->StaticWaveformLen;
                memcpy(newTrack->StaticWaveform, t->StaticWaveform, t->StaticWaveformLen > 8192 ? 8192 : t->StaticWaveformLen);
                newTrack->DynamicWaveform = t->DynamicWaveform;
                newTrack->DynamicWaveformLen = t->DynamicWaveformLen;
                newTrack->WaveformType = t->WaveformType;
                
                // Cues and Beats
                newTrack->BeatGridCount = t->BeatGridCount > 1024 ? 1024 : t->BeatGridCount;
                for (int i = 0; i < newTrack->BeatGridCount; i++) newTrack->BeatGrid[i] = t->BeatGrid[i];
                
                for (uint32_t i = 0; i < t->CueCount && i < 32; i++) {
                    if (t->Cues[i].ID >= 1 && t->Cues[i].ID <= 8) {
                        newTrack->HotCues[newTrack->HotCuesCount].ID = t->Cues[i].ID;
                        newTrack->HotCues[newTrack->HotCuesCount].Start = t->Cues[i].Time;
                        memcpy(newTrack->HotCues[newTrack->HotCuesCount].Color, t->Cues[i].Color, 3);
                        newTrack->HotCuesCount++;
                    } else if (t->Cues[i].ID == 0) {
                        newTrack->Cues[newTrack->CuesCount].Start = t->Cues[i].Time;
                        memcpy(newTrack->Cues[newTrack->CuesCount].Color, t->Cues[i].Color, 3);
                        newTrack->CuesCount++;
                    }
                }

                TrackState *oldTrack = targetDeck->LoadedTrack;
                targetDeck->LoadedTrack = newTrack;
                if (oldTrack) free(oldTrack);
                targetDeck->PositionMs = (newTrack->CuesCount > 0) ? newTrack->Cues[0].Start : (newTrack->BeatGridCount > 0 ? newTrack->BeatGrid[0].Time : 0);
                DeckAudio_JumpToMs(&s->AudioPlugin->Decks[loadToDeck], (uint32_t)targetDeck->PositionMs);
            }
          }
        }
      }
    } else { // Serato
      if (idx < s->ActiveTrackCount && s->SeratoTrackPointers[idx]) {
        SeratoTrack *t = s->SeratoTrackPointers[idx];
        printf("[BROWSER] Loading Serato track: %s to Deck %c\n", t->Title, loadToDeck == 0 ? 'A' : 'B');
        
        if (s->SelectedStorage) {
          if (s->AudioPlugin) {
            char fullPath[1024];
            const char *relPath = t->FilePath;
            // Serato locations can be absolute or relative. Let's assume relative to root if it starts with /
            if (relPath[0] == '/' || relPath[0] == '\\') relPath++;
            snprintf(fullPath, sizeof(fullPath), "%s/%s", s->SelectedStorage->Path, relPath);
            DeckAudio_LoadTrack(&s->AudioPlugin->Decks[loadToDeck], fullPath);
          }

          struct DeckState *targetDeck = loadToDeck == 0 ? s->DeckA : s->DeckB;
          if (targetDeck) {
            strcpy(targetDeck->TrackTitle, t->Title);
            strcpy(targetDeck->ArtistName, t->Artist);
            strcpy(targetDeck->TrackKey, t->Key);
            targetDeck->OriginalBPM = t->BPM;
            targetDeck->CurrentBPM = t->BPM;
            targetDeck->ArtworkPath[0] = '\0'; // Serato artwork not implemented yet

            TrackState *newTrack = (TrackState *)malloc(sizeof(TrackState));
            if (newTrack) {
                memset(newTrack, 0, sizeof(TrackState));
                // Basic Serato TrackState setup (no waveforms/cues yet)
                TrackState *oldTrack = targetDeck->LoadedTrack;
                targetDeck->LoadedTrack = newTrack;
                if (oldTrack) free(oldTrack);
                targetDeck->PositionMs = 0;
                DeckAudio_JumpToMs(&s->AudioPlugin->Decks[loadToDeck], 0);
            }
          }
        }
      }
    }
  }

  if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) &&
      !s->ShowLoadPopup) {
    if (s->BrowseLevel == 0) {
      if (s->CurrentPlaylistIdx >= 0)
        s->BrowseLevel = 1;
      else
        s->BrowseLevel = 2;
    } else if (s->BrowseLevel == 1)
      s->BrowseLevel = 2;
    else if (s->BrowseLevel == 2)
      s->BrowseLevel = 3;
    s->CursorPos = s->ScrollOffset = 0;
  }

  return 0;
}

static void Browser_Draw(Component *base) {
  BrowserRenderer *r = (BrowserRenderer *)base;
  BrowserState *s = r->State;

  if (!s->IsActive)
    return;

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
    bool isAssigned = isBank && (s->PlaylistBankIdx[i - 4] >= 0);

    // Background
    Color bg = isHovered ? ColorBlue
                         : (isBank ? (isAssigned ? ColorDGreen : ColorDark3)
                                   : ColorDark2);
    DrawRectangle(0, boxY, sidebarW, sidebarW, bg);

    // Inner separation lines
    DrawRectangleLinesEx(boxRect, 1.0f, ColorDark1);

    if (!isBank) {
      // Icon / Symbol for main navigation
      const char *sidIcons[] = {
          "\uf03c", "\uf07b", "\uf002",
          "\uf287"}; // Playlist, Folder, Search, Drive (USB)
      UIDrawText(sidIcons[i], faceIcon, S(12), boxY + S(12), S(16), ColorWhite);
    } else {
      // Playlist Bank Placeholders (1-3)
      char bankNum[4];
      sprintf(bankNum, "P%d", i - 3);
      // Draw a bookmark icon as a background for the bank spot
      UIDrawText("\uf02e", faceIcon, S(13), boxY + S(8), S(14),
                 isAssigned ? ColorWhite : ColorShadow);
      UIDrawText(bankNum, faceXS, S(14), boxY + S(24), S(10), ColorWhite);
    }
  }

  // Header Color & Text
  Color headerClr = ColorBlue;
  const char *titleText = "TRACKS";
  char countText[32] = "";

  if (s->BrowseLevel == 1) {
    headerClr = ColorDGreen;
    titleText = "PLAYLIST";
    int totalPl = 0;
    if (s->DatabaseType == 0) totalPl = s->DB ? s->DB->PlaylistCount : 0;
    else totalPl = s->SeratoDB ? s->SeratoDB->PlaylistCount : 0;
    sprintf(countText, "TOTAL %d", totalPl);
  } else if (s->BrowseLevel == 2) {
    headerClr = ColorOrange;
    titleText = "BROWSE";
  } else if (s->BrowseLevel == 3) {
    Browser_RefreshStorages(s); // Refresh on Source screen
    headerClr = ColorBlue;
    titleText = "SOURCE";
    sprintf(countText, "TOTAL %d", s->StorageCount);
  } else {
    // Track level (Playlist or Global)
    int total = s->ActiveTrackCount;
    sprintf(countText, "TOTAL %d", total);
  }

  UIDrawText(titleText, faceSm, sidebarW + S(8), TOP_BAR_H - S(14), S(13),
             headerClr);
  if (countText[0]) {
    UIDrawText(countText, faceXS, SCREEN_WIDTH - S(80), TOP_BAR_H - S(14),
               S(10), headerClr);
  }

  float rowH = S(28.0f);
  int totalVisible = 10;
  float listX = sidebarW;
  float listW = SCREEN_WIDTH - sidebarW - S(8);
  if (s->InfoEnabled)
    listW = SCREEN_WIDTH - sidebarW - S(160);

  for (int i = 0; i < totalVisible; i++) {
    int idx = s->ScrollOffset + i;
    const char *title = "";
    const char *artist = "";
    const char *bpmText = "124.0";
    const char *keyStr = "12A";
    bool isPlaying = false;
    (void)isPlaying;

    switch (s->BrowseLevel) {
    case 0:
      if (s->DatabaseType == 0) {
        if (idx < s->ActiveTrackCount && s->TrackPointers[idx]) {
          RBTrack *t = s->TrackPointers[idx];
          title = t->Title;
          artist = t->Artist;
          static char bpmBuf[16];
          sprintf(bpmBuf, "%.1f", t->BPM);
          bpmText = bpmBuf;
          keyStr = t->Key;
        }
      } else {
        if (idx < s->ActiveTrackCount && s->SeratoTrackPointers[idx]) {
          SeratoTrack *t = s->SeratoTrackPointers[idx];
          title = t->Title;
          artist = t->Artist;
          static char bpmBuf[16];
          sprintf(bpmBuf, "%.1f", t->BPM);
          bpmText = bpmBuf;
          keyStr = t->Key;
        }
      }
      break;
    case 1:
      if (s->DatabaseType == 0) {
        if (s->DB && idx >= 0 && (uint32_t)idx < s->DB->PlaylistCount)
          title = s->DB->Playlists[idx].Name;
      } else {
        if (s->SeratoDB && idx >= 0 && (uint32_t)idx < s->SeratoDB->PlaylistCount)
          title = s->SeratoDB->Playlists[idx].Name;
      }
      break;
    case 2:
      if (idx < 5)
        title = categories[idx];
      break;
    case 3:
      if (idx < s->StorageCount)
        title = s->AvailableStorages[idx].Name;
      break;
    }

    if (title[0] == '\0')
      continue;

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
    if (s->BrowseLevel == 3)
      textX = listX + S(38);
    else if (s->BrowseLevel > 0)
      textX = listX + S(20);

    float textY = ry + (artist[0] == '\0' ? S(6) : S(2));

    // Marquee Logic for Title
    float maxTitleW = listW - (textX - listX) - S(90); // Space for BPM/Key/LOAD
    Vector2 fullSize = MeasureTextEx(faceSm, title, S(13), 1.0f);

    if (isCursor && fullSize.x > maxTitleW) {
      // Animation
      double now = GetTime();
      if (s->LastAnimTime == 0)
        s->LastAnimTime = now;
      float dt = (float)(now - s->LastAnimTime);
      s->LastAnimTime = now;

      s->MarqueeScrollX += dt * S(40.0f); // 40px per second
      if (s->MarqueeScrollX > fullSize.x + S(40.0f))
        s->MarqueeScrollX = -S(20.0f); // Loop with gap

      BeginScissorMode(textX, ry, maxTitleW, rowH);
      UIDrawText(title, faceSm, textX - s->MarqueeScrollX, textY, S(13),
                 ColorWhite);
      EndScissorMode();
    } else {
      // Normal truncated display
      BeginScissorMode(textX, ry, maxTitleW, rowH);
      UIDrawText(title, faceSm, textX, textY, S(13), ColorWhite);
      EndScissorMode();
    }

    if (artist[0] != '\0' && s->BrowseLevel == 0 && !s->InfoEnabled) {
      UIDrawText(artist, faceXS, textX, ry + S(15), S(10),
                 isCursor ? ColorWhite : ColorShadow);
    }

    // BPM & Key & LOAD Button
    if (s->BrowseLevel == 0 && !s->InfoEnabled) {
      UIDrawText(bpmText, faceXS, listX + listW - S(125), ry + S(4), S(10),
                 isCursor ? ColorWhite : ColorShadow);
      UIDrawText(keyStr, faceXS, listX + listW - S(85), ry + S(4), S(10),
                 isCursor ? ColorWhite : ColorShadow);

      // LOAD Button
      float loadW = S(45);
      Rectangle loadRect = {listX + listW - loadW - S(5), ry + S(4), loadW,
                            rowH - S(8)};
      bool hoverLoad = CheckCollisionPointRec(mPos, loadRect);

      DrawRectangleRec(loadRect, hoverLoad ? ColorBlue : ColorDark3);
      DrawRectangleLinesEx(loadRect, 1.0f, ColorShadow);
      DrawCentredText("LOAD", faceXS, loadRect.x, loadRect.width,
                      loadRect.y + S(2), S(10), ColorWhite);
    }

    // Storage icons
    if (s->BrowseLevel == 3 && idx < s->StorageCount) {
      const char *icon = "\uf287"; // uf287 usb (UTF-8)
      if (strcmp(s->AvailableStorages[idx].Type, "SD") == 0)
        icon = "\uf7c2"; // uf7c2 sd-card
      UIDrawText(icon, faceBrand, listX + S(11), ry + S(7), S(12), ColorWhite);
    }
  }

  // Scrollbar
  int maxItems = 0;
  if (s->BrowseLevel == 0)
    maxItems = s->ActiveTrackCount;
  else if (s->BrowseLevel == 1) {
    if (s->DatabaseType == 0) maxItems = s->DB ? (int)s->DB->PlaylistCount : 0;
    else maxItems = s->SeratoDB ? (int)s->SeratoDB->PlaylistCount : 0;
  } else if (s->BrowseLevel == 2)
    maxItems = 5;
  else if (s->BrowseLevel == 3)
    maxItems = s->StorageCount;

  DrawScrollbar(SCREEN_WIDTH - S(4), TOP_BAR_H, S(2), viewH - TOP_BAR_H,
                maxItems, s->ScrollOffset, totalVisible);

  // Drag and Drop Preview
  if (s->IsDragging) {
    const char *dragTitle = "Playlist Item";
    if (s->DatabaseType == 0) {
        if (s->DraggingType == 1 && s->DB && s->DraggingIdx >= 0 && (uint32_t)s->DraggingIdx < s->DB->PlaylistCount) {
            dragTitle = s->DB->Playlists[s->DraggingIdx].Name;
        }
    } else {
        if (s->DraggingType == 1 && s->SeratoDB && s->DraggingIdx >= 0 && (uint32_t)s->DraggingIdx < s->SeratoDB->PlaylistCount) {
            dragTitle = s->SeratoDB->Playlists[s->DraggingIdx].Name;
        }
    }
    DrawRectangle(mPos.x, mPos.y, S(120), rowH, Fade(ColorBlue, 0.6f));
    UIDrawText(dragTitle, faceSm, mPos.x + S(8), mPos.y + S(6), S(13),
               ColorWhite);
  }

  // Load Deck Popup Modal
  if (s->ShowLoadPopup) {
    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    DrawRectangle(0, 0, SCREEN_WIDTH, viewH, Fade(ColorBlack, 0.8f));
    float pw = S(240);
    float ph = S(120);
    float px = (SCREEN_WIDTH - pw) / 2.0f;
    float py = (viewH - ph) / 2.0f;

    // Background
    DrawRectangle(px, py, pw, ph, ColorBGUtil);
    DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 1.0f, ColorGray);

    // Title
    DrawCentredText("LOAD TRACK TO...", faceMd, px, pw, py + S(15), S(15),
                    ColorWhite);
    const char *trackName = "Unknown";
    if (s->PopupTrackIdx >= 0 && s->PopupTrackIdx < s->ActiveTrackCount) {
        if (s->DatabaseType == 0) {
            if (s->TrackPointers[s->PopupTrackIdx]) trackName = s->TrackPointers[s->PopupTrackIdx]->Title;
        } else {
            if (s->SeratoTrackPointers[s->PopupTrackIdx]) trackName = s->SeratoTrackPointers[s->PopupTrackIdx]->Title;
        }
    }
    DrawCentredText(trackName, faceXS, px, pw, py + S(35), S(10), ColorShadow);

    // Buttons Deck A & Deck B
    Rectangle deckARect = {px, py + ph / 2.0f, pw / 2.0f, ph / 2.0f};
    Rectangle deckBRect = {px + pw / 2.0f, py + ph / 2.0f, pw / 2.0f,
                           ph / 2.0f};

    bool hoverA = CheckCollisionPointRec(mPos, deckARect);
    bool hoverB = CheckCollisionPointRec(mPos, deckBRect);

    DrawRectangleRec(deckARect, hoverA ? ColorOrange : ColorDark1);
    DrawRectangleLinesEx(deckARect, 1.0f, ColorShadow);
    DrawCentredText("DECK 1", faceMd, px, pw / 2.0f, py + ph / 2.0f + S(20),
                    S(15), hoverA ? ColorBlack : ColorOrange);

    DrawRectangleRec(deckBRect, hoverB ? ColorBlue : ColorDark2);
    DrawRectangleLinesEx(deckBRect, 1.0f, ColorShadow);
    DrawCentredText("DECK 2", faceMd, px + pw / 2.0f, pw / 2.0f,
                    py + ph / 2.0f + S(20), S(15),
                    hoverB ? ColorBlack : ColorBlue);
  }
}

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state) {
  r->base.Update = Browser_Update;
  r->base.Draw = Browser_Draw;
  r->State = state;
}
