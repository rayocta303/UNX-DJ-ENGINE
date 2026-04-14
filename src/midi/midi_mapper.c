#include "midi/midi_mapper.h"
#include "logic/control_object.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

bool MIDI_LoadMapping(MidiMapping *map, const char *path) {
    map->count = 0;
    memset(map->name, 0, sizeof(map->name));
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    MappingEntry cur = {0};
    bool inControl = false;

    while (fgets(line, sizeof(line), f)) {
        char *p;
        // Device Info
        if ((p = strstr(line, "<name>"))) {
            sscanf(p, "<name>%[^<]", map->name);
        }

        if (strstr(line, "<control>")) {
            inControl = true;
            memset(&cur, 0, sizeof(MappingEntry));
        } else if (strstr(line, "</control>")) {
            if (map->count < 256) {
                map->entries[map->count++] = cur;
            }
            inControl = false;
        }

        if (inControl) {
            if ((p = strstr(line, "<group>"))) {
                sscanf(p, "<group>%[^<]", cur.group);
            } else if ((p = strstr(line, "<key>"))) {
                sscanf(p, "<key>%[^<]", cur.key);
            } else if ((p = strstr(line, "<status>"))) {
                unsigned int s;
                sscanf(p, "<status>%x", &s);
                cur.status = (uint8_t)s;
            } else if ((p = strstr(line, "<midino>"))) {
                unsigned int m;
                sscanf(p, "<midino>%x", &m);
                cur.midino = (uint8_t)m;
            }
        }
    }

    fclose(f);
    return map->count > 0;
}

bool MIDI_ScanControllers(const char *dir, const char *deviceName, MidiMapping *out) {
    printf("[MIDI] Scanning %s for device: %s\n", dir, deviceName);
#if defined(_WIN32)
    char searchPath[MAX_PATH];
    snprintf(searchPath, MAX_PATH, "%s/*.xml", dir);
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        char fullPath[MAX_PATH];
        snprintf(fullPath, MAX_PATH, "%s/%s", dir, findData.cFileName);
        MidiMapping temp;
        if (MIDI_LoadMapping(&temp, fullPath)) {
            // Check if name matches (partial match allowed like Mixxx)
            if (strstr(deviceName, temp.name) || strstr(temp.name, deviceName)) {
                *out = temp;
                FindClose(hFind);
                return true;
            }
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
#else
    DIR *d = opendir(dir);
    if (!d) return false;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strstr(entry->d_name, ".xml")) {
            char fullPath[512];
            snprintf(fullPath, 512, "%s/%s", dir, entry->d_name);
            MidiMapping temp;
            if (MIDI_LoadMapping(&temp, fullPath)) {
                if (strstr(deviceName, temp.name) || strstr(temp.name, deviceName)) {
                    *out = temp;
                    closedir(d);
                    return true;
                }
            }
        }
    }
    closedir(d);
#endif
    return false;
}

void MIDI_HandleMapping(MidiMapping *map, uint8_t status, uint8_t midino, float normalizedValue) {
    for (int i = 0; i < map->count; i++) {
        if (map->entries[i].status == status && map->entries[i].midino == midino) {
            CO_SetValue(map->entries[i].group, map->entries[i].key, normalizedValue);
        }
    }
}

