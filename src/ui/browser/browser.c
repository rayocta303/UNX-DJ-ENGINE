#include "ui/browser/browser.h"
#include "audio/engine.h"
#include "rlgl.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/player_state.h"
#include "core/system_info.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>

// Case-insensitive substring search helper
static const char* stristr_local(const char* haystack, const char* needle) {
  if (!*needle) return haystack;
  for (; *haystack; ++haystack) {
    if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
      const char* h = haystack;
      const char* n = needle;
      while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
        ++h;
        ++n;
      }
      if (!*n) return haystack;
    }
  }
  return NULL;
}

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

static void Browser_SwitchStorageByPath(BrowserState *s, const char *path) {
  // Find storage in AvailableStorages
  int foundIdx = -1;
  for (int j = 0; j < s->StorageCount; j++) {
    if (strcmp(s->AvailableStorages[j].Path, path) == 0) {
      foundIdx = j;
      break;
    }
  }

  if (foundIdx != -1) {
    s->SelectedStorage = &s->AvailableStorages[foundIdx];
    if (s->DB)
      RB_FreeDatabase(s->DB);
    if (s->SeratoDB)
      Serato_FreeDatabase(s->SeratoDB);
    s->DB = NULL;
    s->SeratoDB = NULL;

    s->DB = RB_LoadDatabase(s->SelectedStorage->Path);
    s->SeratoDB = Serato_LoadDatabase(s->SelectedStorage->Path);
    s->HasBothDatabases = (s->DB != NULL && s->SeratoDB != NULL);

    if (s->DB) {
      s->DatabaseType = 0;
      if (s->TrackPointers)
        free(s->TrackPointers);
      s->TrackPointers = (RBTrack **)malloc(s->DB->TrackCount * sizeof(RBTrack *));
    }
    if (s->SeratoDB) {
      if (s->SeratoTrackPointers)
        free(s->SeratoTrackPointers);
      s->SeratoTrackPointers = (SeratoTrack **)malloc(s->SeratoDB->TrackCount * sizeof(SeratoTrack *));
      if (!s->DB)
        s->DatabaseType = 1;
    }
  }
}

// Sorter Helpers
static int CompareTracks_BPM_RB(const void *a, const void *b) {
    RBTrack *ta = *(RBTrack **)a;
    RBTrack *tb = *(RBTrack **)b;
    if (!ta || !tb) return 0;
    if (ta->BPM < tb->BPM) return -1;
    if (ta->BPM > tb->BPM) return 1;
    return 0;
}

static int CompareTracks_Key_RB(const void *a, const void *b) {
    RBTrack *ta = *(RBTrack **)a;
    RBTrack *tb = *(RBTrack **)b;
    if (!ta || !tb) return 0;
    return strcmp(ta->Key, tb->Key);
}

static int CompareTracks_BPM_Serato(const void *a, const void *b) {
    SeratoTrack *ta = *(SeratoTrack **)a;
    SeratoTrack *tb = *(SeratoTrack **)b;
    if (!ta || !tb) return 0;
    if (ta->BPM < tb->BPM) return -1;
    if (ta->BPM > tb->BPM) return 1;
    return 0;
}

static int CompareTracks_Key_Serato(const void *a, const void *b) {
    SeratoTrack *ta = *(SeratoTrack **)a;
    SeratoTrack *tb = *(SeratoTrack **)b;
    if (!ta || !tb) return 0;
    return strcmp(ta->Key, tb->Key);
}
static int CompareTracks_Title_RB(const void *a, const void *b) {
    RBTrack *ta = *(RBTrack **)a;
    RBTrack *tb = *(RBTrack **)b;
    if (!ta || !tb) return 0;
    return strcmp(ta->Title, tb->Title);
}

static int CompareTracks_Rating_RB(const void *a, const void *b) {
    RBTrack *ta = *(RBTrack **)a;
    RBTrack *tb = *(RBTrack **)b;
    if (!ta || !tb) return 0;
    // Sort ratings descending (5 stars first)
    if (ta->Rating > tb->Rating) return -1;
    if (ta->Rating < tb->Rating) return 1;
    return 0;
}

static int CompareTracks_Title_Serato(const void *a, const void *b) {
    SeratoTrack *ta = *(SeratoTrack **)a;
    SeratoTrack *tb = *(SeratoTrack **)b;
    if (!ta || !tb) return 0;
    return strcmp(ta->Title, tb->Title);
}

static int CompareTracks_Rating_Serato(const void *a, const void *b) {
    // SeratoTrack struct currently lacks a Rating field in the DB v2 parser.
    // Retain original order.
    return 0;
}

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

  // Apply search filter
  if (s->IsSearching && s->SearchQuery[0] != '\0') {
    int filteredCount = 0;
    if (s->DatabaseType == 0 && s->TrackPointers) { // Rekordbox
      for (int i = 0; i < s->ActiveTrackCount; i++) {
        RBTrack *t = s->TrackPointers[i];
        if (t) {
          if (stristr_local(t->Title, s->SearchQuery) || stristr_local(t->Artist, s->SearchQuery)) {
            s->TrackPointers[filteredCount++] = t;
          }
        }
      }
    } else if (s->DatabaseType == 1 && s->SeratoTrackPointers) { // Serato
      for (int i = 0; i < s->ActiveTrackCount; i++) {
        SeratoTrack *t = s->SeratoTrackPointers[i];
        if (t) {
          if (stristr_local(t->Title, s->SearchQuery) || stristr_local(t->Artist, s->SearchQuery)) {
            s->SeratoTrackPointers[filteredCount++] = t;
          }
        }
      }
    }
    s->ActiveTrackCount = filteredCount;
  }
  // Apply Sort
  if (s->SortMode > 0 && s->ActiveTrackCount > 0) {
      if (s->DatabaseType == 0 && s->TrackPointers) { // Rekordbox
          if (s->SortMode == 1) qsort(s->TrackPointers, s->ActiveTrackCount, sizeof(RBTrack *), CompareTracks_BPM_RB);
          else if (s->SortMode == 2) qsort(s->TrackPointers, s->ActiveTrackCount, sizeof(RBTrack *), CompareTracks_Key_RB);
          else if (s->SortMode == 3) qsort(s->TrackPointers, s->ActiveTrackCount, sizeof(RBTrack *), CompareTracks_Title_RB);
          else if (s->SortMode == 4) qsort(s->TrackPointers, s->ActiveTrackCount, sizeof(RBTrack *), CompareTracks_Rating_RB);
      } else if (s->DatabaseType == 1 && s->SeratoTrackPointers) { // Serato
          if (s->SortMode == 1) qsort(s->SeratoTrackPointers, s->ActiveTrackCount, sizeof(SeratoTrack *), CompareTracks_BPM_Serato);
          else if (s->SortMode == 2) qsort(s->SeratoTrackPointers, s->ActiveTrackCount, sizeof(SeratoTrack *), CompareTracks_Key_Serato);
          else if (s->SortMode == 3) qsort(s->SeratoTrackPointers, s->ActiveTrackCount, sizeof(SeratoTrack *), CompareTracks_Title_Serato);
          else if (s->SortMode == 4) qsort(s->SeratoTrackPointers, s->ActiveTrackCount, sizeof(SeratoTrack *), CompareTracks_Rating_Serato);
      }
  }
}

void Browser_Back(BrowserState *s) {
  if (s->IsSearching) {
      s->IsSearching = false;
      s->SearchQuery[0] = '\0';
      Browser_UpdateActiveTracks(s);
  } else if (s->IsTagList) {
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
  const char *testPath = "usb_test";
#ifdef __ANDROID__
  // On Android, check the root of internal storage for usb_test folder
  if (stat("/storage/emulated/0/usb_test", &st) == 0) {
    testPath = "/storage/emulated/0/usb_test";
    printf("[BROWSER] Found Android usb_test at %s\n", testPath);
  }
#endif

#if defined(PLATFORM_IOS)
  // On iOS, we use the Documents container as a primary storage
  extern const char *ios_get_documents_path(const char *filename);
  const char *docPath = ios_get_documents_path("");
  if (docPath && docPath[0] != '\0') {
    strcpy(s->AvailableStorages[s->StorageCount].Name, "App Container");
    strcpy(s->AvailableStorages[s->StorageCount].Path, docPath);
    strcpy(s->AvailableStorages[s->StorageCount].Type, "Internal");

    // Check if there's a Rekordbox DB in the container root
    char dbCheck[512];
    const char *sep = "";
    size_t pLen = strlen(docPath);
    if (pLen > 0 && docPath[pLen-1] != '/' && docPath[pLen-1] != '\\') sep = "/";

    snprintf(dbCheck, sizeof(dbCheck), "%s%sPIONEER/rekordbox/export.pdb", docPath, sep);
    if (stat(dbCheck, &st) == 0) {
      strcpy(s->AvailableStorages[s->StorageCount].Type, "Rekordbox");
    } else {
      snprintf(dbCheck, sizeof(dbCheck), "%s%s_Serato_/database V2", docPath, sep);
      if (stat(dbCheck, &st) == 0) {
        strcpy(s->AvailableStorages[s->StorageCount].Type, "Serato");
      }
    }
    s->StorageCount++;
  }
#endif

  char dbCheck[512];
  snprintf(dbCheck, sizeof(dbCheck), "%s/PIONEER/rekordbox/export.pdb",
           testPath);
  if (stat(dbCheck, &st) == 0) {
    strcpy(s->AvailableStorages[s->StorageCount].Name, "USB Test (RB)");
    strcpy(s->AvailableStorages[s->StorageCount].Path, testPath);
    strcpy(s->AvailableStorages[s->StorageCount].Type, "Rekordbox");
    s->StorageCount++;
  } else {
    snprintf(dbCheck, sizeof(dbCheck), "%s/_Serato_/database V2", testPath);
    if (stat(dbCheck, &st) == 0) {
      strcpy(s->AvailableStorages[s->StorageCount].Name, "USB Test (Serato)");
      strcpy(s->AvailableStorages[s->StorageCount].Path, testPath);
      strcpy(s->AvailableStorages[s->StorageCount].Type, "Serato");
      s->StorageCount++;
    }
  }

#ifdef _WIN32
  // 2. Scan drive letters D..Z
  for (char drive = 'D'; drive <= 'Z'; drive++) {
    char path[32];
    sprintf(path, "%c:/PIONEER/rekordbox/export.pdb", drive);
    if (stat(path, &st) == 0) {
      sprintf(s->AvailableStorages[s->StorageCount].Name, "USB (%c:) RB",
              drive);
      sprintf(s->AvailableStorages[s->StorageCount].Path, "%c:/", drive);
      strcpy(s->AvailableStorages[s->StorageCount].Type, "Rekordbox");
      s->StorageCount++;
      if (s->StorageCount >= 16)
        break;
    }
    sprintf(path, "%c:/_Serato_/database V2", drive);
    if (stat(path, &st) == 0) {
      sprintf(s->AvailableStorages[s->StorageCount].Name, "USB (%c:) Serato",
              drive);
      sprintf(s->AvailableStorages[s->StorageCount].Path, "%c:/", drive);
      strcpy(s->AvailableStorages[s->StorageCount].Type, "Serato");
      s->StorageCount++;
      if (s->StorageCount >= 16)
        break;
    }
  }
#else
#ifdef __ANDROID__
  // 2. Scan Android / Linux /storage directory for OTG / SD Cards
  if (s->StorageCount < 16) {
    if (stat("/storage/emulated/0", &st) == 0) {
      strcpy(s->AvailableStorages[s->StorageCount].Name, "Internal Storage");
      strcpy(s->AvailableStorages[s->StorageCount].Path, "/storage/emulated/0");
      strcpy(s->AvailableStorages[s->StorageCount].Type, "Internal");
      s->StorageCount++;
      printf("[BROWSER] Added Internal Storage: /storage/emulated/0\n");
    } else {
      printf("[BROWSER] Internal Storage /storage/emulated/0 NOT FOUND\n");
    }
  }
#else
  if (s->StorageCount < 8) {
    strcpy(s->AvailableStorages[s->StorageCount].Name, "Internal Storage");
    strcpy(s->AvailableStorages[s->StorageCount].Path, "/storage/emulated/0");
    strcpy(s->AvailableStorages[s->StorageCount].Type, "Internal");
    s->StorageCount++;
  }
#endif

#ifdef PLATFORM_IOS
  extern const char *ios_get_documents_path(const char *filename);
  const char *scanDirs[] = {"DOCUMENTS_DIR", "/var/mobile/Media",
                            "/storage",      "/mnt",
                            "/media",        "/run/media"};
  int scanDirCount = 6;
#else
  const char *scanDirs[] = {"/storage", "/mnt", "/media", "/run/media"};
  int scanDirCount = 4;
#endif

  for (int i = 0; i < scanDirCount; i++) {
    const char *dirToScan = scanDirs[i];

#ifdef PLATFORM_IOS
    if (strcmp(dirToScan, "DOCUMENTS_DIR") == 0) {
      dirToScan = ios_get_documents_path("");
    }
#endif

    if (!dirToScan || dirToScan[0] == '\0')
      continue;

    DIR *d = opendir(dirToScan);
    if (d) {
      struct dirent *dir;
      while ((dir = readdir(d)) != NULL) {
        if (s->StorageCount >= 16)
          break;

        if (dir->d_name[0] == '.')
          continue;

        // Skip system folders and internal mount points
        bool skip = false;
        const char* toSkip[] = {"self", "emulated", "knox-emulated", "container", "secure", "asec", "obb", "runtime", "appfuse", "shared", "user", "media_rw", "temp", "expand", "legacy"};
        for(int k=0; k<15; k++) {
            if(strcmp(dir->d_name, toSkip[k]) == 0) { skip = true; break; }
        }
        if (skip) continue;


        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirToScan, dir->d_name);

        printf("[BROWSER] Scanning potential storage: %s\n", fullPath);


        struct stat st_dir;
        if (stat(fullPath, &st_dir) == 0 && S_ISDIR(st_dir.st_mode) &&
            access(fullPath, R_OK) == 0) {
          bool exists = false;
          for (int j = 0; j < s->StorageCount; j++) {
            if (strcmp(s->AvailableStorages[j].Path, fullPath) == 0) {
              exists = true;
              break;
            }
          }
          if (exists)
            continue;

          // Check for DB type
          char dbPath[1024];
          bool hasRB = false;
          bool hasSerato = false;

          snprintf(dbPath, sizeof(dbPath), "%s/PIONEER/rekordbox/export.pdb",
                   fullPath);
          if (stat(dbPath, &st) == 0)
            hasRB = true;

          snprintf(dbPath, sizeof(dbPath), "%s/_Serato_/database V2", fullPath);
          if (stat(dbPath, &st) == 0)
            hasSerato = true;

          const char *type = "USB";
          if (hasRB && hasSerato)
            type = "RB/Serato";
          else if (hasRB)
            type = "Rekordbox";
          else if (hasSerato)
            type = "Serato";
          else if (strchr(dir->d_name, '-') != NULL)
            type = "SD";

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
  bool triggerEnter = false;

  // Dropdown and Search Box Interaction
  Vector2 mousePos = UIGetMousePosition();
  if (s->BrowseLevel == 0) {
    float sidebarW = S(40);
    float listW = SCREEN_WIDTH - sidebarW - S(8);
    if (s->InfoEnabled) listW = SCREEN_WIDTH - sidebarW - S(160);
    
    float sortButtonW = S(80);
    Rectangle sortButtonRect = {sidebarW + listW - sortButtonW, TOP_BAR_H, sortButtonW, S(28.0f)};
    Rectangle dropdownRect = {sortButtonRect.x, sortButtonRect.y + sortButtonRect.height, sortButtonW, S(28.0f) * 5}; // 5 items
    Rectangle searchBoxRect = {sidebarW, TOP_BAR_H, listW - sortButtonW - S(4), S(28.0f)};

    if (s->ShowSortDropdown) {
        bool hoverDropdown = CheckCollisionPointRec(mousePos, dropdownRect);
        bool hoverButton = CheckCollisionPointRec(mousePos, sortButtonRect);

        // 1. Handle Selection on Release
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            if (hoverDropdown) {
                int clickedIdx = (mousePos.y - dropdownRect.y) / S(28.0f);
                if (clickedIdx >= 0 && clickedIdx < 5) {
                    s->SortMode = clickedIdx;
                    s->CursorPos = s->ScrollOffset = 0;
                    Browser_UpdateActiveTracks(s);
                }
                s->ShowSortDropdown = false;
            } else if (!hoverButton) {
                // If they drag out of the menu and release, close it safely
                s->ShowSortDropdown = false;
            }
            
            // Absorb the release event so the UI underneath doesn't react
            return 0; 
        }
        
        // 2. Handle Clicks (Press) to close the menu
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (!hoverDropdown) {
                // Clicked outside or on the button itself -> close menu
                s->ShowSortDropdown = false; 
                return 0; // Absorb click so it doesn't interact with tracks underneath
            }
            return 0; // Pressed inside the dropdown, absorb it
        }
        
        // Block hovers on underlying elements if the mouse is over the menu
        if (hoverDropdown || hoverButton) return 0;
    } 
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Handle Sort Button (Open Menu)
        if (CheckCollisionPointRec(mousePos, sortButtonRect)) {
            s->ShowSortDropdown = true;
            return 0; // Absorb click
        }

        // Handle Search Box
        if (CheckCollisionPointRec(mousePos, searchBoxRect)) {
            if (!s->IsSearching) {
                s->IsSearching = true;
                System_ShowKeyboard(true);
            }
        } else {
            // Click outside search box
            if (s->IsSearching) {
                s->IsSearching = false;
                System_ShowKeyboard(false);
            }
        }
    }
  }

  // Keyboard Typing Interaction
  if (s->IsSearching) {
    bool queryChanged = false;
    int key = GetCharPressed();
    while (key > 0) {
      if ((key >= 32) && (key <= 125) && (strlen(s->SearchQuery) < 63)) {
        int len = strlen(s->SearchQuery);
        s->SearchQuery[len] = (char)key;
        s->SearchQuery[len+1] = '\0';
        queryChanged = true;
      }
      key = GetCharPressed();
    }
    
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
      int len = strlen(s->SearchQuery);
      if (len > 0) {
        s->SearchQuery[len-1] = '\0';
        queryChanged = true;
      }
    }
    
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
      s->IsSearching = false;
      System_ShowKeyboard(false);
    }
    
    if (queryChanged) {
      s->CursorPos = s->ScrollOffset = 0;
      Browser_UpdateActiveTracks(s);
    }
  }

  // MIDI Navigation
  if (s->MidiBrowseDelta != 0) {
      if (s->MidiBrowseDelta > 0) {
          for (int i = 0; i < s->MidiBrowseDelta; i++) {
              if (s->CursorPos < 9) s->CursorPos++;
              else s->ScrollOffset++;
          }
      } else {
          for (int i = 0; i < -s->MidiBrowseDelta; i++) {
              if (s->CursorPos > 0) s->CursorPos--;
              else if (s->ScrollOffset > 0) s->ScrollOffset--;
          }
      }
      s->MidiBrowseDelta = 0;
  }
  
  if (s->MidiRequestEnter) {
      triggerEnter = true;
      s->MidiRequestEnter = false;
  }
  
  if (s->MidiRequestBack) {
      Browser_Back(s);
      s->MidiRequestBack = false;
  }

  if (s->MidiRequestLoadA) {
      loadToDeck = 0;
      s->MidiRequestLoadA = false;
  }
  if (s->MidiRequestLoadB) {
      loadToDeck = 1;
      s->MidiRequestLoadB = false;
  }

  int targetIdx = s->ScrollOffset + s->CursorPos;

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
    if (!s->IsSearching && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))) {
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

  float sidebarW = S(40);
  float rowH = S(28.0f);
  int totalVisible = 10;
  float listYOffset = TOP_BAR_H;
  if (s->BrowseLevel == 0) {
      totalVisible = 9;
      listYOffset += rowH;
  }
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
          if (s->PlaylistBank[bankIdx].PlaylistIdx >= 0) {
            // Check if we need to switch storage
            if (s->SelectedStorage && strcmp(s->PlaylistBank[bankIdx].StoragePath, s->SelectedStorage->Path) != 0) {
              Browser_SwitchStorageByPath(s, s->PlaylistBank[bankIdx].StoragePath);
            }
            
            s->CurrentPlaylistIdx = s->PlaylistBank[bankIdx].PlaylistIdx;
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
            s->PlaylistBank[bankIdx].PlaylistIdx = s->DraggingIdx;
            if (s->SelectedStorage) {
              strncpy(s->PlaylistBank[bankIdx].StoragePath, s->SelectedStorage->Path, 511);
            }
            // Cache the name
            if (s->DatabaseType == 0 && s->DB && s->DraggingIdx < (int)s->DB->PlaylistCount) {
              strncpy(s->PlaylistBank[bankIdx].Name, s->DB->Playlists[s->DraggingIdx].Name, 63);
            } else if (s->DatabaseType == 1 && s->SeratoDB && s->DraggingIdx < (int)s->SeratoDB->PlaylistCount) {
              strncpy(s->PlaylistBank[bankIdx].Name, s->SeratoDB->Playlists[s->DraggingIdx].Name, 63);
            }

            printf("[BROWSER] Banked Playlist '%s' to Slot %d\n", s->PlaylistBank[bankIdx].Name, bankIdx + 1);
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
      totalItems = 5 + (s->HasBothDatabases ? 1 : 0);
      break;
    case 3:
      totalItems = s->StorageCount;
      break;
    }
  }

  if (!s->ShowLoadPopup) {
    for (int i = 0; i < totalVisible; i++) {
      int idx = s->ScrollOffset + i;
      if (idx >= totalItems)
        break;

      Rectangle itemRect = {sidebarW, listYOffset + i * rowH, listW, rowH};
      if (CheckCollisionPointRec(mousePos, itemRect)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          s->TouchDragAccumulator = 0;
          s->IsDragging = false; // Start as potential drag
          s->DraggingIdx = idx;
          if (s->BrowseLevel == 1)
            s->DraggingType = 1; // Playlist
          else if (s->BrowseLevel == 0)
            s->DraggingType = 0; // Track
          else
            s->DraggingType = -1; // Other
          if (s->CursorPos != i) {
            s->CursorPos = i;
            s->MarqueeScrollX = 0; // Reset marquee on selection change
          }
        }

        // Check if internal "LOAD" button area was clicked (only for tracks)
        float loadBtnW = S(45);
        Rectangle loadBtnRect = {sidebarW + listW - loadBtnW - S(5),
                                 listYOffset + i * rowH + S(4), loadBtnW,
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

    // Drag logic for Playlist Banking (Horizontal/Significant move) or
    // Scrolling
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      Vector2 delta = GetMouseDelta();
      s->TouchDragAccumulator += fabsf(delta.y) + fabsf(delta.x);

      // If we are on Level 1 (Playlists) and move enough, trigger actual Drag
      // state
      if (s->DraggingType == 1 && !s->IsDragging &&
          s->TouchDragAccumulator > S(15.0f)) {
        s->IsDragging = true;
      }

      if (!s->IsDragging) {
        // List Scrolling logic (only if not dragging to bank)
        static float scrollAccum = 0;
        scrollAccum += delta.y;
        float threshold = S(20.0f);
        if (scrollAccum < -threshold) {
          if (s->CursorPos + s->ScrollOffset < totalItems - 1) {
            if (s->CursorPos < totalVisible - 1)
              s->CursorPos++;
            else
              s->ScrollOffset++;
          }
          scrollAccum = 0;
        } else if (scrollAccum > threshold) {
          if (s->CursorPos > 0)
            s->CursorPos--;
          else if (s->ScrollOffset > 0)
            s->ScrollOffset--;
          scrollAccum = 0;
        }
      }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
      s->IsDragging = false;

    if (!s->IsSearching && IsKeyPressed(KEY_DOWN)) {
      if (s->CursorPos + s->ScrollOffset < totalItems - 1) {
        if (s->CursorPos < totalVisible - 1)
          s->CursorPos++;
        else
          s->ScrollOffset++;
      }
    }
    if (!s->IsSearching && IsKeyPressed(KEY_UP)) {
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
        if (s->DB)
          RB_FreeDatabase(s->DB);
        if (s->SeratoDB)
          Serato_FreeDatabase(s->SeratoDB);
        s->DB = NULL;
        s->SeratoDB = NULL;

        // Attempt to load both databases if they exist
        s->DB = RB_LoadDatabase(s->SelectedStorage->Path);
        s->SeratoDB = Serato_LoadDatabase(s->SelectedStorage->Path);
        s->HasBothDatabases = (s->DB != NULL && s->SeratoDB != NULL);

        if (s->DB) {
          s->DatabaseType = 0; // Default to Rekordbox if present
          if (s->TrackPointers)
            free(s->TrackPointers);
          s->TrackPointers =
              (RBTrack **)malloc(s->DB->TrackCount * sizeof(RBTrack *));
        } else if (s->SeratoDB) {
          s->DatabaseType = 1; // Fallback to Serato
        } else {
          s->DatabaseType = 0; // Default
        }

        if (s->SeratoDB) {
          if (s->SeratoTrackPointers)
            free(s->SeratoTrackPointers);
          s->SeratoTrackPointers = (SeratoTrack **)malloc(
              s->SeratoDB->TrackCount * sizeof(SeratoTrack *));
        }

        s->BrowseLevel = 2; // Categories level
        s->CursorPos = s->ScrollOffset = 0;
        printf("[BROWSER] Selected storage: %s (DB Found: %s)\n", 
               s->SelectedStorage->Path, (s->DB || s->SeratoDB) ? "YES" : "NO");

      }
    } else if (s->BrowseLevel == 2) {
      if (s->CursorPos == 5 && s->HasBothDatabases) {
        // TOGGLE DATABASE
        s->DatabaseType = (s->DatabaseType == 0) ? 1 : 0;
        printf("[BROWSER] Switched database to %s\n",
               s->DatabaseType == 0 ? "Rekordbox" : "Serato");
        s->CurrentPlaylistIdx = -1; // Reset playlist selection on switch
        s->CursorPos = s->ScrollOffset = 0;
        Browser_UpdateActiveTracks(s);
      } else if (s->CursorPos == 2) {
        s->BrowseLevel = 1; // Categories to Playlists
      } else if (s->CursorPos == 0 || s->CursorPos == 1 || s->CursorPos == 3 || s->CursorPos == 4) {
        s->BrowseLevel = 0; // Categories to Tracks/Folders/Search
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

  if (!s->IsSearching && IsKeyPressed(KEY_BACKSPACE)) {
    Browser_Back(s);
  }

  if (s->BrowseLevel == 0 && loadToDeck != -1) {
    // LOAD TRACK
    struct DeckState *targetDeck = loadToDeck == 0 ? s->DeckA : s->DeckB;
    if (targetDeck) targetDeck->IsLoading = true;
    if (targetDeck && targetDeck->Waveform.LoadLock && targetDeck->IsPlaying) {
      printf("[BROWSER] LOAD LOCKED: Deck %c is playing\n",
             loadToDeck == 0 ? 'A' : 'B');
      return 0; // Prevent load
    }

    int idx = targetIdx;
    if (s->DatabaseType == 0) { // Rekordbox
      if (idx < s->ActiveTrackCount && s->TrackPointers[idx]) {
        RBTrack *t = s->TrackPointers[idx];
        printf("[BROWSER] Loading RB track: %s to Deck %c\n", t->Title,
               loadToDeck == 0 ? 'A' : 'B');

        if (s->SelectedStorage) {
          RB_LoadTrackData(t, s->SelectedStorage->Path);

          if (s->AudioPlugin) {
            char fullPath[1024];
            const char *relPath = t->FilePath;
            if (relPath[0] == '/' || relPath[0] == '\\')
              relPath++;
            snprintf(fullPath, sizeof(fullPath), "%s/%s",
                     s->SelectedStorage->Path, relPath);
            DeckAudio_LoadTrackAsync(&s->AudioPlugin->Decks[loadToDeck], fullPath);
          }

          struct DeckState *targetDeck = loadToDeck == 0 ? s->DeckA : s->DeckB;
          if (targetDeck) {
            strncpy(targetDeck->TrackTitle, t->Title, 127);
            targetDeck->TrackTitle[127] = '\0';
            strncpy(targetDeck->ArtistName, t->Artist, 127);
            targetDeck->ArtistName[127] = '\0';
            strncpy(targetDeck->AlbumName, t->Album, 127);
            targetDeck->AlbumName[127] = '\0';
            strncpy(targetDeck->GenreName, t->Genre, 63);
            targetDeck->GenreName[63] = '\0';
            strncpy(targetDeck->TrackKey, t->Key, 15);
            targetDeck->TrackKey[15] = '\0';
            strncpy(targetDeck->LabelName, t->Label, 127);
            targetDeck->LabelName[127] = '\0';
            strncpy(targetDeck->Comment, t->Comment, 255);
            targetDeck->Comment[255] = '\0';
            targetDeck->Rating = t->Rating;
            targetDeck->Year = t->Year;
            targetDeck->TrackNumber = t->TrackNumber;
            targetDeck->OriginalBPM = t->BPM;
            targetDeck->CurrentBPM = t->BPM;

            if (s->SelectedStorage) {
              strncpy(targetDeck->SourceName, s->SelectedStorage->Name, 31);
            }

            // Artwork
            if (t->ArtworkPath[0] != '\0') {
              const char *artRel = t->ArtworkPath;
              while (artRel[0] == '/' || artRel[0] == '\\')
                artRel++;
              
              const char *sep = "";
              size_t pLen = strlen(s->SelectedStorage->Path);
              if (pLen > 0 && s->SelectedStorage->Path[pLen-1] != '/' && s->SelectedStorage->Path[pLen-1] != '\\') {
                  sep = "/";
              }
              
              snprintf(targetDeck->ArtworkPath, sizeof(targetDeck->ArtworkPath),
                       "%s%s%s", s->SelectedStorage->Path, sep, artRel);
            } else
              targetDeck->ArtworkPath[0] = '\0';

            // Allocate and setup TrackState
            TrackState *newTrack = (TrackState *)malloc(sizeof(TrackState));
            if (newTrack) {
              memset(newTrack, 0, sizeof(TrackState));
              newTrack->StaticWaveformLen = t->StaticWaveformLen;
              newTrack->StaticWaveformType = t->StaticWaveformType;
              memcpy(newTrack->StaticWaveform, t->StaticWaveform,
                     t->StaticWaveformLen > 8192 ? 8192 : t->StaticWaveformLen);
              
              // DEEP COPY: Isolate dynamic waveform memory per deck
              newTrack->DynamicWaveformLen = t->DynamicWaveformLen;
              if (newTrack->DynamicWaveformLen > 0 && t->DynamicWaveform != NULL) {
                  newTrack->DynamicWaveform = (unsigned char*)malloc(newTrack->DynamicWaveformLen);
                  if (newTrack->DynamicWaveform) {
                      memcpy(newTrack->DynamicWaveform, t->DynamicWaveform, newTrack->DynamicWaveformLen);
                  }
              } else {
                  newTrack->DynamicWaveform = NULL;
              }
              newTrack->WaveformType = t->WaveformType;

              // Cues and Beats
              newTrack->BeatGridCount = t->BeatGridCount;
              if (newTrack->BeatGridCount > 0 && t->BeatGrid != NULL) {
                  newTrack->BeatGrid = (RBBeat*)malloc(sizeof(RBBeat) * newTrack->BeatGridCount);
                  if (newTrack->BeatGrid) {
                      memcpy(newTrack->BeatGrid, t->BeatGrid, sizeof(RBBeat) * newTrack->BeatGridCount);
                  }
              } else {
                  newTrack->BeatGrid = NULL;
              }

              for (uint32_t i = 0; i < t->CueCount && i < 32; i++) {
                if (t->Cues[i].ID >= 1 && t->Cues[i].ID <= 8) {
                  newTrack->HotCues[newTrack->HotCuesCount].ID = t->Cues[i].ID;
                  newTrack->HotCues[newTrack->HotCuesCount].Start =
                      t->Cues[i].Time;
                  memcpy(newTrack->HotCues[newTrack->HotCuesCount].Color,
                         t->Cues[i].Color, 3);
                  newTrack->HotCuesCount++;
                } else if (t->Cues[i].ID == 0) {
                  newTrack->Cues[newTrack->CuesCount].Start = t->Cues[i].Time;
                  memcpy(newTrack->Cues[newTrack->CuesCount].Color,
                         t->Cues[i].Color, 3);
                  newTrack->CuesCount++;
                }
              }

              TrackState *oldTrack = targetDeck->LoadedTrack;
              targetDeck->LoadedTrack = newTrack; // Atomic pointer swap on UI thread

              // Cleanup old track memory (including buffers)
              if (oldTrack){
                if (oldTrack->BeatGrid != NULL) free(oldTrack->BeatGrid);
                if (oldTrack->DynamicWaveform != NULL) free(oldTrack->DynamicWaveform);
                free(oldTrack);
              }
                
              targetDeck->PositionMs = (newTrack->CuesCount > 0)
                                           ? newTrack->Cues[0].Start
                                           : (newTrack->BeatGridCount > 0
                                                  ? newTrack->BeatGrid[0].Time
                                                  : 0);
              DeckAudio_JumpToMs(&s->AudioPlugin->Decks[loadToDeck],
                                 (uint32_t)targetDeck->PositionMs);
            }
          }
        }
      }
    } else { // Serato
      if (idx < s->ActiveTrackCount && s->SeratoTrackPointers[idx]) {
        SeratoTrack *t = s->SeratoTrackPointers[idx];
        printf("[BROWSER] Loading Serato track: %s to Deck %c\n", t->Title,
               loadToDeck == 0 ? 'A' : 'B');

        if (s->SelectedStorage) {
          Serato_LoadTrackData(t, s->SelectedStorage->Path);

          if (s->AudioPlugin) {

            char fullPath[1024];
            const char *relPath = t->FilePath;
            // Serato locations can be absolute or relative. Let's assume
            // relative to root if it starts with /
            if (relPath[0] == '/' || relPath[0] == '\\')
              relPath++;
            snprintf(fullPath, sizeof(fullPath), "%s/%s",
                     s->SelectedStorage->Path, relPath);
            DeckAudio_LoadTrack(&s->AudioPlugin->Decks[loadToDeck], fullPath);
          }

          struct DeckState *targetDeck = loadToDeck == 0 ? s->DeckA : s->DeckB;
          if (targetDeck) {
            strncpy(targetDeck->TrackTitle, t->Title, 127);
            targetDeck->TrackTitle[127] = '\0';
            strncpy(targetDeck->ArtistName, t->Artist, 127);
            targetDeck->ArtistName[127] = '\0';
            strncpy(targetDeck->AlbumName, t->Album, 127);
            targetDeck->AlbumName[127] = '\0';
            strncpy(targetDeck->GenreName, t->Genre, 63);
            targetDeck->GenreName[63] = '\0';
            strncpy(targetDeck->TrackKey, t->Key, 15);
            targetDeck->TrackKey[15] = '\0';
            strncpy(targetDeck->LabelName, t->Label, 127);
            targetDeck->LabelName[127] = '\0';
            strncpy(targetDeck->Comment, t->Comment, 255);
            targetDeck->Comment[255] = '\0';
            targetDeck->Rating = 0; // Serato rating not in DB v2
            targetDeck->Year = t->Year;
            targetDeck->TrackNumber = 0; // Not available in Serato DB v2
            targetDeck->OriginalBPM = t->BPM;
            targetDeck->CurrentBPM = t->BPM;

            if (s->SelectedStorage) {
              strncpy(targetDeck->SourceName, s->SelectedStorage->Name, 31);
            }
            targetDeck->ArtworkPath[0] =
                '\0'; // Serato artwork not implemented yet

            TrackState *newTrack = (TrackState *)malloc(sizeof(TrackState));
            if (newTrack) {
              memset(newTrack, 0, sizeof(TrackState));

              // Copy cues from Serato metadata
              for (uint32_t i = 0; i < t->CueCount && i < 32; i++) {
                if (t->Cues[i].ID >= 1 && t->Cues[i].ID <= 8) {
                  newTrack->HotCues[newTrack->HotCuesCount].ID = t->Cues[i].ID;
                  newTrack->HotCues[newTrack->HotCuesCount].Start =
                      t->Cues[i].Time;
                  memcpy(newTrack->HotCues[newTrack->HotCuesCount].Color,
                         t->Cues[i].Color, 3);
                  newTrack->HotCuesCount++;
                } else {
                  newTrack->Cues[newTrack->CuesCount].Start = t->Cues[i].Time;
                  memcpy(newTrack->Cues[newTrack->CuesCount].Color,
                         t->Cues[i].Color, 3);
                  newTrack->CuesCount++;
                }
              }

              TrackState *oldTrack = targetDeck->LoadedTrack;
              targetDeck->LoadedTrack = newTrack;
              if (oldTrack) {
                if (oldTrack->BeatGrid != NULL) free(oldTrack->BeatGrid);
                if (oldTrack->DynamicWaveform != NULL) free(oldTrack->DynamicWaveform);
                free(oldTrack);
              }
              targetDeck->PositionMs =
                  (newTrack->CuesCount > 0) ? newTrack->Cues[0].Start : 0;
              DeckAudio_JumpToMs(&s->AudioPlugin->Decks[loadToDeck],
                                 (uint32_t)targetDeck->PositionMs);
            }
          }
        }
      }
    }
  }

  if (!s->IsSearching && (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) &&
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
    bool isAssigned = isBank && (s->PlaylistBank[i - 4].PlaylistIdx >= 0);

    bool isActiveNav = false;
    if (!isBank) {
      if (i == 0 && s->BrowseLevel == 0)
        isActiveNav = true; // Tracks
      if (i == 1 && s->BrowseLevel == 2)
        isActiveNav = true; // Folders/Categories
      if (i == 2 && s->BrowseLevel == 1)
        isActiveNav = true; // Playlists
      if (i == 3 && s->BrowseLevel == 3)
        isActiveNav = true; // Source
    }

    // Background
    Color bg = ColorDark2;
    if (isHovered)
      bg = ColorDark1;
    if (isActiveNav)
      bg = (Color){30, 30, 60, 255};
    if (isBank)
      bg = isAssigned ? ColorDGreen : ColorDark3;

    DrawRectangle(0, boxY, sidebarW, sidebarW, bg);
    if (isActiveNav)
      DrawRectangle(0, boxY, S(3), sidebarW, ColorBlue);

    // Inner separation lines
    DrawRectangleLinesEx(boxRect, 1.0f, ColorDark1);

    if (!isBank) {
      const char *sidIcons[] = {"\uf03a", "\uf07b", "\uf5c0",
                                "\uf287"}; // Tracks, Folders, Playlist, USB
      DrawCentredText(sidIcons[i], (i == 3) ? faceBrand : faceIcon, 0, sidebarW,
                      boxY + S(12), S(16),
                      isActiveNav ? ColorBlue
                                  : (isHovered ? ColorWhite : ColorShadow));
    } else {
      // Playlist Bank Placeholders (1-3)
      int bankIdx = i - 4;
      if (s->PlaylistBank[bankIdx].PlaylistIdx >= 0) {
        // Show truncated name (e.g. "Techno")
        char displayName[8];
        strncpy(displayName, s->PlaylistBank[bankIdx].Name, 7);
        displayName[7] = '\0';
        DrawCentredText(displayName, faceXS, 0, sidebarW, boxY + S(15), S(9), ColorWhite);
      } else {
        char bankNum[4];
        sprintf(bankNum, "P%d", bankIdx + 1);
        DrawCentredText("\uf02e", faceIcon, 0, sidebarW, boxY + S(8), S(14), ColorShadow);
        DrawCentredText(bankNum, faceXS, 0, sidebarW, boxY + S(24), S(10), ColorShadow);
      }
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
    if (s->DatabaseType == 0)
      totalPl = s->DB ? s->DB->PlaylistCount : 0;
    else
      totalPl = s->SeratoDB ? s->SeratoDB->PlaylistCount : 0;
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

  float listYOffset = TOP_BAR_H;
  float rowH = S(28.0f);
  int totalVisible = 10;
  float listX = sidebarW;
  float listW = SCREEN_WIDTH - sidebarW - S(8);
  if (s->InfoEnabled)
    listW = SCREEN_WIDTH - sidebarW - S(160);

  // Draw Search Box & Sort Button
  if (s->BrowseLevel == 0) {
    totalVisible = 9;
    float sortButtonW = S(80);
    Rectangle searchBoxRect = {listX, listYOffset, listW - sortButtonW - S(4), rowH};
    Rectangle sortButtonRect = {listX + listW - sortButtonW, listYOffset, sortButtonW, rowH};
    listYOffset += rowH;
    
    // Draw Search Input
    DrawRectangleRec(searchBoxRect, s->IsSearching ? ColorDark1 : ColorDark2);
    DrawRectangleLinesEx(searchBoxRect, 1.0f, s->IsSearching ? ColorBlue : ColorDark1);
    
    char displayQuery[128];
    if (strlen(s->SearchQuery) > 0) {
        snprintf(displayQuery, sizeof(displayQuery), "%s", s->SearchQuery);
    } else {
        snprintf(displayQuery, sizeof(displayQuery), s->IsSearching ? "" : "Search...");
    }
    if (s->IsSearching && (int)(GetTime() * 2) % 2 == 0) {
        strncat(displayQuery, "|", sizeof(displayQuery) - strlen(displayQuery) - 1);
    }
    UIDrawText("\uf002", faceIcon, searchBoxRect.x + S(8), searchBoxRect.y + S(8), S(12), s->IsSearching ? ColorWhite : ColorShadow);
    UIDrawText(displayQuery, faceXS, searchBoxRect.x + S(30), searchBoxRect.y + S(9), S(10), s->IsSearching ? ColorWhite : ColorShadow);

    // Draw Sort Button
    DrawRectangleRec(sortButtonRect, s->ShowSortDropdown ? ColorDark1 : ColorDark2);
    DrawRectangleLinesEx(sortButtonRect, 1.0f, ColorDark1);
    const char* sortLabels[] = {"Default", "BPM", "Key", "Title", "Rating"}; // Added 2 labels
    UIDrawText(sortLabels[s->SortMode], faceXS, sortButtonRect.x + S(8), sortButtonRect.y + S(9), S(10), ColorWhite);
    UIDrawText("\uf0d7", faceIcon, sortButtonRect.x + sortButtonW - S(18), sortButtonRect.y + S(10), S(10), ColorShadow); // Chevrons
  }

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
        if (s->SeratoDB && idx >= 0 &&
            (uint32_t)idx < s->SeratoDB->PlaylistCount)
          title = s->SeratoDB->Playlists[idx].Name;
      }
      break;
    case 2:
      if (idx < 5)
        title = categories[idx];
      else if (idx == 5 && s->HasBothDatabases) {
        static char switchBuf[32];
        sprintf(switchBuf, "SWITCH TO %s",
                s->DatabaseType == 0 ? "SERATO" : "REKORDBOX");
        title = switchBuf;
      }
      break;
    case 3:
      if (idx < s->StorageCount)
        title = s->AvailableStorages[idx].Name;
      break;
    }

    if (title[0] == '\0')
      continue;

    float ry = listYOffset + i * rowH;
    bool isCursor = (i == s->CursorPos);

    if (isCursor) {
      DrawRectangle(listX, ry + 1, listW, rowH - 2, ColorBlue);
      if (s->BrowseLevel > 0 && !s->IsTagList) {
        DrawSelectionTriangle(listX + S(2), ry + (rowH / 2.0f), ColorWhite);
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

    // Marquee Logic for Title (Optimized: only measure for cursor item)
    float maxTitleW = listW - (textX - listX) - S(130);

    if (isCursor) {
      Vector2 fullSize = MeasureTextEx(faceSm, title, S(13), 1.0f);
      if (fullSize.x > maxTitleW) {
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
        UIDrawText(title, faceSm, textX, textY, S(13), ColorWhite);
      }
    } else {
      // Normal truncated display with ellipsis
      UIDrawTextTruncated(title, faceSm, textX, textY, S(13), ColorWhite, maxTitleW);
    }

    if (artist[0] != '\0' && s->BrowseLevel == 0 && !s->InfoEnabled) {
      UIDrawText(artist, faceXS, textX, ry + S(15), S(10),
                 isCursor ? ColorWhite : ColorShadow);
    }

    // BPM & Key & LOAD Button
    if (s->BrowseLevel == 0 && !s->InfoEnabled) {
      UIDrawText(bpmText, faceXS, listX + listW - S(125), ry + S(9), S(10),
                 isCursor ? ColorWhite : ColorShadow);
      UIDrawText(keyStr, faceXS, listX + listW - S(85), ry + S(9), S(10),
                 isCursor ? ColorWhite : ColorShadow);

      // LOAD Button
      float loadW = S(45);
      Rectangle loadRect = {listX + listW - loadW - S(5), ry + S(4), loadW,
                            rowH - S(8)};
      bool hoverLoad = CheckCollisionPointRec(mPos, loadRect);

      DrawRectangleRec(loadRect, hoverLoad ? ColorBlue : ColorDark3);
      DrawRectangleLinesEx(loadRect, 1.0f, ColorShadow);
      DrawCentredText("LOAD", faceXS, loadRect.x, loadRect.width,
                      loadRect.y + S(5), S(10), ColorWhite);
    }

    // Storage icons
    if (s->BrowseLevel == 3 && idx < s->StorageCount) {
      const char *icon = "\uf287"; // USB
      Font iconFont = faceBrand;

      if (strcmp(s->AvailableStorages[idx].Type, "Internal") == 0) {
        icon = "\uf3cd"; // Mobile icon for Internal
        iconFont = faceIcon;
      } else if (strcmp(s->AvailableStorages[idx].Type, "SD") == 0) {
        icon = "\uf7c2"; // SD Card
        iconFont = faceIcon;
      }

      UIDrawText(icon, iconFont, listX + S(11), ry + S(7), S(12), ColorWhite);
    }
  }

  // Scrollbar
  int maxItems = 0;
  if (s->BrowseLevel == 0)
    maxItems = s->ActiveTrackCount;
  else if (s->BrowseLevel == 1) {
    if (s->DatabaseType == 0)
      maxItems = s->DB ? (int)s->DB->PlaylistCount : 0;
    else
      maxItems = s->SeratoDB ? (int)s->SeratoDB->PlaylistCount : 0;
  } else if (s->BrowseLevel == 2)
    maxItems = 5;
  else if (s->BrowseLevel == 3)
    maxItems = s->StorageCount;

  DrawScrollbar(SCREEN_WIDTH - S(4), TOP_BAR_H, S(2), viewH - TOP_BAR_H,
                maxItems, s->ScrollOffset, totalVisible);

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
        if (s->TrackPointers[s->PopupTrackIdx])
          trackName = s->TrackPointers[s->PopupTrackIdx]->Title;
      } else {
        if (s->SeratoTrackPointers[s->PopupTrackIdx])
          trackName = s->SeratoTrackPointers[s->PopupTrackIdx]->Title;
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

  // Drag and Drop Visual (Only for Playlists)
  if (s->IsDragging && s->DraggingType == 1) {
    char dragName[128] = "Playlist";
    if (s->DatabaseType == 0 && s->DB && s->DraggingIdx >= 0 &&
        (uint32_t)s->DraggingIdx < s->DB->PlaylistCount) {
      strncpy(dragName, s->DB->Playlists[s->DraggingIdx].Name, 127);
    } else if (s->DatabaseType == 1 && s->SeratoDB && s->DraggingIdx >= 0 &&
               (uint32_t)s->DraggingIdx < s->SeratoDB->PlaylistCount) {
      strncpy(dragName, s->SeratoDB->Playlists[s->DraggingIdx].Name, 127);
    }

    float dw = S(120), dh = S(22);
    DrawRectangle(mPos.x + 10, mPos.y + 10, dw, dh, Fade(ColorDark3, 0.8f));
    DrawRectangleLines(mPos.x + 10, mPos.y + 10, dw, dh, ColorBlue);
    UIDrawText(dragName, faceXS, mPos.x + 15, mPos.y + 15, S(10), ColorWhite);
  }

  // Overlay Dropdown Menu
  if (s->BrowseLevel == 0 && s->ShowSortDropdown) {
    float listW = SCREEN_WIDTH - sidebarW - S(8);
    if (s->InfoEnabled) listW = SCREEN_WIDTH - sidebarW - S(160);
    float sortButtonW = S(80);
    
    Rectangle dropdownRect = {sidebarW + listW - sortButtonW, TOP_BAR_H + rowH, sortButtonW, rowH * 5};
    DrawRectangleRec(dropdownRect, ColorDark2);
    DrawRectangleLinesEx(dropdownRect, 1.0f, ColorShadow);

    const char* sortLabelsList[] = {"Default", "BPM", "Key", "Title", "Rating"};
    for (int i = 0; i < 5; i++) { // Loop to 5
        Rectangle itemRect = {dropdownRect.x, dropdownRect.y + i * rowH, dropdownRect.width, rowH};
        bool hover = CheckCollisionPointRec(mPos, itemRect);
        
        if (hover) DrawRectangleRec(itemRect, ColorDark1);
        if (s->SortMode == i) DrawRectangle(itemRect.x, itemRect.y, S(3), itemRect.height, ColorBlue);

        UIDrawText(sortLabelsList[i], faceXS, itemRect.x + S(10), itemRect.y + S(9), S(10), hover ? ColorWhite : ColorShadow);
    }
  }
}

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state) {
  r->base.Update = Browser_Update;
  r->base.Draw = Browser_Draw;
  r->State = state;
}
