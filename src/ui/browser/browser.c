#include "ui/browser/browser.h"
#include "core/memory_guard.h"
#include "core/logger.h"
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
        uint32_t tid = s->TagList[i];
        if (tid > 0 && tid <= s->DB->TrackCount && s->DB->Tracks[tid - 1].ID == tid) {
          s->TrackPointers[i] = &s->DB->Tracks[tid - 1];
        } else {
          for (uint32_t j = 0; j < s->DB->TrackCount; j++) {
            if (s->DB->Tracks[j].ID == tid) {
              s->TrackPointers[i] = &s->DB->Tracks[j];
              break;
            }
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
        if (tid > 0 && tid <= s->DB->TrackCount && s->DB->Tracks[tid - 1].ID == tid) {
          s->TrackPointers[i] = &s->DB->Tracks[tid - 1];
        } else {
          for (uint32_t j = 0; j < s->DB->TrackCount; j++) {
            if (s->DB->Tracks[j].ID == tid) {
              s->TrackPointers[i] = &s->DB->Tracks[j];
              break;
            }
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
        if (tid > 0 && tid <= s->SeratoDB->TrackCount && s->SeratoDB->Tracks[tid - 1].ID == tid) {
          s->SeratoTrackPointers[i] = &s->SeratoDB->Tracks[tid - 1];
        } else {
          for (uint32_t j = 0; j < s->SeratoDB->TrackCount; j++) {
            if (s->SeratoDB->Tracks[j].ID == tid) {
              s->SeratoTrackPointers[i] = &s->SeratoDB->Tracks[j];
              break;
            }
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
  }
}

void Browser_CheckStorageConnection(BrowserState *s) {
  if (!s || !s->SelectedStorage) return;

  struct stat st;
  bool isConnected = false;

  if (s->SelectedStorage->Path[0] != '\0') {
    if (stat(s->SelectedStorage->Path, &st) == 0) {
      isConnected = true;
    }
  }

  if (!isConnected) {
    char devName[128];
    snprintf(devName, sizeof(devName), "%s",
             (s->SelectedStorage->Name[0] != '\0') ? s->SelectedStorage->Name : "USB Device");

    char toastMsg[160];
    snprintf(toastMsg, sizeof(toastMsg), "USB DISCONNECTED: %s", devName);

    // 1. Alert Toast Notification (Red warning banner)
    Toast_Show(toastMsg, 4.0f, (Color){240, 50, 50, 255});

    UNX_LOG_WARN("[BROWSER] Storage device disconnected unexpectedly: %s (%s). Resetting browser state.",
                 devName, s->SelectedStorage->Path);

    // 2. Free memory allocated for this storage DB
    if (s->DB) {
      RB_FreeDatabase(s->DB);
      s->DB = NULL;
    }
    if (s->SeratoDB) {
      Serato_FreeDatabase(s->SeratoDB);
      s->SeratoDB = NULL;
    }
    if (s->TrackPointers) {
      free(s->TrackPointers);
      s->TrackPointers = NULL;
    }
    if (s->SeratoTrackPointers) {
      free(s->SeratoTrackPointers);
      s->SeratoTrackPointers = NULL;
    }

    // 3. Reset browser state to Source Selection (BrowseLevel = 3)
    s->SelectedStorage = NULL;
    s->BrowseLevel = 3;
    s->CursorPos = 0;
    s->ScrollOffset = 0;
    s->VisualScroll = 0.0f;
    s->ScrollVelocity = 0.0f;
    s->CurrentPlaylistIdx = -1;
    s->ActiveTrackCount = 0;
    s->IsSearching = false;
    s->SearchQuery[0] = '\0';
    s->IsTagList = false;

    // 4. Refresh available device list
    Browser_RefreshStorages(s);
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

  static char s_knownPaths[16][512];
  static char s_knownNames[16][128];
  static int s_knownCount = 0;
  static bool s_hasInitializedStorages = false;

  if (!s_hasInitializedStorages) {
    s_knownCount = s->StorageCount;
    for (int i = 0; i < s->StorageCount && i < 16; i++) {
      strncpy(s_knownPaths[i], s->AvailableStorages[i].Path, 511);
      strncpy(s_knownNames[i], s->AvailableStorages[i].Name, 127);
    }
    s_hasInitializedStorages = true;
  } else {
    // Check for NEWLY CONNECTED USB devices
    for (int i = 0; i < s->StorageCount; i++) {
      bool isNew = true;
      for (int k = 0; k < s_knownCount; k++) {
        if (strcmp(s->AvailableStorages[i].Path, s_knownPaths[k]) == 0) {
          isNew = false;
          break;
        }
      }
      if (isNew) {
        char toastMsg[160];
        snprintf(toastMsg, sizeof(toastMsg), "USB CONNECTED: %s", s->AvailableStorages[i].Name);
        Toast_Show(toastMsg, 3.5f, (Color){40, 200, 80, 255}); // Green Toast
      }
    }

    // Check for REMOVED / DISCONNECTED USB devices
    for (int k = 0; k < s_knownCount; k++) {
      bool isRemoved = true;
      for (int i = 0; i < s->StorageCount; i++) {
        if (strcmp(s_knownPaths[k], s->AvailableStorages[i].Path) == 0) {
          isRemoved = false;
          break;
        }
      }
      if (isRemoved) {
        char toastMsg[160];
        snprintf(toastMsg, sizeof(toastMsg), "USB DISCONNECTED: %s", s_knownNames[k]);
        Toast_Show(toastMsg, 4.0f, (Color){240, 50, 50, 255}); // Red Toast
      }
    }

    s_knownCount = s->StorageCount;
    for (int i = 0; i < s->StorageCount && i < 16; i++) {
      strncpy(s_knownPaths[i], s->AvailableStorages[i].Path, 511);
      strncpy(s_knownNames[i], s->AvailableStorages[i].Name, 127);
    }
  }
}

static int Browser_Update(Component *base) {
  BrowserRenderer *r = (BrowserRenderer *)base;
  BrowserState *s = r->State;

  if (!s->IsActive)
    return 0;

  // Check storage connection and auto-reset state if disconnected
  Browser_CheckStorageConnection(s);

  int loadToDeck = -1;
  int targetIdx = s->ScrollOffset + s->CursorPos;
  bool triggerEnter = false;

  // Dropdown and Search Box Interaction
  Vector2 mousePos = UIGetMousePosition();
  if (s->BrowseLevel == 0) {
    float sidebarW = S(40);
    float listW = SCREEN_WIDTH - sidebarW - S(8);
    if (s->InfoEnabled) listW = SCREEN_WIDTH - sidebarW - S(160);
    
    float sortButtonW = S(65);
    float oskButtonW = S(36);
    Rectangle sortButtonRect = {sidebarW + listW - sortButtonW, TOP_BAR_H, sortButtonW, S(28.0f)};
    Rectangle dropdownRect = {sortButtonRect.x, sortButtonRect.y + sortButtonRect.height, sortButtonW, S(28.0f) * 5}; // 5 items
    Rectangle oskButtonRect = {sidebarW + listW - sortButtonW - oskButtonW - S(4), TOP_BAR_H, oskButtonW, S(28.0f)};
    Rectangle searchBoxRect = {sidebarW, TOP_BAR_H, listW - sortButtonW - oskButtonW - S(8), S(28.0f)};

    // Handle OSK Panel Touch Absorption
    if (s->ShowOSK) {
      float viewH = SCREEN_HEIGHT - DECK_STR_H;
      float oskH = S(210);
      Rectangle oskPanelRect = {sidebarW, viewH - oskH, SCREEN_WIDTH - sidebarW, oskH};
      if (CheckCollisionPointRec(mousePos, oskPanelRect)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          // Touch absorbed by OSK overlay
        }
      }
    }

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
        s->VisualScroll = 0;
        s->ScrollVelocity = 0;
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
        // Handle OSK Button Toggle
        if (CheckCollisionPointRec(mousePos, oskButtonRect)) {
            s->ShowOSK = !s->ShowOSK;
            s->IsSearching = s->ShowOSK;
            return 0;
        }

        // Handle Sort Button (Open Menu)
        if (CheckCollisionPointRec(mousePos, sortButtonRect)) {
            s->ShowSortDropdown = true;
            return 0; // Absorb click
        }

        // Handle Search Box
        if (CheckCollisionPointRec(mousePos, searchBoxRect)) {
            s->IsSearching = true;
            s->ShowOSK = true;
        } else if (!s->ShowOSK) {
            // Click outside search box
            if (s->IsSearching) {
                s->IsSearching = false;
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
      float itemRowH = S(28.0f);
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
      s->VisualScroll = (float)(s->ScrollOffset * itemRowH);
      s->ScrollVelocity = 0;
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
          s->VisualScroll = 0;
          s->ScrollVelocity = 0;
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
        s->VisualScroll = 0;
        s->ScrollVelocity = 0;
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

  // 2. Data State
  int totalItems = 0;
  if (s->IsTagList)
    totalItems = s->TagListCount;
  else {
    switch (s->BrowseLevel) {
    case 0:
      totalItems = s->ActiveTrackCount;
      break;
    case 1:
      if (s->DatabaseType == 0)
        totalItems = s->DB ? s->DB->PlaylistCount : 0;
      else
        totalItems = s->SeratoDB ? s->SeratoDB->PlaylistCount : 0;
      break;
    case 2:
      totalItems = 5 + (s->HasBothDatabases ? 1 : 0);
      break;
    case 3:
      totalItems = s->StorageCount;
      break;
    }
  }

  // 3. Touch Kinetic Scrolling & Interactive Scrollbar Logic
  if (!s->ShowLoadPopup) {
    float maxScroll = (totalItems - totalVisible) * rowH;
    if (maxScroll < 0) maxScroll = 0;

    float viewH = SCREEN_HEIGHT - DECK_STR_H;
    float sbTrackW = S(28);
    float sbTrackX = SCREEN_WIDTH - sbTrackW;
    float sbTrackY = TOP_BAR_H;
    float sbTrackH = viewH - TOP_BAR_H;
    Rectangle sbRect = {sbTrackX, sbTrackY, sbTrackW, sbTrackH};

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      s->TouchDragAccumulator = 0.0f;
      s->TouchVelocityY = 0.0f;
      s->LastTouchY = mousePos.y;

      if (CheckCollisionPointRec(mousePos, sbRect)) {
        s->IsScrollbarDragging = true;
        s->IsDragging = false;
      } else {
        s->IsScrollbarDragging = false;
      }
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      if (s->IsScrollbarDragging) {
        float ratio = (mousePos.y - sbTrackY) / sbTrackH;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        s->VisualScroll = ratio * maxScroll;
        s->ScrollVelocity = 0.0f;
      } else {
        float frameTime = GetFrameTime();
        if (frameTime < 0.001f) frameTime = 0.016f;
        float dy = mousePos.y - s->LastTouchY;
        s->LastTouchY = mousePos.y;
        s->TouchDragAccumulator += fabsf(dy);

        if (!s->IsDragging && s->TouchDragAccumulator > S(8.0f)) {
          s->IsDragging = true;
        }

        if (s->IsDragging) {
          float resistance = 1.0f;
          if (s->VisualScroll < 0 && dy > 0) resistance = 0.3f;
          if (s->VisualScroll > maxScroll && dy < 0) resistance = 0.3f;

          s->VisualScroll -= dy * resistance;
          float instantVel = (-dy * resistance) / frameTime;
          s->TouchVelocityY = s->TouchVelocityY * 0.3f + instantVel * 0.7f;
        }
      }
    } else {
      // Kinetic Inertia Decay (Smooth Flick)
      s->VisualScroll += s->ScrollVelocity * GetFrameTime();
      s->ScrollVelocity *= 0.95f; // Friction
      if (fabsf(s->ScrollVelocity) < 5.0f) s->ScrollVelocity = 0.0f;

      // Elastic Bungee Return (Spring-back)
      float springK = 14.0f; 
      if (s->VisualScroll < 0) {
        s->VisualScroll -= s->VisualScroll * springK * GetFrameTime();
        if (fabsf(s->VisualScroll) < 0.5f) s->VisualScroll = 0.0f;
      }
      if (s->VisualScroll > maxScroll) {
        float diff = s->VisualScroll - maxScroll;
        s->VisualScroll -= diff * springK * GetFrameTime();
        if (fabsf(s->VisualScroll - maxScroll) < 0.5f) s->VisualScroll = maxScroll;
      }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      if (s->IsDragging && fabsf(s->TouchVelocityY) > 60.0f) {
        s->ScrollVelocity = s->TouchVelocityY; // Apply touch flick velocity
      }
      s->IsDragging = false;
      s->IsScrollbarDragging = false;
    }

    // Wheel Scroll
    float wheelMove = GetMouseWheelMove();
    if (wheelMove != 0) {
      s->VisualScroll -= wheelMove * rowH * 3.0f;
      s->ScrollVelocity = 0.0f; 
    }

    // Constraints for interaction safety
    if (s->VisualScroll < -S(120)) s->VisualScroll = -S(120);
    if (s->VisualScroll > maxScroll + S(120)) s->VisualScroll = maxScroll + S(120);

    // Sync back to discrete offsets for logic compatibility
    s->ScrollOffset = (int)(s->VisualScroll / rowH);
    if (s->ScrollOffset < 0) s->ScrollOffset = 0;
    int maxOffset = totalItems - totalVisible;
    if (maxOffset < 0) maxOffset = 0;
    if (s->ScrollOffset > maxOffset) s->ScrollOffset = maxOffset;
  }

  // 3. List Item Interaction (Tap & Load trigger)
  if (!s->ShowLoadPopup && !s->IsScrollbarDragging) {
    float pixelOffset = fmodf(s->VisualScroll, rowH);
    for (int i = 0; i < totalVisible + 1; i++) {
      int idx = s->ScrollOffset + i;
      if (idx < 0 || idx >= totalItems) continue;

      float ry = listYOffset - pixelOffset + i * rowH;
      Rectangle itemRect = {sidebarW, ry, listW - S(28), rowH};

      // Visually within the list clip area
      if (ry < listYOffset - S(10) || ry > SCREEN_HEIGHT - DECK_STR_H - S(5)) continue;

      if (CheckCollisionPointRec(mousePos, itemRect)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          s->DraggingIdx = idx;
          if (s->BrowseLevel == 1) s->DraggingType = 1; 
          else if (s->BrowseLevel == 0) s->DraggingType = 0;
          else s->DraggingType = -1;

          if (s->CursorPos + s->ScrollOffset != idx) {
            s->CursorPos = idx - s->ScrollOffset;
            s->MarqueeScrollX = 0; 
          }
        }

        float loadBtnW = S(45);
        Rectangle loadBtnRect = {sidebarW + listW - loadBtnW - S(32), ry + S(4), loadBtnW, rowH - S(8)};
        bool isLoadClick = (s->BrowseLevel == 0) && CheckCollisionPointRec(mousePos, loadBtnRect);

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !s->IsDragging) {
          if (s->TouchDragAccumulator < S(8.0f) && fabsf(s->ScrollVelocity) < 40.0f) {
            if (isLoadClick) {
              s->ShowLoadPopup = true;
              s->PopupTrackIdx = idx;
            } else if (!s->IsTagList) {
              triggerEnter = true;
            }
          }
        }
      }
    }
  }

  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
    s->IsDragging = false;

  // 4. Keyboard Navigation (Sync with VisualScroll)
  if (!s->IsSearching && !s->ShowLoadPopup) {
    if (IsKeyPressed(KEY_DOWN)) {
      if (s->CursorPos + s->ScrollOffset < totalItems - 1) {
        s->CursorPos++;
        // Keep cursor in view
        if (s->CursorPos >= totalVisible) {
            s->CursorPos = totalVisible - 1;
            s->VisualScroll += rowH;
            s->ScrollVelocity = 0;
        }
      }
    }
    if (IsKeyPressed(KEY_UP)) {
      if (s->CursorPos + s->ScrollOffset > 0) {
        s->CursorPos--;
        // Keep cursor in view
        if (s->CursorPos < 0) {
            s->CursorPos = 0;
            s->VisualScroll -= rowH;
            s->ScrollVelocity = 0;
        }
      }
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
        s->VisualScroll = 0;
        s->ScrollVelocity = 0;
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
        s->VisualScroll = 0;
        s->ScrollVelocity = 0;
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
        s->VisualScroll = 0;
        s->ScrollVelocity = 0;
        }
      } else {
        if (s->SeratoDB && idx < (int)s->SeratoDB->PlaylistCount) {
          s->CurrentPlaylistIdx = idx;
          s->BrowseLevel = 0;
          Browser_UpdateActiveTracks(s);
          s->CursorPos = s->ScrollOffset = 0;
        s->VisualScroll = 0;
        s->ScrollVelocity = 0;
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

        if (MemoryGuard_GetLevel() == MEM_MODE_CRITICAL) {
            UNX_LOG_ERR("[BROWSER] LOAD BLOCKED: Memory is critical.");
            s->ShowLoadPopup = false;
            return 0;
        }

        if (s->SelectedStorage) {
          RB_LoadTrackData(&t->Analysis, t->AnalyzePath, t->Title, t->ID, s->SelectedStorage->Path);

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
            targetDeck->OriginalBPM = (t->BPM > 0) ? t->BPM : 120.0f;

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
            if (!newTrack) {
                UNX_LOG_ERR("[BROWSER] OOM: Failed to allocate TrackState");
                s->ShowLoadPopup = false;
                return 0;
            }
            
            memset(newTrack, 0, sizeof(TrackState));
              
              if (s->SelectedStorage) {
                snprintf(newTrack->AnalyzePath, sizeof(newTrack->AnalyzePath), "%s/%s", s->SelectedStorage->Path, t->AnalyzePath);
              }

              newTrack->Analysis.StaticWaveformLen = t->Analysis.StaticWaveformLen;
              newTrack->Analysis.StaticWaveformType = t->Analysis.StaticWaveformType;
              memcpy(newTrack->Analysis.StaticWaveform, t->Analysis.StaticWaveform,
                     t->Analysis.StaticWaveformLen > 8192 ? 8192 : t->Analysis.StaticWaveformLen);
              
              // DEEP COPY: Isolate dynamic waveform memory per deck
              newTrack->Analysis.DynamicWaveformLen = t->Analysis.DynamicWaveformLen;
              if (newTrack->Analysis.DynamicWaveformLen > 0 && t->Analysis.DynamicWaveform != NULL) {
                  newTrack->Analysis.DynamicWaveform = (unsigned char*)malloc(newTrack->Analysis.DynamicWaveformLen);
                  if (newTrack->Analysis.DynamicWaveform) {
                      memcpy(newTrack->Analysis.DynamicWaveform, t->Analysis.DynamicWaveform, newTrack->Analysis.DynamicWaveformLen);
                      newTrack->Analysis.WaveformType = t->Analysis.WaveformType;
                  }
              }

              // Deep copy BeatGrid
              newTrack->Analysis.BeatGridCount = t->Analysis.BeatGridCount;
              if (newTrack->Analysis.BeatGridCount > 0 && t->Analysis.BeatGrid != NULL) {
                  newTrack->Analysis.BeatGrid = (RBBeat*)malloc(sizeof(RBBeat) * newTrack->Analysis.BeatGridCount);
                  if (newTrack->Analysis.BeatGrid) {
                      memcpy(newTrack->Analysis.BeatGrid, t->Analysis.BeatGrid, sizeof(RBBeat) * newTrack->Analysis.BeatGridCount);
                  }
              }

              // Deep copy Phrases
              newTrack->Analysis.PhraseCount = t->Analysis.PhraseCount;
              if (newTrack->Analysis.PhraseCount > 0 && t->Analysis.Phrases != NULL) {
                  newTrack->Analysis.Phrases = (RBPhrase*)malloc(sizeof(RBPhrase) * newTrack->Analysis.PhraseCount);
                  if (newTrack->Analysis.Phrases) {
                      memcpy(newTrack->Analysis.Phrases, t->Analysis.Phrases, sizeof(RBPhrase) * newTrack->Analysis.PhraseCount);
                  }
              }

              // Deep copy Cues
              newTrack->Analysis.CueCount = t->Analysis.CueCount;
              if (newTrack->Analysis.CueCount > 0 && t->Analysis.Cues != NULL) {
                  newTrack->Analysis.Cues = (RBCue*)malloc(sizeof(RBCue) * newTrack->Analysis.CueCount);
                  if (newTrack->Analysis.Cues) {
                      memcpy(newTrack->Analysis.Cues, t->Analysis.Cues, sizeof(RBCue) * newTrack->Analysis.CueCount);
                  }
              }

              for (uint32_t i = 0; i < t->Analysis.CueCount && i < 32; i++) {
                if (t->Analysis.Cues[i].ID >= 1 && t->Analysis.Cues[i].ID <= 8) {
                  newTrack->HotCues[newTrack->HotCuesCount].ID = t->Analysis.Cues[i].ID;
                  newTrack->HotCues[newTrack->HotCuesCount].Start =
                      t->Analysis.Cues[i].Time;
                  memcpy(newTrack->HotCues[newTrack->HotCuesCount].Color,
                         t->Analysis.Cues[i].Color, 3);
                  newTrack->HotCuesCount++;
                } else if (t->Analysis.Cues[i].ID == 0) {
                  newTrack->Cues[newTrack->CuesCount].Start = t->Analysis.Cues[i].Time;
                  memcpy(newTrack->Cues[newTrack->CuesCount].Color,
                         t->Analysis.Cues[i].Color, 3);
                  newTrack->CuesCount++;
                }
              }

              // Also copy Phrases to legacy array if needed for UI
              newTrack->PhraseCount = t->Analysis.PhraseCount > 64 ? 64 : t->Analysis.PhraseCount;
              for (int i = 0; i < newTrack->PhraseCount; i++) {
                  newTrack->Phrases[i].Index = t->Analysis.Phrases[i].Index;
                  newTrack->Phrases[i].Beat = t->Analysis.Phrases[i].Beat;
                  newTrack->Phrases[i].KindID = t->Analysis.Phrases[i].KindID;
                  strncpy(newTrack->Phrases[i].Kind, t->Analysis.Phrases[i].Kind, 31);
              }

              TrackState *oldTrack = targetDeck->LoadedTrack;
              targetDeck->LoadedTrack = newTrack; // Atomic pointer swap on UI thread

              // Cleanup old track memory (including buffers)
              if (oldTrack){
                if (oldTrack->Analysis.BeatGrid != NULL) free(oldTrack->Analysis.BeatGrid);
                if (oldTrack->Analysis.Cues != NULL) free(oldTrack->Analysis.Cues);
                if (oldTrack->Analysis.Phrases != NULL) free(oldTrack->Analysis.Phrases);
                if (oldTrack->Analysis.DynamicWaveform != NULL) free(oldTrack->Analysis.DynamicWaveform);
                free(oldTrack);
              }
                
              targetDeck->PositionMs = (newTrack->CuesCount > 0)
                                           ? newTrack->Cues[0].Start
                                           : (newTrack->Analysis.BeatGridCount > 0
                                                  ? newTrack->Analysis.BeatGrid[0].Time
                                                  : 0);
              DeckAudio_JumpToMs(&s->AudioPlugin->Decks[loadToDeck],
                                 (uint32_t)targetDeck->PositionMs);
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
            targetDeck->OriginalBPM = (t->BPM > 0) ? t->BPM : 120.0f;

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
                } else if (t->Cues[i].ID == 0) {
                  newTrack->Cues[newTrack->CuesCount].Start = t->Cues[i].Time;
                  memcpy(newTrack->Cues[newTrack->CuesCount].Color,
                         t->Cues[i].Color, 3);
                  newTrack->CuesCount++;
                }
              }

              TrackState *oldTrack = targetDeck->LoadedTrack;
              targetDeck->LoadedTrack = newTrack;
              if (oldTrack) {
                if (oldTrack->Analysis.BeatGrid != NULL) free(oldTrack->Analysis.BeatGrid);
                if (oldTrack->Analysis.Phrases != NULL) free(oldTrack->Analysis.Phrases);
                if (oldTrack->Analysis.Cues != NULL) free(oldTrack->Analysis.Cues);
                if (oldTrack->Analysis.DynamicWaveform != NULL) free(oldTrack->Analysis.DynamicWaveform);
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

// On-Screen Keyboard (OSK) Touch Rendering & Interaction
static void Browser_DrawOSK(BrowserState *s, Vector2 mPos) {
  if (!s->ShowOSK) return;

  float sidebarW = S(40);
  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  float oskH = S(210);
  float oskW = SCREEN_WIDTH - sidebarW;
  float oskX = sidebarW;
  float oskY = viewH - oskH;

  // Outer Overlay Background
  DrawRectangle((int)oskX, (int)oskY, (int)oskW, (int)oskH, (Color){ 14, 16, 22, 248 });
  DrawRectangleLinesEx((Rectangle){ oskX, oskY, oskW, oskH }, 1.5f, ColorBlue);

  // Header Title & Search Preview
  DrawRectangle((int)oskX, (int)oskY, (int)oskW, (int)S(32), (Color){ 22, 26, 38, 255 });
  DrawLine((int)oskX, (int)(oskY + S(32)), (int)(oskX + oskW), (int)(oskY + S(32)), ColorDark1);

  Font faceXS = UIFonts_GetFace(S(9));
  Font faceSm = UIFonts_GetFace(S(12));
  Font faceMd = UIFonts_GetFace(S(14));
  Font faceIcon = UIFonts_GetIcon(S(6));

  // Icon + "SEARCH"
  UIDrawText("\uf11c", faceIcon, oskX + S(10), oskY + S(10), S(12), ColorOrange);
  UIDrawText("SEARCH", faceSm, oskX + S(28), oskY + S(9), S(12), ColorOrange);

  // Current Search Query Box in Header
  float previewW = oskW - S(160);
  Rectangle prevRect = { oskX + S(85), oskY + S(4), previewW, S(24) };
  DrawRectangleRec(prevRect, ColorBlack);
  DrawRectangleLinesEx(prevRect, 1.0f, ColorDark1);

  char searchDisplay[128];
  if (strlen(s->SearchQuery) > 0) {
      snprintf(searchDisplay, sizeof(searchDisplay), "%s", s->SearchQuery);
  } else {
      snprintf(searchDisplay, sizeof(searchDisplay), "Type to search tracks...");
  }
  if ((int)(GetTime() * 2) % 2 == 0) {
      strncat(searchDisplay, "|", sizeof(searchDisplay) - strlen(searchDisplay) - 1);
  }
  UIDrawText(searchDisplay, faceXS, prevRect.x + S(6), prevRect.y + S(6), S(11), strlen(s->SearchQuery) > 0 ? ColorWhite : ColorShadow);

  // Close Button on top-right of Header
  Rectangle closeBtn = { oskX + oskW - S(60), oskY + S(4), S(52), S(24) };
  bool hoverClose = CheckCollisionPointRec(mPos, closeBtn);
  DrawRectangleRec(closeBtn, hoverClose ? ColorRed : ColorDark2);
  DrawRectangleLinesEx(closeBtn, 1.0f, ColorShadow);
  DrawCentredText("HIDE", faceXS, closeBtn.x, closeBtn.width, closeBtn.y + S(7), S(10), ColorWhite);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverClose) {
      s->ShowOSK = false;
      s->IsSearching = (strlen(s->SearchQuery) > 0);
      return;
  }

  // Key Definitions
  const char *row1_letters = "QWERTYUIOP";
  const char *row1_symbols = "1234567890";
  
  const char *row2_letters = "ASDFGHJKL";
  const char *row2_symbols = "-/:;()$&@\"";
  
  const char *row3_letters = "ZXCVBNM";
  const char *row3_symbols = ".,?!'#%*";

  float startY = oskY + S(38);
  float keyH = S(36);
  float gap = S(4);
  float availW = oskW - S(16);

  // ROW 1 (10 Keys)
  int count1 = 10;
  float kw1 = (availW - (count1 - 1) * gap) / (float)count1;
  for (int i = 0; i < count1; i++) {
      char ch = (s->OSKMode == 0) ? row1_letters[i] : row1_symbols[i];
      if (s->OSKMode == 0 && !s->OSKShiftActive) ch = (char)tolower((unsigned char)ch);

      Rectangle kRect = { oskX + S(8) + i * (kw1 + gap), startY, kw1, keyH };
      bool isHover = CheckCollisionPointRec(mPos, kRect);
      DrawRectangleRec(kRect, isHover ? ColorBlue : ColorDark2);
      DrawRectangleLinesEx(kRect, 1.0f, isHover ? ColorWhite : ColorDark1);

      char label[2] = { ch, '\0' };
      DrawCentredText(label, faceMd, kRect.x, kRect.width, kRect.y + S(10), S(14), ColorWhite);

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isHover) {
          if (strlen(s->SearchQuery) < 63) {
              int len = strlen(s->SearchQuery);
              s->SearchQuery[len] = ch;
              s->SearchQuery[len + 1] = '\0';
              s->CursorPos = s->ScrollOffset = 0;
              s->VisualScroll = 0;
              Browser_UpdateActiveTracks(s);
          }
      }
  }

  // ROW 2 (9-10 Keys)
  float startY2 = startY + keyH + gap;
  int count2 = (s->OSKMode == 0) ? 9 : 10;
  float kw2 = (availW - (count2 - 1) * gap) / (float)count2;
  float offset2 = (s->OSKMode == 0) ? (kw2 * 0.5f) : 0;

  for (int i = 0; i < count2; i++) {
      char ch = (s->OSKMode == 0) ? row2_letters[i] : row2_symbols[i];
      if (s->OSKMode == 0 && !s->OSKShiftActive) ch = (char)tolower((unsigned char)ch);

      Rectangle kRect = { oskX + S(8) + offset2 + i * (kw2 + gap), startY2, kw2, keyH };
      bool isHover = CheckCollisionPointRec(mPos, kRect);
      DrawRectangleRec(kRect, isHover ? ColorBlue : ColorDark2);
      DrawRectangleLinesEx(kRect, 1.0f, isHover ? ColorWhite : ColorDark1);

      char label[2] = { ch, '\0' };
      DrawCentredText(label, faceMd, kRect.x, kRect.width, kRect.y + S(10), S(14), ColorWhite);

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isHover) {
          if (strlen(s->SearchQuery) < 63) {
              int len = strlen(s->SearchQuery);
              s->SearchQuery[len] = ch;
              s->SearchQuery[len + 1] = '\0';
              s->CursorPos = s->ScrollOffset = 0;
              s->VisualScroll = 0;
              Browser_UpdateActiveTracks(s);
          }
      }
  }

  // ROW 3: SHIFT | KEYS | BKSP
  float startY3 = startY2 + keyH + gap;
  float kwShift = S(50);
  float kwBksp = S(55);
  int count3 = (s->OSKMode == 0) ? 7 : 8;
  float middleAvailW = availW - kwShift - kwBksp - 2 * gap;
  float kw3 = (middleAvailW - (count3 - 1) * gap) / (float)count3;

  // SHIFT KEY
  Rectangle shiftRect = { oskX + S(8), startY3, kwShift, keyH };
  bool hoverShift = CheckCollisionPointRec(mPos, shiftRect);
  Color shiftBg = s->OSKShiftActive ? ColorOrange : (hoverShift ? ColorBlue : ColorDark2);
  DrawRectangleRec(shiftRect, shiftBg);
  DrawRectangleLinesEx(shiftRect, 1.0f, ColorDark1);
  DrawCentredText("SHIFT", faceXS, shiftRect.x, shiftRect.width, shiftRect.y + S(12), S(10), ColorWhite);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverShift) {
      s->OSKShiftActive = !s->OSKShiftActive;
  }

  // ROW 3 LETTERS / SYMBOLS
  for (int i = 0; i < count3; i++) {
      char ch = (s->OSKMode == 0) ? row3_letters[i] : row3_symbols[i];
      if (s->OSKMode == 0 && !s->OSKShiftActive) ch = (char)tolower((unsigned char)ch);

      Rectangle kRect = { oskX + S(8) + kwShift + gap + i * (kw3 + gap), startY3, kw3, keyH };
      bool isHover = CheckCollisionPointRec(mPos, kRect);
      DrawRectangleRec(kRect, isHover ? ColorBlue : ColorDark2);
      DrawRectangleLinesEx(kRect, 1.0f, isHover ? ColorWhite : ColorDark1);

      char label[2] = { ch, '\0' };
      DrawCentredText(label, faceMd, kRect.x, kRect.width, kRect.y + S(10), S(14), ColorWhite);

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isHover) {
          if (strlen(s->SearchQuery) < 63) {
              int len = strlen(s->SearchQuery);
              s->SearchQuery[len] = ch;
              s->SearchQuery[len + 1] = '\0';
              s->CursorPos = s->ScrollOffset = 0;
              s->VisualScroll = 0;
              Browser_UpdateActiveTracks(s);
          }
      }
  }

  // BKSP KEY
  Rectangle bkspRect = { oskX + S(8) + oskW - S(16) - kwBksp, startY3, kwBksp, keyH };
  bool hoverBksp = CheckCollisionPointRec(mPos, bkspRect);
  DrawRectangleRec(bkspRect, hoverBksp ? ColorRed : ColorDark2);
  DrawRectangleLinesEx(bkspRect, 1.0f, ColorDark1);
  DrawCentredText("BKSP", faceXS, bkspRect.x, bkspRect.width, bkspRect.y + S(12), S(10), ColorWhite);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverBksp) {
      int len = strlen(s->SearchQuery);
      if (len > 0) {
          s->SearchQuery[len - 1] = '\0';
          s->CursorPos = s->ScrollOffset = 0;
          s->VisualScroll = 0;
          Browser_UpdateActiveTracks(s);
      }
  }

  // ROW 4: ?123/ABC | SPACE | CLEAR | SEARCH
  float startY4 = startY3 + keyH + gap;
  float kwMode = S(65);
  float kwClear = S(65);
  float kwSearch = S(80);
  float kwSpace = availW - kwMode - kwClear - kwSearch - 3 * gap;

  // MODE KEY (?123 / ABC)
  Rectangle modeRect = { oskX + S(8), startY4, kwMode, keyH };
  bool hoverMode = CheckCollisionPointRec(mPos, modeRect);
  DrawRectangleRec(modeRect, hoverMode ? ColorBlue : ColorDark2);
  DrawRectangleLinesEx(modeRect, 1.0f, ColorDark1);
  DrawCentredText((s->OSKMode == 0) ? "?123" : "ABC", faceSm, modeRect.x, modeRect.width, modeRect.y + S(11), S(12), ColorOrange);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverMode) {
      s->OSKMode = (s->OSKMode == 0) ? 1 : 0;
  }

  // SPACE BAR
  Rectangle spaceRect = { oskX + S(8) + kwMode + gap, startY4, kwSpace, keyH };
  bool hoverSpace = CheckCollisionPointRec(mPos, spaceRect);
  DrawRectangleRec(spaceRect, hoverSpace ? ColorBlue : ColorDark2);
  DrawRectangleLinesEx(spaceRect, 1.0f, ColorDark1);
  DrawCentredText("SPACE", faceXS, spaceRect.x, spaceRect.width, spaceRect.y + S(12), S(10), ColorShadow);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverSpace) {
      if (strlen(s->SearchQuery) < 63) {
          int len = strlen(s->SearchQuery);
          s->SearchQuery[len] = ' ';
          s->SearchQuery[len + 1] = '\0';
          s->CursorPos = s->ScrollOffset = 0;
          s->VisualScroll = 0;
          Browser_UpdateActiveTracks(s);
      }
  }

  // CLEAR KEY
  Rectangle clearRect = { spaceRect.x + kwSpace + gap, startY4, kwClear, keyH };
  bool hoverClear = CheckCollisionPointRec(mPos, clearRect);
  DrawRectangleRec(clearRect, hoverClear ? ColorRed : ColorDark2);
  DrawRectangleLinesEx(clearRect, 1.0f, ColorDark1);
  DrawCentredText("CLEAR", faceXS, clearRect.x, clearRect.width, clearRect.y + S(12), S(10), ColorWhite);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverClear) {
      s->SearchQuery[0] = '\0';
      s->CursorPos = s->ScrollOffset = 0;
      s->VisualScroll = 0;
      Browser_UpdateActiveTracks(s);
  }

  // SEARCH / DONE KEY
  Rectangle searchBtnRect = { clearRect.x + kwClear + gap, startY4, kwSearch, keyH };
  bool hoverSearch = CheckCollisionPointRec(mPos, searchBtnRect);
  DrawRectangleRec(searchBtnRect, hoverSearch ? ColorOrange : ColorBlue);
  DrawRectangleLinesEx(searchBtnRect, 1.0f, ColorWhite);
  DrawCentredText("SEARCH", faceSm, searchBtnRect.x, searchBtnRect.width, searchBtnRect.y + S(11), S(12), ColorWhite);

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverSearch) {
      s->ShowOSK = false;
      s->IsSearching = (strlen(s->SearchQuery) > 0);
  }
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

  // Draw Search Box, OSK Button & Sort Button
  if (s->BrowseLevel == 0) {
    totalVisible = 9;
    float sortButtonW = S(65);
    float oskButtonW = S(36);
    Rectangle searchBoxRect = {listX, listYOffset, listW - sortButtonW - oskButtonW - S(8), rowH};
    Rectangle oskButtonRect = {listX + listW - sortButtonW - oskButtonW - S(4), listYOffset, oskButtonW, rowH};
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

    // Draw OSK Button (Keyboard Icon)
    bool isOskHover = CheckCollisionPointRec(mPos, oskButtonRect);
    Color oskBg = (s->ShowOSK || isOskHover) ? ColorBlue : ColorDark2;
    DrawRectangleRec(oskButtonRect, oskBg);
    DrawRectangleLinesEx(oskButtonRect, 1.0f, s->ShowOSK ? ColorWhite : ColorDark1);
    UIDrawText("\uf11c", faceIcon, oskButtonRect.x + S(11), oskButtonRect.y + S(8), S(12), ColorWhite);

    // Draw Sort Button
    DrawRectangleRec(sortButtonRect, s->ShowSortDropdown ? ColorDark1 : ColorDark2);
    DrawRectangleLinesEx(sortButtonRect, 1.0f, ColorDark1);
    const char* sortLabels[] = {"Default", "BPM", "Key", "Title", "Rating"};
    UIDrawText(sortLabels[s->SortMode], faceXS, sortButtonRect.x + S(6), sortButtonRect.y + S(9), S(9), ColorWhite);
    UIDrawText("\uf0d7", faceIcon, sortButtonRect.x + sortButtonW - S(14), sortButtonRect.y + S(10), S(9), ColorShadow);
  }

  int totalItems = 0;
  if (s->BrowseLevel == 0)
    totalItems = s->ActiveTrackCount;
  else if (s->BrowseLevel == 1) {
    if (s->DatabaseType == 0)
      totalItems = s->DB ? (int)s->DB->PlaylistCount : 0;
    else
      totalItems = s->SeratoDB ? (int)s->SeratoDB->PlaylistCount : 0;
  } else if (s->BrowseLevel == 2)
    totalItems = 5 + (s->HasBothDatabases ? 1 : 0);
  else if (s->BrowseLevel == 3)
    totalItems = s->StorageCount;

  float listAreaH = (viewH - listYOffset);
  BeginScissorMode((int)listX, (int)listYOffset, (int)listW + S(10), (int)listAreaH);

  float pixelOffset = fmodf(s->VisualScroll, rowH);
  int itemsToDraw = totalVisible + 1;

  for (int i = 0; i < itemsToDraw; i++) {
    int idx = s->ScrollOffset + i;
    if (idx < 0 || idx >= totalItems) continue;

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

    float ry = listYOffset - pixelOffset + i * rowH;
    bool isCursor = (idx == s->CursorPos + s->ScrollOffset);

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

    // Marquee Logic for Title
    static int lastCursor = -1;
    if (s->CursorPos != lastCursor) {
        s->MarqueeScrollX = 0;
        lastCursor = s->CursorPos;
    }
    if (isCursor) s->MarqueeScrollX += GetFrameTime();

    float maxTitleW = listW - (textX - listX) - S(130);
    Rectangle titleRect = { textX, textY, maxTitleW, rowH };
    UIDrawScrollingText(title, faceSm, titleRect, S(13), ColorWhite, isCursor ? s->MarqueeScrollX : 0.0f);

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
  EndScissorMode();

  // Modern Touch-Friendly Scrollbar
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

  if (maxItems > totalVisible) {
    float sbTrackW = S(28);
    float sbTrackX = SCREEN_WIDTH - sbTrackW;
    float sbTrackY = TOP_BAR_H;
    float sbTrackH = viewH - TOP_BAR_H;

    // Background Track
    DrawRectangle(sbTrackX, sbTrackY, sbTrackW, sbTrackH, (Color){ 20, 20, 20, 180 });
    DrawLine(sbTrackX, sbTrackY, sbTrackX, sbTrackY + sbTrackH, ColorDark1);

    // Calculate handle height and Y position
    float maxScroll = (maxItems - totalVisible) * rowH;
    float handleH = (sbTrackH * totalVisible) / (float)maxItems;
    if (handleH < S(35)) handleH = S(35);
    if (handleH > sbTrackH) handleH = sbTrackH;

    float scrollRatio = (maxScroll > 0) ? (s->VisualScroll / maxScroll) : 0.0f;
    if (scrollRatio < 0.0f) scrollRatio = 0.0f;
    if (scrollRatio > 1.0f) scrollRatio = 1.0f;
    float handleY = sbTrackY + scrollRatio * (sbTrackH - handleH);

    Color handleColor = s->IsScrollbarDragging ? ColorOrange : (s->IsDragging ? ColorWhite : (Color){ 160, 160, 160, 200 });
    DrawRectangleRounded((Rectangle){ sbTrackX + S(6), handleY, sbTrackW - S(12), handleH }, 0.5f, 4, handleColor);
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

  // Render On-Screen Keyboard (OSK) Overlay on top
  Browser_DrawOSK(s, mPos);
}

void BrowserRenderer_Init(BrowserRenderer *r, BrowserState *state) {
  r->base.Update = Browser_Update;
  r->base.Draw = Browser_Draw;
  r->State = state;
}
