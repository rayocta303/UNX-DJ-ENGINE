#include "hotcue_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_HOTCUE_ENTRIES 2048
#define DB_MAGIC "UNXHC01"

typedef struct {
    char FilePath[512];
    char Title[128];
    char Artist[128];
    int Count;
    HotCue Cues[8];
} HotCueDBEntry;

static HotCueDBEntry g_entries[MAX_HOTCUE_ENTRIES];
static int g_entryCount = 0;
static bool g_initialized = false;
static char g_lastStoragePath[512] = {0};

static void NormalizeString(const char *in, char *out, size_t maxLen) {
    if (!in || !out || maxLen == 0) return;
    size_t i = 0;
    while (in[i] != '\0' && i < maxLen - 1) {
        char c = in[i];
        if (c == '\\') c = '/';
        out[i] = (char)tolower((unsigned char)c);
        i++;
    }
    out[i] = '\0';
}

static void GetDbPath(const char *storagePath, char *outPath, size_t maxLen) {
    if (storagePath && storagePath[0] != '\0') {
        snprintf(outPath, maxLen, "%s/unx_hotcues.db", storagePath);
    } else {
        snprintf(outPath, maxLen, "unx_hotcues.db");
    }
}

static void SaveToFile(const char *dbPath) {
    FILE *f = fopen(dbPath, "wb");
    if (!f) return;

    char magic[8] = DB_MAGIC;
    fwrite(magic, 1, 8, f);
    fwrite(&g_entryCount, sizeof(int), 1, f);

    for (int i = 0; i < g_entryCount; i++) {
        fwrite(&g_entries[i], sizeof(HotCueDBEntry), 1, f);
    }

    fclose(f);
}

static void LoadFromFile(const char *dbPath) {
    FILE *f = fopen(dbPath, "rb");
    if (!f) return;

    char magic[8] = {0};
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, DB_MAGIC, 7) != 0) {
        fclose(f);
        return;
    }

    int count = 0;
    if (fread(&count, sizeof(int), 1, f) != 1 || count < 0 || count > MAX_HOTCUE_ENTRIES) {
        fclose(f);
        return;
    }

    for (int i = 0; i < count; i++) {
        HotCueDBEntry entry;
        if (fread(&entry, sizeof(HotCueDBEntry), 1, f) == 1) {
            // Check if entry already exists in global array
            int existingIdx = -1;
            char nPath1[512], nPath2[512];
            NormalizeString(entry.FilePath, nPath1, sizeof(nPath1));

            for (int k = 0; k < g_entryCount; k++) {
                NormalizeString(g_entries[k].FilePath, nPath2, sizeof(nPath2));
                if (strlen(nPath1) > 0 && strcmp(nPath1, nPath2) == 0) {
                    existingIdx = k;
                    break;
                }
                if (strlen(entry.Title) > 0 && strcasecmp(entry.Title, g_entries[k].Title) == 0 &&
                    strlen(entry.Artist) > 0 && strcasecmp(entry.Artist, g_entries[k].Artist) == 0) {
                    existingIdx = k;
                    break;
                }
            }

            if (existingIdx >= 0) {
                g_entries[existingIdx] = entry;
            } else if (g_entryCount < MAX_HOTCUE_ENTRIES) {
                g_entries[g_entryCount++] = entry;
            }
        }
    }

    fclose(f);
}

void HotCueDB_Init(const char *storagePath) {
    g_entryCount = 0;
    g_initialized = true;

    // Load from local app directory first
    char localDb[512];
    GetDbPath(NULL, localDb, sizeof(localDb));
    LoadFromFile(localDb);

    // Also load from storage drive if available
    if (storagePath && storagePath[0] != '\0') {
        strncpy(g_lastStoragePath, storagePath, sizeof(g_lastStoragePath) - 1);
        char storageDb[512];
        GetDbPath(storagePath, storageDb, sizeof(storageDb));
        LoadFromFile(storageDb);
    }
}

void HotCueDB_LoadStorage(const char *storagePath) {
    if (!g_initialized) {
        HotCueDB_Init(storagePath);
        return;
    }

    if (storagePath && storagePath[0] != '\0') {
        strncpy(g_lastStoragePath, storagePath, sizeof(g_lastStoragePath) - 1);
        char storageDb[512];
        GetDbPath(storagePath, storageDb, sizeof(storageDb));
        LoadFromFile(storageDb);
    }
}

void HotCueDB_SaveTrack(const char *storagePath, const char *filePath, const char *title, const char *artist, const HotCue *hotCues, int count) {
    if (!g_initialized) {
        HotCueDB_Init(storagePath);
    }

    if (!filePath && !title) return;

    char nPathTarget[512];
    NormalizeString(filePath ? filePath : "", nPathTarget, sizeof(nPathTarget));

    int targetIdx = -1;
    for (int i = 0; i < g_entryCount; i++) {
        char nPathCurrent[512];
        NormalizeString(g_entries[i].FilePath, nPathCurrent, sizeof(nPathCurrent));

        if (strlen(nPathTarget) > 0 && strlen(nPathCurrent) > 0) {
            // Match by filename suffix / path
            char *sub1 = strrchr(nPathTarget, '/');
            char *sub2 = strrchr(nPathCurrent, '/');
            if (sub1 && sub2 && strcmp(sub1, sub2) == 0) {
                targetIdx = i;
                break;
            } else if (strcmp(nPathTarget, nPathCurrent) == 0) {
                targetIdx = i;
                break;
            }
        }

        if (title && strlen(title) > 0 && strcasecmp(title, g_entries[i].Title) == 0) {
            if (!artist || strlen(artist) == 0 || strcasecmp(artist, g_entries[i].Artist) == 0) {
                targetIdx = i;
                break;
            }
        }
    }

    if (targetIdx < 0) {
        if (g_entryCount >= MAX_HOTCUE_ENTRIES) return;
        targetIdx = g_entryCount++;
    }

    HotCueDBEntry *e = &g_entries[targetIdx];
    memset(e, 0, sizeof(HotCueDBEntry));

    if (filePath) strncpy(e->FilePath, filePath, sizeof(e->FilePath) - 1);
    if (title) strncpy(e->Title, title, sizeof(e->Title) - 1);
    if (artist) strncpy(e->Artist, artist, sizeof(e->Artist) - 1);

    e->Count = (count > 8) ? 8 : (count < 0 ? 0 : count);
    if (hotCues && e->Count > 0) {
        memcpy(e->Cues, hotCues, sizeof(HotCue) * e->Count);
    }

    // Save to local app DB
    char localDb[512];
    GetDbPath(NULL, localDb, sizeof(localDb));
    SaveToFile(localDb);

    // Save to storage drive DB if available
    const char *activeStorage = (storagePath && storagePath[0] != '\0') ? storagePath : g_lastStoragePath;
    if (activeStorage && activeStorage[0] != '\0') {
        char storageDb[512];
        GetDbPath(activeStorage, storageDb, sizeof(storageDb));
        SaveToFile(storageDb);
    }
}

bool HotCueDB_GetTrack(const char *storagePath, const char *filePath, const char *title, const char *artist, HotCue *outHotCues, int *outCount) {
    if (!g_initialized) {
        HotCueDB_Init(storagePath);
    }

    if (!outHotCues || !outCount) return false;

    char nPathTarget[512];
    NormalizeString(filePath ? filePath : "", nPathTarget, sizeof(nPathTarget));

    for (int i = 0; i < g_entryCount; i++) {
        char nPathCurrent[512];
        NormalizeString(g_entries[i].FilePath, nPathCurrent, sizeof(nPathCurrent));

        bool match = false;
        if (strlen(nPathTarget) > 0 && strlen(nPathCurrent) > 0) {
            char *sub1 = strrchr(nPathTarget, '/');
            char *sub2 = strrchr(nPathCurrent, '/');
            if (sub1 && sub2 && strcmp(sub1, sub2) == 0) {
                match = true;
            } else if (strcmp(nPathTarget, nPathCurrent) == 0) {
                match = true;
            }
        }

        if (!match && title && strlen(title) > 0 && strcasecmp(title, g_entries[i].Title) == 0) {
            if (!artist || strlen(artist) == 0 || strcasecmp(artist, g_entries[i].Artist) == 0) {
                match = true;
            }
        }

        if (match) {
            *outCount = g_entries[i].Count;
            if (g_entries[i].Count > 0) {
                memcpy(outHotCues, g_entries[i].Cues, sizeof(HotCue) * g_entries[i].Count);
            }
            return true;
        }
    }

    return false;
}
