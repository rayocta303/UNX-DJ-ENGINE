#include "unx_database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0777)
#endif

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
static char g_sessionHistoryFile[1024] = {0};

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
        snprintf(outPath, maxLen, "%s/UNX-DJ/unx_hotcues.db", storagePath);
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

void UNXDatabase_Init(const char* storagePath) {
    if (!storagePath || storagePath[0] == '\0') return;

    // Create UNX-DJ folder
    char unxDjPath[1024];
    snprintf(unxDjPath, sizeof(unxDjPath), "%s/UNX-DJ", storagePath);
    MKDIR(unxDjPath);

    // Create UNX-DJ/History folder
    char historyPath[1024];
    snprintf(historyPath, sizeof(historyPath), "%s/UNX-DJ/History", storagePath);
    MKDIR(historyPath);

    // Generate Session CSV filename: UNX-YYYY-MM-DD_HH-mm-ss.csv
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(g_sessionHistoryFile, sizeof(g_sessionHistoryFile),
             "%s/UNX-%04d-%02d-%02d_%02d-%02d-%02d.csv",
             historyPath, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Initialize HotCue DB
    UNXDatabase_LoadStorage(storagePath);
}

void UNXDatabase_LoadStorage(const char* storagePath) {
    if (!storagePath || storagePath[0] == '\0') return;

    if (g_initialized && strcmp(g_lastStoragePath, storagePath) == 0) {
        return;
    }

    g_entryCount = 0;
    strncpy(g_lastStoragePath, storagePath, sizeof(g_lastStoragePath) - 1);

    char dbPath[1024];
    GetDbPath(storagePath, dbPath, sizeof(dbPath));
    LoadFromFile(dbPath);
    
    // Also try to load from root if missing in UNX-DJ folder (migration)
    if (g_entryCount == 0) {
        char oldDbPath[1024];
        snprintf(oldDbPath, sizeof(oldDbPath), "%s/unx_hotcues.db", storagePath);
        LoadFromFile(oldDbPath);
        if (g_entryCount > 0) {
            // Save to new location to migrate
            SaveToFile(dbPath);
        }
    }
    
    g_initialized = true;
}

void UNXDatabase_SaveTrack(const char* storagePath, const char* filePath, const char* title, const char* artist, const HotCue* hotCues, int count) {
    if (!g_initialized || strcmp(g_lastStoragePath, storagePath) != 0) {
        UNXDatabase_LoadStorage(storagePath);
    }

    char nPath1[512], nPath2[512];
    NormalizeString(filePath, nPath1, sizeof(nPath1));

    int targetIdx = -1;
    for (int i = 0; i < g_entryCount; i++) {
        NormalizeString(g_entries[i].FilePath, nPath2, sizeof(nPath2));
        if (strlen(nPath1) > 0 && strcmp(nPath1, nPath2) == 0) {
            targetIdx = i;
            break;
        }
        if (strlen(title) > 0 && strcasecmp(title, g_entries[i].Title) == 0 &&
            strlen(artist) > 0 && strcasecmp(artist, g_entries[i].Artist) == 0) {
            targetIdx = i;
            break;
        }
    }

    if (targetIdx < 0) {
        if (g_entryCount >= MAX_HOTCUE_ENTRIES) return;
        targetIdx = g_entryCount++;
    }

    HotCueDBEntry *entry = &g_entries[targetIdx];
    strncpy(entry->FilePath, filePath, sizeof(entry->FilePath) - 1);
    strncpy(entry->Title, title, sizeof(entry->Title) - 1);
    strncpy(entry->Artist, artist, sizeof(entry->Artist) - 1);
    entry->Count = count;
    
    memset(entry->Cues, 0, sizeof(entry->Cues));
    for (int i = 0; i < count && i < 8; i++) {
        entry->Cues[i] = hotCues[i];
    }

    char dbPath[1024];
    GetDbPath(storagePath, dbPath, sizeof(dbPath));
    SaveToFile(dbPath);
}

bool UNXDatabase_GetTrack(const char* storagePath, const char* filePath, const char* title, const char* artist, HotCue* outHotCues, int* outCount) {
    if (!g_initialized || strcmp(g_lastStoragePath, storagePath) != 0) {
        UNXDatabase_LoadStorage(storagePath);
    }

    char nPath1[512], nPath2[512];
    NormalizeString(filePath, nPath1, sizeof(nPath1));

    for (int i = 0; i < g_entryCount; i++) {
        NormalizeString(g_entries[i].FilePath, nPath2, sizeof(nPath2));
        
        bool match = false;
        if (strlen(nPath1) > 0 && strcmp(nPath1, nPath2) == 0) {
            match = true;
        } else if (strlen(title) > 0 && strcasecmp(title, g_entries[i].Title) == 0 &&
                   strlen(artist) > 0 && strcasecmp(artist, g_entries[i].Artist) == 0) {
            match = true;
        }

        if (match) {
            *outCount = g_entries[i].Count;
            if (outHotCues) {
                memcpy(outHotCues, g_entries[i].Cues, sizeof(HotCue) * (*outCount));
            }
            return true;
        }
    }

    *outCount = 0;
    return false;
}

void UNXDatabase_LogHistory(const char* storagePath, DeckState* deck) {
    if (!deck || !deck->LoadedTrack || !storagePath || storagePath[0] == '\0') return;
    TrackState* track = deck->LoadedTrack;
    
    // If the session file isn't created yet, create it and write CSV header
    if (g_sessionHistoryFile[0] == '\0') {
        UNXDatabase_Init(storagePath);
    }

    // Check if file exists to write header
    bool writeHeader = false;
    FILE *checkF = fopen(g_sessionHistoryFile, "r");
    if (!checkF) {
        writeHeader = true;
    } else {
        fclose(checkF);
    }

    FILE *f = fopen(g_sessionHistoryFile, "a");
    if (!f) return;

    if (writeHeader) {
        fprintf(f, "TimeLoaded,Title,Artist,BPM,Key,FilePath\n");
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timeStr[64];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Escape quotes in CSV
    fprintf(f, "\"%s\",\"%s\",\"%s\",%.1f,\"%s\",\"%s\"\n",
            timeStr,
            track->Title,
            track->Artist,
            deck->OriginalBPM,
            deck->TrackKey,
            track->FilePath);

    fclose(f);
}
