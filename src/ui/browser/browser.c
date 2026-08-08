#include "ui/browser/browser.h"
#include "core/memory_guard.h"
#include "core/logger.h"
#include "core/logic/control_object.h"
#include "audio/engine.h"
#include "rlgl.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/player/player_state.h"
#include "library/hotcue_db.h"
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

// Camelot Wheel Key Representation and Harmonic Compatibility Parser
typedef struct {
    int Number;   // 1 to 12, or 0 if invalid
    char Letter;  // 'A' (Minor) or 'B' (Major)
} CamelotKey;

static CamelotKey ParseCamelotKey(const char *rawKey) {
    CamelotKey ck = {0, 0};
    if (!rawKey || rawKey[0] == '\0') return ck;

    // 1. Direct Camelot notation check (e.g. "8A", "12B", "08A")
    int num = 0;
    char letter = 0;
    if (sscanf(rawKey, "%d%c", &num, &letter) == 2 && num >= 1 && num <= 12 &&
        (letter == 'A' || letter == 'a' || letter == 'B' || letter == 'b')) {
        ck.Number = num;
        ck.Letter = (letter >= 'a') ? (letter - 32) : letter;
        return ck;
    }

    // 2. Musical notation parsing
    char buf[16];
    strncpy(buf, rawKey, 15);
    buf[15] = '\0';

    bool isMinor = false;
    size_t len = strlen(buf);
    if (len > 1 && (buf[len - 1] == 'm' || buf[len - 1] == 'M')) {
        isMinor = true;
        buf[len - 1] = '\0';
    }

    if (isMinor) {
        ck.Letter = 'A';
        if (strcmp(buf, "Ab") == 0 || strcmp(buf, "G#") == 0) ck.Number = 1;
        else if (strcmp(buf, "Eb") == 0 || strcmp(buf, "D#") == 0) ck.Number = 2;
        else if (strcmp(buf, "Bb") == 0 || strcmp(buf, "A#") == 0) ck.Number = 3;
        else if (strcmp(buf, "F") == 0) ck.Number = 4;
        else if (strcmp(buf, "C") == 0) ck.Number = 5;
        else if (strcmp(buf, "G") == 0) ck.Number = 6;
        else if (strcmp(buf, "D") == 0) ck.Number = 7;
        else if (strcmp(buf, "A") == 0) ck.Number = 8;
        else if (strcmp(buf, "E") == 0) ck.Number = 9;
        else if (strcmp(buf, "B") == 0) ck.Number = 10;
        else if (strcmp(buf, "F#") == 0 || strcmp(buf, "Gb") == 0) ck.Number = 11;
        else if (strcmp(buf, "C#") == 0 || strcmp(buf, "Db") == 0) ck.Number = 12;
    } else {
        ck.Letter = 'B';
        if (strcmp(buf, "B") == 0 || strcmp(buf, "Cb") == 0) ck.Number = 1;
        else if (strcmp(buf, "F#") == 0 || strcmp(buf, "Gb") == 0) ck.Number = 2;
        else if (strcmp(buf, "Db") == 0 || strcmp(buf, "C#") == 0) ck.Number = 3;
        else if (strcmp(buf, "Ab") == 0 || strcmp(buf, "G#") == 0) ck.Number = 4;
        else if (strcmp(buf, "Eb") == 0 || strcmp(buf, "D#") == 0) ck.Number = 5;
        else if (strcmp(buf, "Bb") == 0 || strcmp(buf, "A#") == 0) ck.Number = 6;
        else if (strcmp(buf, "F") == 0) ck.Number = 7;
        else if (strcmp(buf, "C") == 0) ck.Number = 8;
        else if (strcmp(buf, "G") == 0) ck.Number = 9;
        else if (strcmp(buf, "D") == 0) ck.Number = 10;
        else if (strcmp(buf, "A") == 0) ck.Number = 11;
        else if (strcmp(buf, "E") == 0) ck.Number = 12;
    }

    return ck;
}

// Rekordbox / Mixed In Key Traffic Light Harmonic Mixing Match Level
// 2 = Perfect Harmonic Match (Same key, Relative Major/Minor, +1/-1 Semitone)
// 1 = Energy Shift / Semi-compatible Match
// 0 = Incompatible
static int GetCamelotHarmonicMatchLevel(const char *keyTrackStr, const char *keyMasterStr) {
    if (!keyTrackStr || !keyMasterStr || keyTrackStr[0] == '\0' || keyMasterStr[0] == '\0') return 0;
    
    CamelotKey tKey = ParseCamelotKey(keyTrackStr);
    CamelotKey mKey = ParseCamelotKey(keyMasterStr);

    if (tKey.Number == 0 || mKey.Number == 0) return 0;

    // 1. Same Key (e.g. 8A & 8A)
    if (tKey.Number == mKey.Number && tKey.Letter == mKey.Letter) return 2;

    // 2. Relative Major / Minor (e.g. 8A & 8B)
    if (tKey.Number == mKey.Number && tKey.Letter != mKey.Letter) return 2;

    // 3. +1 Step (Dominant) (e.g. 8A & 9A)
    int plus1 = (mKey.Number % 12) + 1;
    if (tKey.Number == plus1 && tKey.Letter == mKey.Letter) return 2;

    // 4. -1 Step (Subdominant) (e.g. 8A & 7A)
    int minus1 = (mKey.Number == 1) ? 12 : (mKey.Number - 1);
    if (tKey.Number == minus1 && tKey.Letter == mKey.Letter) return 2;

    // 5. Energy Shift / Semi-compatible Match (+1/-1 Step with letter flip or +2 steps)
    if (tKey.Number == plus1 && tKey.Letter != mKey.Letter) return 1;
    if (tKey.Number == minus1 && tKey.Letter != mKey.Letter) return 1;
    int plus2 = ((mKey.Number + 1) % 12) + 1;
    if (tKey.Number == plus2 && tKey.Letter == mKey.Letter) return 1;

    return 0;
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
    CamelotKey ka = ParseCamelotKey(ta->Key);
    CamelotKey kb = ParseCamelotKey(tb->Key);
    if (ka.Number != kb.Number) return ka.Number - kb.Number;
    return ka.Letter - kb.Letter;
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
    CamelotKey ka = ParseCamelotKey(ta->Key);
    CamelotKey kb = ParseCamelotKey(tb->Key);
    if (ka.Number != kb.Number) return ka.Number - kb.Number;
    return ka.Letter - kb.Letter;
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
    (void)a;
    (void)b;
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

  // Reverse if DESC order
  if (!s->SortAscending && s->ActiveTrackCount > 1) {
      if (s->DatabaseType == 0 && s->TrackPointers) {
          for (int i = 0; i < s->ActiveTrackCount / 2; i++) {
              RBTrack *tmp = s->TrackPointers[i];
              s->TrackPointers[i] = s->TrackPointers[s->ActiveTrackCount - 1 - i];
              s->TrackPointers[s->ActiveTrackCount - 1 - i] = tmp;
          }
      } else if (s->DatabaseType == 1 && s->SeratoTrackPointers) {
          for (int i = 0; i < s->ActiveTrackCount / 2; i++) {
              SeratoTrack *tmp = s->SeratoTrackPointers[i];
              s->SeratoTrackPointers[i] = s->SeratoTrackPointers[s->ActiveTrackCount - 1 - i];
              s->SeratoTrackPointers[s->ActiveTrackCount - 1 - i] = tmp;
          }
      }
  }
}

void Browser_Back(BrowserState *s) {
  if (!s || !s->IsActive) return;

  if (s->ShowLoadPopup) {
    s->ShowLoadPopup = false;
    return;
  }
  if (s->ShowOSK || s->IsSearching) {
    s->ShowOSK = false;
    s->IsSearching = false;
    s->SearchQuery[0] = '\0';
    s->CursorPos = s->ScrollOffset = 0;
    s->VisualScroll = 0.0f;
    Browser_UpdateActiveTracks(s);
    return;
  }
  if (s->IsTagList) {
    s->IsTagList = false;
    return;
  }

  if (s->BrowseLevel == 0) {
    if (s->CurrentPlaylistIdx >= 0) {
      s->CurrentPlaylistIdx = -1;
      s->BrowseLevel = 1; // Return to Playlists List
    } else {
      s->BrowseLevel = 2; // Return to Categories
    }
    s->CursorPos = s->ScrollOffset = 0;
    s->VisualScroll = 0.0f;
    Browser_UpdateActiveTracks(s);
  } else if (s->BrowseLevel == 1) {
    s->BrowseLevel = 2; // Return to Categories
    s->CursorPos = s->ScrollOffset = 0;
    s->VisualScroll = 0.0f;
    Browser_UpdateActiveTracks(s);
  } else if (s->BrowseLevel == 2) {
    if (s->StorageCount > 1) {
      s->BrowseLevel = 3; // Return to Storage Selection (USB)
    }
    s->CursorPos = s->ScrollOffset = 0;
    s->VisualScroll = 0.0f;
  } else if (s->BrowseLevel == 3) {
    s->IsActive = false; // Exit browser -> Return to Player
  }
}

void Browser_CheckStorageConnection(BrowserState *s) {
  if (!s || !s->SelectedStorage) return;

  // Only perform disconnection check if user has selected a storage and is actively inside it (BrowseLevel < 3)
  if (s->BrowseLevel >= 3) return;

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

#if defined(__linux__) && !defined(__ANDROID__)
static void Linux_AutoMountUSBStorages(void) {
    static double lastMountCheck = 0;
    double now = GetTime();
    if (now - lastMountCheck < 5.0) return;
    lastMountCheck = now;

    char mountsContent[4096] = "";
    FILE *fmounts = fopen("/proc/mounts", "r");
    if (fmounts) {
        size_t len = fread(mountsContent, 1, sizeof(mountsContent) - 1, fmounts);
        mountsContent[len] = '\0';
        fclose(fmounts);
    }

    DIR *ddev = opendir("/dev");
    if (ddev) {
        struct dirent *ent;
        while ((ent = readdir(ddev)) != NULL) {
            if ((strncmp(ent->d_name, "sd", 2) == 0 && strlen(ent->d_name) >= 4) ||
                (strncmp(ent->d_name, "mmcblk", 6) == 0 && strstr(ent->d_name, "p"))) {
                
                char devPath[128];
                snprintf(devPath, sizeof(devPath), "/dev/%s", ent->d_name);
                
                if (strstr(mountsContent, devPath) == NULL) {
                    char mountPoint[128];
                    snprintf(mountPoint, sizeof(mountPoint), "/media/%s", ent->d_name);
                    
                    char cmd[512];
                    snprintf(cmd, sizeof(cmd),
                             "mkdir -p %s 2>/dev/null && "
                             "(mount -o rw,relatime,fmask=0022,dmask=0022,codepage=437,iocharset=utf8,utf8 %s %s 2>/dev/null || "
                             " mount %s %s 2>/dev/null || true)",
                             mountPoint, devPath, mountPoint, devPath, mountPoint);
                    system(cmd);
                }
            }
        }
        closedir(ddev);
    }
}

static bool IsValidStorageDir(const char *fullPath) {
    struct stat st;
    if (stat(fullPath, &st) != 0 || !S_ISDIR(st.st_mode) || access(fullPath, R_OK) != 0) {
        return false;
    }

    FILE *f = fopen("/proc/mounts", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, fullPath)) {
                fclose(f);
                return true;
            }
        }
        fclose(f);
    }

    DIR *d = opendir(fullPath);
    if (d) {
        struct dirent *ent;
        bool hasFiles = false;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] != '.') {
                hasFiles = true;
                break;
            }
        }
        closedir(d);
        if (hasFiles) return true;
    }

    return false;
}
#endif

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
  if (stat("/storage/emulated/0", &st) == 0) {
    strcpy(s->AvailableStorages[s->StorageCount].Name, "Internal Storage");
    strcpy(s->AvailableStorages[s->StorageCount].Path, "/storage/emulated/0");
    strcpy(s->AvailableStorages[s->StorageCount].Type, "Internal");
    s->StorageCount++;
  }

#ifdef PLATFORM_IOS
  extern const char *ios_get_documents_path(const char *filename);
  const char *scanDirs[] = {"DOCUMENTS_DIR", "/var/mobile/Media",
                            "/storage",      "/mnt",
                            "/media",        "/run/media",
                            "/media/root",   "/run/media/root"};
  int scanDirCount = 8;
#else
  const char *scanDirs[] = {"/storage", "/mnt", "/media", "/run/media", "/media/root", "/run/media/root"};
  int scanDirCount = 6;
#endif

#if defined(__linux__) && !defined(__ANDROID__)
  Linux_AutoMountUSBStorages();
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

        // Skip system folders, internal mount points, and system partitions
        bool skip = false;
        const char* toSkip[] = {"self", "emulated", "knox-emulated", "container", "secure", "asec", "obb", "runtime", "appfuse", "shared", "user", "media_rw", "temp", "expand", "legacy", "boot", "root", "system", "etc", "usr", "var", "dev", "proc", "sys"};
        for(int k=0; k<24; k++) {
            if(strcmp(dir->d_name, toSkip[k]) == 0) { skip = true; break; }
        }
        if (strncmp(dir->d_name, "nand-sata", 9) == 0 ||
            strncmp(dir->d_name, "mmcblk", 6) == 0) {
            skip = true;
        }
        if (skip) continue;

        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirToScan, dir->d_name);

#if defined(__linux__) && !defined(__ANDROID__)
        if (!IsValidStorageDir(fullPath)) continue;
#else
        struct stat st_dir;
        if (stat(fullPath, &st_dir) != 0 || !S_ISDIR(st_dir.st_mode) || access(fullPath, R_OK) != 0) continue;
#endif

        printf("[BROWSER] Scanning potential storage: %s\n", fullPath);

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

        snprintf(dbPath, sizeof(dbPath), "%s/PIONEER/rekordbox/export.pdb", fullPath);
        if (stat(dbPath, &st) == 0) hasRB = true;
        snprintf(dbPath, sizeof(dbPath), "%s/PIONEER/Rekordbox/export.pdb", fullPath);
        if (stat(dbPath, &st) == 0) hasRB = true;
        snprintf(dbPath, sizeof(dbPath), "%s/PIONEER/REKORDBOX/export.pdb", fullPath);
        if (stat(dbPath, &st) == 0) hasRB = true;
        snprintf(dbPath, sizeof(dbPath), "%s/pioneer/rekordbox/export.pdb", fullPath);
        if (stat(dbPath, &st) == 0) hasRB = true;

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

        // Display name formatting (e.g. sda1 -> USB1)
        char displayName[64];
        if (strncmp(dir->d_name, "sd", 2) == 0 && strlen(dir->d_name) >= 3) {
            int driveNum = dir->d_name[2] - 'a' + 1;
            if (driveNum >= 1 && driveNum <= 9) {
                snprintf(displayName, sizeof(displayName), "USB%d (%s)", driveNum, type);
            } else {
                snprintf(displayName, sizeof(displayName), "USB %s (%s)", dir->d_name, type);
            }
        } else {
            snprintf(displayName, sizeof(displayName), "%s (%s)", dir->d_name, type);
        }

        snprintf(s->AvailableStorages[s->StorageCount].Name,
                 sizeof(s->AvailableStorages[0].Name), "%s", displayName);
        snprintf(s->AvailableStorages[s->StorageCount].Path,
                 sizeof(s->AvailableStorages[0].Path), "%s", fullPath);
        strcpy(s->AvailableStorages[s->StorageCount].Type, type);
        s->StorageCount++;
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
  static double lastBrowserListTapTime = 0.0;
  static double lastTrackTapTime = 0.0;
  static int lastTrackTapIdx = -1;

  // Layout Variables
  Vector2 mousePos = UIGetMousePosition();
  float sidebarW = S(40);
  float rowH = S(28.0f);
  float sbTrackW = S(14);
  float listW = SCREEN_WIDTH - sidebarW - sbTrackW - S(4);
  if (s->InfoEnabled) listW = SCREEN_WIDTH - sidebarW - S(160);

  // Dropdown and Search Box Interaction
  if (s->BrowseLevel == 0) {
    float oskButtonW = S(36);
    Rectangle searchBoxRect = {sidebarW, TOP_BAR_H, listW - oskButtonW - S(4), rowH};
    Rectangle oskButtonRect = {sidebarW + listW - oskButtonW, TOP_BAR_H, oskButtonW, rowH};

    // Table Header Column Rectangles (Row 2)
    float headerH = S(24.0f);
    float headerY = TOP_BAR_H + rowH;
    Rectangle headTitleRect = {sidebarW, headerY, listW - S(110), headerH};
    Rectangle headBpmRect   = {sidebarW + listW - S(110), headerY, S(55), headerH};
    Rectangle headKeyRect   = {sidebarW + listW - S(55), headerY, S(55), headerH};

    // Handle OSK Panel Touch Absorption
    if (s->ShowOSK) {
      float viewH = SCREEN_HEIGHT - DECK_STR_H;
      float oskH = S(172);
      Rectangle oskPanelRect = {0, viewH - oskH, SCREEN_WIDTH, oskH};
      if (CheckCollisionPointRec(mousePos, oskPanelRect)) {
        if (UI_IsPressed()) {
          // Touch absorbed by OSK overlay
        }
      }
    }

    if (UI_IsReleased() && !s->IsDragging) {
      double now = GetTime();
      if ((now - lastBrowserListTapTime) >= 0.18) {
        // Handle OSK Button Toggle
        if (CheckCollisionPointRec(mousePos, oskButtonRect)) {
          lastBrowserListTapTime = now;
          s->ShowOSK = !s->ShowOSK;
          s->IsSearching = s->ShowOSK;
          return 0;
        }

        // Handle Search Box Focus
        if (CheckCollisionPointRec(mousePos, searchBoxRect)) {
          lastBrowserListTapTime = now;
          s->IsSearching = true;
          s->ShowOSK = true;
        } else if (!s->ShowOSK && s->IsSearching) {
          s->IsSearching = false;
        }

        // Handle Table Header Column Clicks (Sort ASC / DESC)
        if (CheckCollisionPointRec(mousePos, headBpmRect)) {
          lastBrowserListTapTime = now;
          if (s->SortMode == 1) {
            s->SortAscending = !s->SortAscending;
          } else {
            s->SortMode = 1; // BPM
            s->SortAscending = true;
          }
          s->CursorPos = s->ScrollOffset = 0;
          s->VisualScroll = 0;
          s->ScrollVelocity = 0;
          Browser_UpdateActiveTracks(s);
          return 0;
        }

        if (CheckCollisionPointRec(mousePos, headKeyRect)) {
          lastBrowserListTapTime = now;
          if (s->SortMode == 2) {
            s->SortAscending = !s->SortAscending;
          } else {
            s->SortMode = 2; // Key
            s->SortAscending = true;
          }
          s->CursorPos = s->ScrollOffset = 0;
          s->VisualScroll = 0;
          s->ScrollVelocity = 0;
          Browser_UpdateActiveTracks(s);
          return 0;
        }

        if (CheckCollisionPointRec(mousePos, headTitleRect)) {
          lastBrowserListTapTime = now;
          if (s->SortMode == 3 || s->SortMode == 0) {
            s->SortAscending = !s->SortAscending;
            s->SortMode = 3; // Title
          } else {
            s->SortMode = 3; // Title
            s->SortAscending = true;
          }
          s->CursorPos = s->ScrollOffset = 0;
          s->VisualScroll = 0;
          s->ScrollVelocity = 0;
          Browser_UpdateActiveTracks(s);
          return 0;
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
      bool isShift = (CO_GetValue("[Channel1]", "shift") > 0.5f) ||
                     (CO_GetValue("[Channel2]", "shift") > 0.5f) ||
                     (CO_GetValue("[Channel3]", "shift") > 0.5f) ||
                     (CO_GetValue("[Channel4]", "shift") > 0.5f);
      if (isShift) {
          CO_AddValue("[Master]", "waveform_zoom_step", (float)s->MidiBrowseDelta);
          s->MidiBrowseDelta = 0;
      }
  }
  
  if (s->MidiRequestEnter || s->MidiRequestMoveFocusForward) {
      triggerEnter = true;
      s->MidiRequestEnter = false;
      s->MidiRequestMoveFocusForward = false;
  }
  
  if (s->MidiRequestBack || s->MidiRequestMoveFocusBackward) {
      Browser_Back(s);
      s->MidiRequestBack = false;
      s->MidiRequestMoveFocusBackward = false;
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
  static double lastPopupOpenedTime = 0.0;
  bool wasPopupOpen = s->ShowLoadPopup;
  if (wasPopupOpen) {
    if ((GetTime() - lastPopupOpenedTime) >= 0.25 && UI_IsReleased()) {
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

  int totalVisible = (s->BrowseLevel == 0) ? 9 : 10;
  float listYOffset = TOP_BAR_H + ((s->BrowseLevel == 0) ? rowH : 0);

  // 1. Sidebar Clicking & Interaction
  for (int i = 0; i < 7; i++) {
    float boxY = TOP_BAR_H + i * sidebarW;
    Rectangle boxRect = {0, boxY, sidebarW, sidebarW};

    if (UICheckClick(boxRect)) {
        lastBrowserListTapTime = GetTime();
        s->TouchDragAccumulator = 999.0f;
        s->ShowOSK = false;
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
      if (s->IsDragging && UI_IsReleased()) {
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

  // Dynamic totalVisible and maxScroll adjustment for OSK
  float viewH = SCREEN_HEIGHT - DECK_STR_H - S(6.0f);
  listYOffset = TOP_BAR_H + ((s->BrowseLevel == 0) ? (rowH + S(24.0f)) : 0);
  float listAreaH = viewH - listYOffset;
  float oskH = S(172);
  if (s->ShowOSK) {
    listAreaH -= oskH;
  }
  if (listAreaH < S(50)) listAreaH = S(50);
  totalVisible = (int)floorf(listAreaH / rowH);
  if (totalVisible < 1) totalVisible = 1;

  if (s->MidiBrowseDelta != 0) {
      float itemRowH = S(28.0f);
      int delta = s->MidiBrowseDelta;
      s->MidiBrowseDelta = 0;
      if (delta > 0) {
          for (int i = 0; i < delta; i++) {
              if (s->CursorPos + s->ScrollOffset < totalItems - 1) {
                  if (s->CursorPos < totalVisible - 1) s->CursorPos++;
                  else s->ScrollOffset++;
              }
          }
      } else if (delta < 0) {
          for (int i = 0; i < -delta; i++) {
              if (s->CursorPos > 0) s->CursorPos--;
              else if (s->ScrollOffset > 0) s->ScrollOffset--;
          }
      }
      s->VisualScroll = (float)(s->ScrollOffset * itemRowH);
      s->ScrollVelocity = 0;
  }

  // Strict Bounds Clamping for Cursor and ScrollOffset
  if (totalItems > 0) {
    if (s->ScrollOffset > totalItems - 1) s->ScrollOffset = totalItems - 1;
    if (s->ScrollOffset < 0) s->ScrollOffset = 0;

    int maxCursor = totalItems - 1 - s->ScrollOffset;
    if (maxCursor > totalVisible - 1) maxCursor = totalVisible - 1;
    if (maxCursor < 0) maxCursor = 0;

    if (s->CursorPos > maxCursor) s->CursorPos = maxCursor;
    if (s->CursorPos < 0) s->CursorPos = 0;
  } else {
    s->CursorPos = 0;
    s->ScrollOffset = 0;
  }

  // 3. Touch Kinetic Scrolling & Interactive Scrollbar Logic
  if (!s->ShowLoadPopup) {
    float maxScroll = (totalItems - totalVisible) * rowH;
    if (maxScroll < 0) maxScroll = 0;

    float sbTrackX = SCREEN_WIDTH - sbTrackW;
    float sbTrackY = listYOffset;
    float sbTrackH = viewH - listYOffset;
    Rectangle sbRect = {sbTrackX, sbTrackY, sbTrackW, sbTrackH};

    if (UI_IsPressed()) {
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

    if (UI_IsDown()) {
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

        if (!s->IsDragging && s->TouchDragAccumulator > S(4.0f)) {
          s->IsDragging = true;
        }

        if (s->IsDragging || s->TouchDragAccumulator > S(2.0f)) {
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

    if (UI_IsReleased()) {
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
        if (UI_IsPressed()) {
          s->DraggingIdx = idx;
          if (s->BrowseLevel == 1) s->DraggingType = 1; 
          else if (s->BrowseLevel == 0) s->DraggingType = 0;
          else s->DraggingType = -1;
        }

        if (UI_IsReleased() && !s->IsDragging) {
          double now = GetTime();
          if (s->TouchDragAccumulator < S(28.0f) && fabsf(s->ScrollVelocity) < 60.0f) {
            s->ShowOSK = false; // Auto hide keyboard on list item tap
            if (s->CursorPos + s->ScrollOffset != idx) {
              s->CursorPos = idx - s->ScrollOffset;
              s->MarqueeScrollX = 0; 
            }
            if (s->BrowseLevel == 0) {
              // Double Tap on Track to open Load Option Popup
              double dt = now - lastTrackTapTime;
              if (idx == lastTrackTapIdx && dt <= 0.38 && dt >= 0.05) {
                s->ShowLoadPopup = true;
                lastPopupOpenedTime = now;
                s->PopupTrackIdx = idx;
                lastTrackTapTime = 0.0;
                lastTrackTapIdx = -1;
              } else {
                lastTrackTapTime = now;
                lastTrackTapIdx = idx;
                lastBrowserListTapTime = now;
              }
            } else if (!s->IsTagList) {
              if ((now - lastBrowserListTapTime) >= 0.14) {
                lastBrowserListTapTime = now;
                triggerEnter = true;
              }
            }
          }
        }
      }
    }
  }

  if (UI_IsReleased())
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
    lastBrowserListTapTime = GetTime();
    s->TouchDragAccumulator = 999.0f; // Consume touch drag distance to prevent re-triggering sub-level
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
      int catIdx = s->ScrollOffset + s->CursorPos;
      if (catIdx == 5 && s->HasBothDatabases) {
        // TOGGLE DATABASE
        s->DatabaseType = (s->DatabaseType == 0) ? 1 : 0;
        printf("[BROWSER] Switched database to %s\n",
               s->DatabaseType == 0 ? "Rekordbox" : "Serato");
        s->CurrentPlaylistIdx = -1; // Reset playlist selection on switch
        s->CursorPos = s->ScrollOffset = 0;
        s->VisualScroll = 0.0f;
        s->ScrollVelocity = 0.0f;
        Browser_UpdateActiveTracks(s);
      } else if (catIdx == 2) {
        s->BrowseLevel = 1; // Categories to Playlists
        s->CursorPos = s->ScrollOffset = 0;
        s->VisualScroll = 0.0f;
        s->ScrollVelocity = 0.0f;
      } else if (catIdx == 4) {
        s->IsSearching = true; // Categories to Search/OSK
        s->ShowOSK = true;
        s->CursorPos = s->ScrollOffset = 0;
        s->VisualScroll = 0.0f;
        s->ScrollVelocity = 0.0f;
      } else {
        s->BrowseLevel = 0; // Categories to Tracks (0: FILENAME, 1: FOLDER, 3: TRACK)
        s->CurrentPlaylistIdx = -1;
        s->CursorPos = s->ScrollOffset = 0;
        s->VisualScroll = 0.0f;
        s->ScrollVelocity = 0.0f;
        Browser_UpdateActiveTracks(s);
      }
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

          char trackFullPath[1024] = {0};
          const char *relPath = t->FilePath;
          if (relPath[0] == '/' || relPath[0] == '\\')
            relPath++;
          snprintf(trackFullPath, sizeof(trackFullPath), "%s/%s",
                   s->SelectedStorage->Path, relPath);

          if (s->AudioPlugin) {
            if (!DeckAudio_LoadTrack(&s->AudioPlugin->Decks[loadToDeck], trackFullPath)) {
              s->ShowLoadPopup = false;
              return 0;
            }
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

              strncpy(newTrack->FilePath, trackFullPath, sizeof(newTrack->FilePath) - 1);
              strncpy(newTrack->Title, t->Title, sizeof(newTrack->Title) - 1);
              strncpy(newTrack->Artist, t->Artist, sizeof(newTrack->Artist) - 1);

              // Check persistent HotCue database for overrides (Rekordbox)
              HotCue dbCues[8];
              int dbCount = 0;
              const char *storagePath = s->SelectedStorage ? s->SelectedStorage->Path : NULL;
              if (HotCueDB_GetTrack(storagePath, newTrack->FilePath, newTrack->Title, newTrack->Artist, dbCues, &dbCount)) {
                  newTrack->HotCuesCount = dbCount;
                  memset(newTrack->HotCues, 0, sizeof(newTrack->HotCues));
                  if (dbCount > 0) {
                      memcpy(newTrack->HotCues, dbCues, sizeof(HotCue) * dbCount);
                  }
                  
                  // Re-build Analysis.Cues to match persisted HotCues
                  if (newTrack->Analysis.Cues) free(newTrack->Analysis.Cues);
                  newTrack->Analysis.CueCount = dbCount;
                  if (dbCount > 0) {
                      newTrack->Analysis.Cues = (RBCue*)malloc(sizeof(RBCue) * dbCount);
                      if (newTrack->Analysis.Cues) {
                          for (int h = 0; h < dbCount; h++) {
                              memset(&newTrack->Analysis.Cues[h], 0, sizeof(RBCue));
                              newTrack->Analysis.Cues[h].Time = dbCues[h].Start;
                              newTrack->Analysis.Cues[h].ID = dbCues[h].ID;
                              newTrack->Analysis.Cues[h].Type = 1;
                              newTrack->Analysis.Cues[h].Status = 1;
                              memcpy(newTrack->Analysis.Cues[h].Color, dbCues[h].Color, 3);
                          }
                      }
                  } else {
                      newTrack->Analysis.Cues = NULL;
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
              targetDeck->LoadAnimTimer = 0.5f;
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

          char trackFullPath[1024] = {0};
          const char *relPath = t->FilePath;
          if (relPath[0] == '/' || relPath[0] == '\\')
            relPath++;
          snprintf(trackFullPath, sizeof(trackFullPath), "%s/%s",
                   s->SelectedStorage->Path, relPath);

          if (s->AudioPlugin) {
            if (!DeckAudio_LoadTrack(&s->AudioPlugin->Decks[loadToDeck], trackFullPath)) {
              s->ShowLoadPopup = false;
              return 0;
            }
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

              // Copy cues to Analysis.Cues for waveform and dynamic editing sync
              newTrack->Analysis.CueCount = t->CueCount;
              if (newTrack->Analysis.CueCount > 0 && t->Cues != NULL) {
                newTrack->Analysis.Cues = (RBCue*)malloc(sizeof(RBCue) * newTrack->Analysis.CueCount);
                if (newTrack->Analysis.Cues) {
                  memcpy(newTrack->Analysis.Cues, t->Cues, sizeof(RBCue) * newTrack->Analysis.CueCount);
                }
              }

              // Synthesize BeatGrid from BPM if not present so Quantize Snap works on Serato tracks
              if (newTrack->Analysis.BeatGridCount == 0 && targetDeck->OriginalBPM > 30.0f) {
                double beatMs = 60000.0 / (double)targetDeck->OriginalBPM;
                uint32_t totalBeats = (uint32_t)(3600000.0 / beatMs); // 1 hour of beats
                newTrack->Analysis.BeatGrid = (RBBeat*)malloc(sizeof(RBBeat) * totalBeats);
                if (newTrack->Analysis.BeatGrid) {
                  newTrack->Analysis.BeatGridCount = totalBeats;
                  for (uint32_t b = 0; b < totalBeats; b++) {
                    newTrack->Analysis.BeatGrid[b].Time = (uint32_t)(b * beatMs);
                    newTrack->Analysis.BeatGrid[b].BPM = (uint16_t)(targetDeck->OriginalBPM * 100.0f);
                    newTrack->Analysis.BeatGrid[b].BeatNumber = (b % 4) + 1;
                  }
                }
              }

              // Copy cues from Serato metadata to HotCues array
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

              strncpy(newTrack->FilePath, trackFullPath, sizeof(newTrack->FilePath) - 1);
              strncpy(newTrack->Title, t->Title, sizeof(newTrack->Title) - 1);
              strncpy(newTrack->Artist, t->Artist, sizeof(newTrack->Artist) - 1);

              // Check persistent HotCue database for overrides (Serato)
              HotCue dbCues[8];
              int dbCount = 0;
              const char *storagePath = s->SelectedStorage ? s->SelectedStorage->Path : NULL;
              if (HotCueDB_GetTrack(storagePath, newTrack->FilePath, newTrack->Title, newTrack->Artist, dbCues, &dbCount)) {
                  newTrack->HotCuesCount = dbCount;
                  memset(newTrack->HotCues, 0, sizeof(newTrack->HotCues));
                  if (dbCount > 0) {
                      memcpy(newTrack->HotCues, dbCues, sizeof(HotCue) * dbCount);
                  }
                  
                  // Re-build Analysis.Cues to match persisted HotCues
                  if (newTrack->Analysis.Cues) free(newTrack->Analysis.Cues);
                  newTrack->Analysis.CueCount = dbCount;
                  if (dbCount > 0) {
                      newTrack->Analysis.Cues = (RBCue*)malloc(sizeof(RBCue) * dbCount);
                      if (newTrack->Analysis.Cues) {
                          for (int h = 0; h < dbCount; h++) {
                              memset(&newTrack->Analysis.Cues[h], 0, sizeof(RBCue));
                              newTrack->Analysis.Cues[h].Time = dbCues[h].Start;
                              newTrack->Analysis.Cues[h].ID = dbCues[h].ID;
                              newTrack->Analysis.Cues[h].Type = 1;
                              newTrack->Analysis.Cues[h].Status = 1;
                              memcpy(newTrack->Analysis.Cues[h].Color, dbCues[h].Color, 3);
                          }
                      }
                  } else {
                      newTrack->Analysis.Cues = NULL;
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
              targetDeck->LoadAnimTimer = 0.5f;
              DeckAudio_JumpToMs(&s->AudioPlugin->Decks[loadToDeck],
                                 (uint32_t)targetDeck->PositionMs);
            }
          }
        }
      }
    }
  }

  if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE))) {
    Browser_Back(s);
  }

  return 0;
}

// On-Screen Keyboard (OSK) Touch Rendering & Interaction
static void Browser_DrawOSK(BrowserState *s, Vector2 mPos) {
  if (!s->ShowOSK) return;

  static double lastOskKeyPressTime = 0.0;
  double now = GetTime();
  bool canPressOSK = (now - lastOskKeyPressTime) >= 0.10;

  float viewH = SCREEN_HEIGHT - DECK_STR_H;
  float oskH = S(172);
  float oskW = SCREEN_WIDTH;
  float oskX = 0.0f;
  float oskY = viewH - oskH;

  // Outer Overlay Background & Sleek Top Accent Line
  DrawRectangle((int)oskX, (int)oskY, (int)oskW, (int)oskH, (Color){ 12, 14, 20, 252 });
  DrawRectangle((int)oskX, (int)oskY, (int)oskW, (int)S(2), ColorBlue);

  Font faceXS = UIFonts_GetFace(S(9));
  Font faceSm = UIFonts_GetFace(S(12));
  Font faceMd = UIFonts_GetFace(S(14));

  // Key Definitions
  const char *row1_letters = "QWERTYUIOP";
  const char *row1_symbols = "1234567890";
  
  const char *row2_letters = "ASDFGHJKL";
  const char *row2_symbols = "-/:;()$&@\"";
  
  const char *row3_letters = "ZXCVBNM";
  const char *row3_symbols = ".,?!'#%*";

  float startY = oskY + S(8);
  float keyH = S(36);
  float gap = S(4);
  float padX = S(8);
  float availW = oskW - 2 * padX;

  // ROW 1 (10 Keys)
  int count1 = 10;
  float kw1 = (availW - (count1 - 1) * gap) / (float)count1;
  for (int i = 0; i < count1; i++) {
      char ch = (s->OSKMode == 0) ? row1_letters[i] : row1_symbols[i];
      if (s->OSKMode == 0 && !s->OSKShiftActive) ch = (char)tolower((unsigned char)ch);

      Rectangle kRect = { oskX + padX + i * (kw1 + gap), startY, kw1, keyH };
      bool isHover = CheckCollisionPointRec(mPos, kRect);
      DrawRectangleRec(kRect, isHover ? ColorBlue : ColorDark2);
      DrawRectangleLinesEx(kRect, 1.0f, isHover ? ColorWhite : ColorDark1);

      char label[2] = { ch, '\0' };
      DrawCentredText(label, faceMd, kRect.x, kRect.width, kRect.y + S(9), S(14), ColorWhite);

      if (canPressOSK && UI_IsPressed() && isHover) {
          lastOskKeyPressTime = now;
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

  // ROW 2 (9 Keys for Letters, 10 for Symbols)
  float startY2 = startY + keyH + gap;
  int count2 = (s->OSKMode == 0) ? 9 : 10;
  float kw2 = (s->OSKMode == 0) ? kw1 : ((availW - (count2 - 1) * gap) / (float)count2);
  float offset2 = (s->OSKMode == 0) ? ((availW - (9 * kw1 + 8 * gap)) / 2.0f) : 0;

  for (int i = 0; i < count2; i++) {
      char ch = (s->OSKMode == 0) ? row2_letters[i] : row2_symbols[i];
      if (s->OSKMode == 0 && !s->OSKShiftActive) ch = (char)tolower((unsigned char)ch);

      Rectangle kRect = { oskX + padX + offset2 + i * (kw2 + gap), startY2, kw2, keyH };
      bool isHover = CheckCollisionPointRec(mPos, kRect);
      DrawRectangleRec(kRect, isHover ? ColorBlue : ColorDark2);
      DrawRectangleLinesEx(kRect, 1.0f, isHover ? ColorWhite : ColorDark1);

      char label[2] = { ch, '\0' };
      DrawCentredText(label, faceMd, kRect.x, kRect.width, kRect.y + S(9), S(14), ColorWhite);

      if (canPressOSK && UI_IsPressed() && isHover) {
          lastOskKeyPressTime = now;
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

  // ROW 3: SHIFT | 7-8 KEYS | BKSP
  float startY3 = startY2 + keyH + gap;
  int count3 = (s->OSKMode == 0) ? 7 : 8;
  float kw3 = kw1;
  float kwShift = (availW - (count3 * kw3 + (count3 + 1) * gap)) / 2.0f;
  if (kwShift < S(40)) kwShift = S(40);
  float kwBksp = kwShift;

  // SHIFT KEY
  Rectangle shiftRect = { oskX + padX, startY3, kwShift, keyH };
  bool hoverShift = CheckCollisionPointRec(mPos, shiftRect);
  Color shiftBg = s->OSKShiftActive ? ColorOrange : (hoverShift ? ColorBlue : ColorDark2);
  DrawRectangleRec(shiftRect, shiftBg);
  DrawRectangleLinesEx(shiftRect, 1.0f, s->OSKShiftActive ? ColorWhite : ColorDark1);
  DrawCentredText("SHIFT", faceXS, shiftRect.x, shiftRect.width, shiftRect.y + S(12), S(10), ColorWhite);

  if (canPressOSK && UI_IsPressed() && hoverShift) {
      lastOskKeyPressTime = now;
      s->OSKShiftActive = !s->OSKShiftActive;
  }

  // ROW 3 LETTERS / SYMBOLS
  for (int i = 0; i < count3; i++) {
      char ch = (s->OSKMode == 0) ? row3_letters[i] : row3_symbols[i];
      if (s->OSKMode == 0 && !s->OSKShiftActive) ch = (char)tolower((unsigned char)ch);

      Rectangle kRect = { oskX + padX + kwShift + gap + i * (kw3 + gap), startY3, kw3, keyH };
      bool isHover = CheckCollisionPointRec(mPos, kRect);
      DrawRectangleRec(kRect, isHover ? ColorBlue : ColorDark2);
      DrawRectangleLinesEx(kRect, 1.0f, isHover ? ColorWhite : ColorDark1);

      char label[2] = { ch, '\0' };
      DrawCentredText(label, faceMd, kRect.x, kRect.width, kRect.y + S(9), S(14), ColorWhite);

      if (canPressOSK && UI_IsPressed() && isHover) {
          lastOskKeyPressTime = now;
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
  Rectangle bkspRect = { oskX + padX + availW - kwBksp, startY3, kwBksp, keyH };
  bool hoverBksp = CheckCollisionPointRec(mPos, bkspRect);
  DrawRectangleRec(bkspRect, hoverBksp ? ColorRed : ColorDark2);
  DrawRectangleLinesEx(bkspRect, 1.0f, hoverBksp ? ColorWhite : ColorDark1);
  DrawCentredText("BKSP", faceXS, bkspRect.x, bkspRect.width, bkspRect.y + S(12), S(10), ColorWhite);

  if (canPressOSK && UI_IsPressed() && hoverBksp) {
      lastOskKeyPressTime = now;
      int len = strlen(s->SearchQuery);
      if (len > 0) {
          s->SearchQuery[len - 1] = '\0';
          s->CursorPos = s->ScrollOffset = 0;
          s->VisualScroll = 0;
          Browser_UpdateActiveTracks(s);
      }
  }

  // ROW 4: ?123/ABC | SPACE | CLEAR | HIDE
  float startY4 = startY3 + keyH + gap;
  float kwMode = S(65);
  float kwClear = S(65);
  float kwHide = S(80);
  float kwSpace = availW - kwMode - kwClear - kwHide - 3 * gap;

  // MODE KEY (?123 / ABC)
  Rectangle modeRect = { oskX + padX, startY4, kwMode, keyH };
  bool hoverMode = CheckCollisionPointRec(mPos, modeRect);
  DrawRectangleRec(modeRect, hoverMode ? ColorBlue : ColorDark2);
  DrawRectangleLinesEx(modeRect, 1.0f, ColorDark1);
  DrawCentredText((s->OSKMode == 0) ? "?123" : "ABC", faceSm, modeRect.x, modeRect.width, modeRect.y + S(11), S(12), ColorOrange);

  if (canPressOSK && UI_IsPressed() && hoverMode) {
      lastOskKeyPressTime = now;
      s->OSKMode = (s->OSKMode == 0) ? 1 : 0;
  }

  // SPACE BAR
  Rectangle spaceRect = { oskX + padX + kwMode + gap, startY4, kwSpace, keyH };
  bool hoverSpace = CheckCollisionPointRec(mPos, spaceRect);
  DrawRectangleRec(spaceRect, hoverSpace ? ColorBlue : ColorDark2);
  DrawRectangleLinesEx(spaceRect, 1.0f, ColorDark1);
  DrawCentredText("SPACE", faceXS, spaceRect.x, spaceRect.width, spaceRect.y + S(12), S(10), ColorShadow);

  if (canPressOSK && UI_IsPressed() && hoverSpace) {
      lastOskKeyPressTime = now;
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

  if (canPressOSK && UI_IsPressed() && hoverClear) {
      lastOskKeyPressTime = now;
      s->SearchQuery[0] = '\0';
      s->CursorPos = s->ScrollOffset = 0;
      s->VisualScroll = 0;
      Browser_UpdateActiveTracks(s);
  }

  // HIDE KEY
  Rectangle hideBtnRect = { clearRect.x + kwClear + gap, startY4, kwHide, keyH };
  bool hoverHide = CheckCollisionPointRec(mPos, hideBtnRect);
  DrawRectangleRec(hideBtnRect, hoverHide ? ColorOrange : ColorBlue);
  DrawRectangleLinesEx(hideBtnRect, 1.0f, ColorWhite);
  DrawCentredText("HIDE", faceSm, hideBtnRect.x, hideBtnRect.width, hideBtnRect.y + S(11), S(12), ColorWhite);

  if (canPressOSK && UI_IsPressed() && hoverHide) {
      lastOskKeyPressTime = now;
      s->ShowOSK = false;
      s->IsSearching = (strlen(s->SearchQuery) > 0);
  }
}

static void Browser_Draw(Component *base) {
  BrowserRenderer *r = (BrowserRenderer *)base;
  BrowserState *s = r->State;

  if (!s->IsActive)
    return;

  float viewH = SCREEN_HEIGHT - DECK_STR_H - S(6.0f);
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
  float sbTrackW = S(14);
  float listW = SCREEN_WIDTH - sidebarW - sbTrackW - S(4);
  if (s->InfoEnabled)
    listW = SCREEN_WIDTH - sidebarW - S(160);

  // Draw Search Box, OSK Button & Table Header
  if (s->BrowseLevel == 0) {
    float headerH = S(24.0f);
    float oskButtonW = S(36);
    Rectangle searchBoxRect = {listX, listYOffset, listW - oskButtonW - S(4), rowH};
    Rectangle oskButtonRect = {listX + listW - oskButtonW, listYOffset, oskButtonW, rowH};
    
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

    listYOffset += rowH;

    // Draw Table Header Bar (Row 2) - Reference Image 2
    Rectangle tableHeaderRect = {listX, listYOffset, listW, headerH};
    DrawRectangleRec(tableHeaderRect, (Color){ 28, 28, 35, 255 });
    DrawRectangleLinesEx(tableHeaderRect, 1.0f, ColorDark1);

    // Column Dividers
    DrawLine(listX + listW - S(110), listYOffset, listX + listW - S(110), listYOffset + headerH, ColorDark1);
    DrawLine(listX + listW - S(55), listYOffset, listX + listW - S(55), listYOffset + headerH, ColorDark1);

    // Column 1: # / TITLE
    char titleHeaderLabel[32] = "# / TITLE";
    if (s->SortMode == 3 || s->SortMode == 0) {
      snprintf(titleHeaderLabel, sizeof(titleHeaderLabel), "# / TITLE %s", s->SortAscending ? "\uf0d8" : "\uf0d7");
    }
    UIDrawText(titleHeaderLabel, faceXS, listX + S(6), listYOffset + S(5), S(10), (s->SortMode == 3 || s->SortMode == 0) ? ColorWhite : ColorShadow);

    // Column 2: BPM (Reference Image 2: BPM ▲ / ▼)
    char bpmHeaderLabel[32] = "BPM";
    if (s->SortMode == 1) {
      snprintf(bpmHeaderLabel, sizeof(bpmHeaderLabel), "BPM %s", s->SortAscending ? "\uf0d8" : "\uf0d7");
    }
    DrawCentredText(bpmHeaderLabel, faceXS, listX + listW - S(110), S(55), listYOffset + S(5), S(10), (s->SortMode == 1) ? ColorWhite : ColorShadow);

    // Column 3: KEY (Reference Image 2: Key ▲ / ▼)
    char keyHeaderLabel[32] = "KEY";
    if (s->SortMode == 2) {
      snprintf(keyHeaderLabel, sizeof(keyHeaderLabel), "KEY %s", s->SortAscending ? "\uf0d8" : "\uf0d7");
    }
    DrawCentredText(keyHeaderLabel, faceXS, listX + listW - S(55), S(55), listYOffset + S(5), S(10), (s->SortMode == 2) ? ColorWhite : ColorShadow);

    listYOffset += headerH;
    totalVisible = (int)floorf((viewH - listYOffset) / rowH);
    if (totalVisible < 1) totalVisible = 1;
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
  if (s->ShowOSK) {
    float oskH = S(172);
    listAreaH -= oskH;
    if (listAreaH < S(50)) listAreaH = S(50);
  }
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

    // Render Track Index Number (019, 020, 021...) & Musical Note Icon (Reference Image 1)
    if (s->BrowseLevel == 0) {
      char numBuf[16];
      sprintf(numBuf, "%03d", idx + 1);
      UIDrawText(numBuf, faceXS, listX + S(4), ry + S(9), S(10), isCursor ? ColorWhite : ColorShadow);
      UIDrawText("\uf001", faceIcon, listX + S(32), ry + S(9), S(9), isCursor ? ColorWhite : ColorShadow);
    }

    float textX = listX + S(36);
    if (s->BrowseLevel == 0)
      textX = listX + S(46);
    else if (s->BrowseLevel == 3)
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

    float maxTitleW = listW - (textX - listX) - S(115);
    Rectangle titleRect = { textX, textY, maxTitleW, rowH };
    UIDrawScrollingText(title, faceSm, titleRect, S(13), ColorWhite, isCursor ? s->MarqueeScrollX : 0.0f);

    if (artist[0] != '\0' && s->BrowseLevel == 0 && !s->InfoEnabled) {
      UIDrawText(artist, faceXS, textX, ry + S(15), S(10),
                 isCursor ? ColorWhite : ColorShadow);
    }

    // BPM & Key Badge (Reference Image 1 & 2 - NO LOAD BUTTON)
    if (s->BrowseLevel == 0 && !s->InfoEnabled) {
      DrawCentredText(bpmText, faceXS, listX + listW - S(110), S(55), ry + S(9), S(11), isCursor ? ColorWhite : ColorShadow);

      // Key Badge with Camelot Wheel Traffic Light Harmonic Matching
      Rectangle keyBadgeRect = { listX + listW - S(52), ry + S(4), S(48), rowH - S(8) };
      
      const char *masterKey = NULL;
      if (s->DeckA && s->DeckA->IsPlaying && s->DeckA->TrackKey[0] != '\0') {
        masterKey = s->DeckA->TrackKey;
      } else if (s->DeckB && s->DeckB->IsPlaying && s->DeckB->TrackKey[0] != '\0') {
        masterKey = s->DeckB->TrackKey;
      } else if (s->DeckA && s->DeckA->TrackKey[0] != '\0') {
        masterKey = s->DeckA->TrackKey;
      } else if (s->DeckB && s->DeckB->TrackKey[0] != '\0') {
        masterKey = s->DeckB->TrackKey;
      }

      int matchLevel = GetCamelotHarmonicMatchLevel(keyStr, masterKey);

      if (matchLevel == 2) {
        // Perfect Camelot Harmonic Match (Traffic Light Green)
        DrawRectangleRec(keyBadgeRect, (Color){ 0, 230, 0, 255 });
        DrawCentredText(keyStr, faceXS, keyBadgeRect.x, keyBadgeRect.width, keyBadgeRect.y + S(5), S(10), ColorBlack);
      } else if (matchLevel == 1) {
        // Energy Shift / Semi-compatible Match (Traffic Light Amber/Yellow)
        DrawRectangleRec(keyBadgeRect, (Color){ 245, 180, 0, 255 });
        DrawCentredText(keyStr, faceXS, keyBadgeRect.x, keyBadgeRect.width, keyBadgeRect.y + S(5), S(10), ColorBlack);
      } else {
        // Incompatible key or no master key loaded
        DrawRectangleRec(keyBadgeRect, (Color){ 35, 35, 42, 255 });
        DrawRectangleLinesEx(keyBadgeRect, 1.0f, ColorDark1);
        DrawCentredText(keyStr, faceXS, keyBadgeRect.x, keyBadgeRect.width, keyBadgeRect.y + S(5), S(10), isCursor ? ColorWhite : ColorShadow);
      }
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
    float sbTrackW = S(14);
    float sbTrackX = SCREEN_WIDTH - sbTrackW;
    float sbTrackY = listYOffset;
    float sbTrackH = viewH - listYOffset;

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
    DrawRectangleRounded((Rectangle){ sbTrackX + S(3), handleY, sbTrackW - S(6), handleH }, 0.5f, 4, handleColor);
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
