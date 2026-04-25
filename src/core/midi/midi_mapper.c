#include "core/midi/midi_mapper.h"
#include "core/midi/midi_translation.h"
#include "core/midi/midi_scripts.h"
#include "core/logic/control_object.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

static void trim_xml_tag(char *str) {
    char *start = strchr(str, '>');
    if (!start) return;
    start++;
    char *end = strchr(start, '<');
    if (!end) return;
    *end = '\0';
    memmove(str, start, strlen(start) + 1);
}

bool MIDI_LoadMapping(MidiMapping *map, const char *path) {
    map->count = 0;
    map->scriptCount = 0;
    memset(map->name, 0, sizeof(map->name));
    memset(map->author, 0, sizeof(map->author));
    memset(map->description, 0, sizeof(map->description));

    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    MappingEntry cur = {0};
    bool inControl = false;
    bool inInfo = false;
    bool inScripts = false;

    while (fgets(line, sizeof(line), f)) {
        char *p;
        
        if (strstr(line, "<info>")) inInfo = true;
        else if (strstr(line, "</info>")) inInfo = false;
        
        if (inInfo) {
            if ((p = strstr(line, "<name>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                strncpy(map->name, temp, 127);
            } else if ((p = strstr(line, "<author>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                strncpy(map->author, temp, 127);
            } else if ((p = strstr(line, "<description>"))) {
                char temp[256]; strcpy(temp, p); trim_xml_tag(temp);
                strncpy(map->description, temp, 255);
            }
        }

        if (strstr(line, "<scriptfiles>")) inScripts = true;
        else if (strstr(line, "</scriptfiles>")) inScripts = false;

        if (inScripts) {
            if ((p = strstr(line, "filename=\""))) {
                char *start = p + 10;
                char *end = strchr(start, '\"');
                if (end && map->scriptCount < 8) {
                    int len = end - start;
                    strncpy(map->scriptFiles[map->scriptCount], start, len);
                    map->scriptFiles[map->scriptCount][len] = '\0';
                    map->scriptCount++;
                }
            }
        }

        if (strstr(line, "<control>")) {
            inControl = true;
            memset(&cur, 0, sizeof(MappingEntry));
        } else if (strstr(line, "</control>")) {
            if (map->count < 512) {
                // Apply Translation if needed
                const MidiTranslation *t = MIDI_GetTranslation(cur.group, cur.key);
                if (t) {
                    strncpy(cur.group, t->unxGroup, 63);
                    strncpy(cur.key, t->unxKey, 63);
                }
                map->entries[map->count++] = cur;
            }
            inControl = false;
        }

        if (inControl) {
            if ((p = strstr(line, "<group>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                strncpy(cur.group, temp, 63);
            } else if ((p = strstr(line, "<key>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                strncpy(cur.key, temp, 63);
            } else if ((p = strstr(line, "<status>"))) {
                unsigned int s;
                sscanf(p, "%*[^0x]0x%x", &s); // Flexible hex parsing
                cur.status = (uint8_t)s;
            } else if ((p = strstr(line, "<midino>"))) {
                unsigned int m;
                sscanf(p, "%*[^0x]0x%x", &m);
                cur.midino = (uint8_t)m;
            } else if (strstr(line, "<Script-Binding/>")) {
                cur.options |= 4; // MIDI_OPT_SCRIPT
                // For script bindings, the key is the function name
                strncpy(cur.scriptFunction, cur.key, 127);
            } else if (strstr(line, "<SelectKnob/>")) {
                cur.options |= 1; // MIDI_OPT_RELATIVE
            } else if (strstr(line, "<fourteen-bit-msb/>")) {
                cur.options |= 8; // 14-bit MSB
            } else if (strstr(line, "<fourteen-bit-lsb/>")) {
                cur.options |= 16; // 14-bit LSB
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
            // Check if name matches (partial match allowed like Engine)
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

int MIDI_ListControllers(const char *dir, char outNames[32][64], char outPaths[32][256]) {
    int count = 0;
#if defined(_WIN32)
    char searchPath[MAX_PATH];
    snprintf(searchPath, MAX_PATH, "%s/*.xml", dir);
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    do {
        char fullPath[MAX_PATH];
        snprintf(fullPath, MAX_PATH, "%s/%s", dir, findData.cFileName);
        MidiMapping temp;
        if (MIDI_LoadMapping(&temp, fullPath)) {
            strncpy(outNames[count], temp.name[0] ? temp.name : findData.cFileName, 63);
            strncpy(outPaths[count], fullPath, 255);
            count++;
        }
    } while (FindNextFileA(hFind, &findData) && count < 32);
    FindClose(hFind);
#else
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < 32) {
        if (strstr(entry->d_name, ".xml")) {
            char fullPath[512];
            snprintf(fullPath, 512, "%s/%s", dir, entry->d_name);
            MidiMapping temp;
            if (MIDI_LoadMapping(&temp, fullPath)) {
                strncpy(outNames[count], temp.name[0] ? temp.name : entry->d_name, 63);
                strncpy(outPaths[count], fullPath, 255);
                count++;
            }
        }
    }
    closedir(d);
#endif
    return count;
}

static uint8_t msbStore[16][128] = {0}; // status & 0xF, midino
static uint8_t lsbStore[16][128] = {0};

void MIDI_HandleMapping(MidiMapping *map, uint8_t status, uint8_t midino, float normalizedValue) {
    uint8_t ch = status & 0x0F;
    uint8_t rawVal = (uint8_t)(normalizedValue * 127.0f);

    for (int i = 0; i < map->count; i++) {
        MappingEntry *e = &map->entries[i];
        if (e->status == status && e->midino == midino) {
            
            // Check if this is a shift/modifier button itself
            if (strstr(e->key, "shift") || strstr(e->key, "Shift")) {
                map->modifiers[0] = (rawVal > 0);
            }

            if (e->options & 8) { // 14-bit MSB
                msbStore[ch][midino] = rawVal;
                uint16_t combined = (rawVal << 7) | lsbStore[ch][midino + 0x20];
                CO_SetValue(e->group, e->key, (float)combined / 16383.0f);
            } else if (e->options & 16) { // 14-bit LSB
                lsbStore[ch][midino] = rawVal;
                uint16_t combined = (msbStore[ch][midino - 0x20] << 7) | rawVal;
                CO_SetValue(e->group, e->key, (float)combined / 16383.0f);
            } else if (e->options & 1) { // MIDI_OPT_RELATIVE
                int val = (int)rawVal;
                float delta = (float)(val - 64);
                
                // If shift held, maybe boost sensitivity?
                if (map->modifiers[0]) delta *= 5.0f;
                
                CO_AddValue(e->group, e->key, delta);
            } else if (e->options & 4) { // MIDI_OPT_SCRIPT
                MIDI_ExecuteScript(map, e->scriptFunction, status, midino, rawVal);
            } else {
                // Standard mapping: if shift held, we could look for a secondary behavior
                // For now, we just pass the value.
                CO_SetValue(e->group, e->key, normalizedValue);
            }
        }
    }
}

