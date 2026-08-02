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
    map->outputCount = 0;
    memset(map->name, 0, sizeof(map->name));
    memset(map->author, 0, sizeof(map->author));
    memset(map->description, 0, sizeof(map->description));
    memset(map->modifiers, 0, sizeof(map->modifiers));
    memset(map->lookupTable, -1, sizeof(map->lookupTable));

    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    MappingEntry cur = {0};
    MidiOutputEntry curOut = {0};
    bool inControl = false;
    bool inOutput = false;
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
                    int len = (int)(end - start);
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
            if (map->count < 2048) {
                // Apply Translation if needed
                const MidiTranslation *t = MIDI_GetTranslation(cur.group, cur.key);
                if (t) {
                    strncpy(cur.group, t->unxGroup, 63);
                    strncpy(cur.key, t->unxKey, 63);
                }

                // Bind ControlObject pointer if exists
                cur.cachedCO = CO_Find(cur.group, cur.key, NULL);

                int idx = map->count;
                map->entries[idx] = cur;
                map->count++;

                // Register into direct O(1) lookup table
                map->lookupTable[cur.status][cur.midino] = idx;

                // Also map NoteOff status (0x80) to NoteOn entry (0x90) for seamless release tracking
                if ((cur.status & 0xF0) == 0x90) {
                    uint8_t noteOffStatus = 0x80 | (cur.status & 0x0F);
                    if (map->lookupTable[noteOffStatus][cur.midino] == -1) {
                        map->lookupTable[noteOffStatus][cur.midino] = idx;
                    }
                }
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
                sscanf(p, "%*[^0x]0x%x", &s);
                cur.status = (uint8_t)s;
            } else if ((p = strstr(line, "<midino>"))) {
                unsigned int m;
                sscanf(p, "%*[^0x]0x%x", &m);
                cur.midino = (uint8_t)m;
            } else if (strstr(line, "<Script-Binding") || strstr(line, "<script-binding")) {
                cur.options |= MIDI_OPT_SCRIPT;
                strncpy(cur.scriptFunction, cur.key, 127);
            } else if (strstr(line, "<SelectKnob") || strstr(line, "<selectknob")) {
                cur.options |= MIDI_OPT_RELATIVE;
            } else if (strstr(line, "<fourteen-bit-msb")) {
                cur.options |= MIDI_OPT_14BIT_MSB;
            } else if (strstr(line, "<fourteen-bit-lsb")) {
                cur.options |= MIDI_OPT_14BIT_LSB;
            } else if (strstr(line, "<invert") || strstr(line, "<Invert")) {
                cur.options |= MIDI_OPT_INVERT;
            } else if (strstr(line, "<button") || strstr(line, "<Button")) {
                cur.options |= MIDI_OPT_BUTTON;
            } else if (strstr(line, "<switch") || strstr(line, "<Switch")) {
                cur.options |= MIDI_OPT_SWITCH;
            } else if (strstr(line, "<rot64inv") || strstr(line, "<Rot64inv")) {
                cur.options |= MIDI_OPT_ROT64INV;
            } else if (strstr(line, "<rot64fast") || strstr(line, "<Rot64fast")) {
                cur.options |= MIDI_OPT_ROT64FAST;
            } else if (strstr(line, "<rot64") || strstr(line, "<Rot64")) {
                cur.options |= MIDI_OPT_ROT64;
            } else if (strstr(line, "<diff") || strstr(line, "<Diff")) {
                cur.options |= MIDI_OPT_DIFF;
            } else if (strstr(line, "<hercjogfast") || strstr(line, "<Hercjogfast")) {
                cur.options |= MIDI_OPT_HERCJOGFAST;
            } else if (strstr(line, "<hercjog") || strstr(line, "<Hercjog")) {
                cur.options |= MIDI_OPT_HERCJOG;
            } else if (strstr(line, "<spread64") || strstr(line, "<Spread64")) {
                cur.options |= MIDI_OPT_SPREAD64;
            }
        }

        if (strstr(line, "<output>")) {
            inOutput = true;
            memset(&curOut, 0, sizeof(MidiOutputEntry));
            curOut.on = 0x7F;
            curOut.off = 0x00;
            curOut.minimum = 0.0f;
            curOut.maximum = 1.0f;
            curOut.lastSentVal = -1;
        } else if (strstr(line, "</output>")) {
            if (map->outputCount < 1024) {
                const MidiTranslation *t = MIDI_GetTranslation(curOut.group, curOut.key);
                if (t) {
                    strncpy(curOut.group, t->unxGroup, 63);
                    strncpy(curOut.key, t->unxKey, 63);
                }
                map->outputs[map->outputCount++] = curOut;
            }
            inOutput = false;
        }

        if (inOutput) {
            if ((p = strstr(line, "<group>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                strncpy(curOut.group, temp, 63);
            } else if ((p = strstr(line, "<key>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                strncpy(curOut.key, temp, 63);
            } else if ((p = strstr(line, "<status>"))) {
                unsigned int s;
                sscanf(p, "%*[^0x]0x%x", &s);
                curOut.status = (uint8_t)s;
            } else if ((p = strstr(line, "<midino>"))) {
                unsigned int m;
                sscanf(p, "%*[^0x]0x%x", &m);
                curOut.midino = (uint8_t)m;
            } else if ((p = strstr(line, "<on>"))) {
                unsigned int onVal;
                sscanf(p, "%*[^0x]0x%x", &onVal);
                curOut.on = (uint8_t)onVal;
            } else if ((p = strstr(line, "<off>"))) {
                unsigned int offVal;
                sscanf(p, "%*[^0x]0x%x", &offVal);
                curOut.off = (uint8_t)offVal;
            } else if ((p = strstr(line, "<minimum>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                curOut.minimum = (float)atof(temp);
            } else if ((p = strstr(line, "<maximum>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                curOut.maximum = (float)atof(temp);
            }
        }
    }

    fclose(f);
    return map->count > 0;
}

bool MIDI_ScanControllers(const char *dir, const char *deviceName, MidiMapping *out) {
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
            if (count < 32) {
                strncpy(outNames[count], temp.name, 63);
                strncpy(outPaths[count], fullPath, 255);
                count++;
            }
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
#else
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strstr(entry->d_name, ".xml")) {
            char fullPath[512];
            snprintf(fullPath, 512, "%s/%s", dir, entry->d_name);
            MidiMapping temp;
            if (MIDI_LoadMapping(&temp, fullPath)) {
                if (count < 32) {
                    strncpy(outNames[count], temp.name, 63);
                    strncpy(outPaths[count], fullPath, 255);
                    count++;
                }
            }
        }
    }
    closedir(d);
#endif
    return count;
}

static uint8_t msbStore[16][128] = {0};
static uint8_t lsbStore[16][128] = {0};

void MIDI_HandleMapping(MidiMapping *map, uint8_t status, uint8_t midino, float normalizedValue) {
    if (!map) return;
    int idx = map->lookupTable[status][midino];
    if (idx < 0 || idx >= map->count) return;

    MappingEntry *e = &map->entries[idx];
    uint8_t ch = status & 0x0F;
    float currentNormVal = normalizedValue;
    uint8_t currentRawVal = (uint8_t)(normalizedValue * 127.0f);

    // NoteOff (0x80) mapped to NoteOn (0x90) entry -> zero velocity release
    if ((status & 0xF0) == 0x80 && (e->status & 0xF0) == 0x90) {
        currentNormVal = 0.0f;
        currentRawVal = 0;
    }

    // Check if this is a shift/modifier button
    if (strstr(e->key, "shift") || strstr(e->key, "Shift")) {
        map->modifiers[0] = (currentRawVal > 0);
    }

    // Invert option
    if (e->options & MIDI_OPT_INVERT) {
        currentNormVal = 1.0f - currentNormVal;
        currentRawVal = 127 - currentRawVal;
    }

    if (e->options & MIDI_OPT_14BIT_MSB) {
        msbStore[ch][midino] = currentRawVal;
        int lsbIndex = midino + 0x20;
        if (lsbIndex < 128) {
            uint16_t combined = (currentRawVal << 7) | lsbStore[ch][lsbIndex];
            CO_SetValue(e->group, e->key, (float)combined / 16383.0f);
        }
    } else if (e->options & MIDI_OPT_14BIT_LSB) {
        lsbStore[ch][midino] = currentRawVal;
        int msbIndex = midino - 0x20;
        if (msbIndex >= 0 && msbIndex < 128) {
            uint16_t combined = (msbStore[ch][msbIndex] << 7) | currentRawVal;
            CO_SetValue(e->group, e->key, (float)combined / 16383.0f);
        }
    } else if (e->options & MIDI_OPT_SCRIPT) {
        MIDI_ExecuteScript(map, e->scriptFunction, status, midino, currentRawVal);
    } else if ((e->options & MIDI_OPT_SWITCH) ||
               strcmp(e->key, "pfl") == 0 ||
               strcmp(e->key, "master_tempo") == 0 ||
               strcmp(e->key, "quantize") == 0 ||
               strcmp(e->key, "vinyl_mode") == 0 ||
               strcmp(e->key, "slip") == 0) {
        if (currentRawVal > 0) {
            CO_ToggleValue(e->group, e->key);
        }
    } else if (e->options & MIDI_OPT_BUTTON) {
        CO_SetValue(e->group, e->key, currentRawVal > 0 ? 1.0f : 0.0f);
    } else if (e->options & (MIDI_OPT_ROT64 | MIDI_OPT_ROT64INV)) {
        float diff = (float)currentRawVal - 64.0f;
        if (diff == -1.0f || diff == 1.0f) {
            diff /= 16.0f;
        } else if (diff != 0.0f) {
            diff += (diff > 0.0f ? -1.0f : 1.0f);
        }
        if (e->options & MIDI_OPT_ROT64INV) diff = -diff;
        CO_AddValue(e->group, e->key, diff);
    } else if (e->options & MIDI_OPT_ROT64FAST) {
        float diff = ((float)currentRawVal - 64.0f) * 1.5f;
        CO_AddValue(e->group, e->key, diff);
    } else if (e->options & (MIDI_OPT_DIFF | MIDI_OPT_RELATIVE)) {
        float diff = (currentRawVal >= 64) ? (float)(currentRawVal - 128) : (float)currentRawVal;
        if (map->modifiers[0]) diff *= 5.0f;
        CO_AddValue(e->group, e->key, diff);
    } else if (e->options & (MIDI_OPT_HERCJOG | MIDI_OPT_HERCJOGFAST)) {
        float diff = (currentRawVal > 64) ? (float)(currentRawVal - 128) : (float)currentRawVal;
        if (e->options & MIDI_OPT_HERCJOGFAST) diff *= 3.0f;
        CO_AddValue(e->group, e->key, diff);
    } else if (e->options & MIDI_OPT_SPREAD64) {
        float diff = (float)currentRawVal - 64.0f;
        CO_AddValue(e->group, e->key, diff);
    } else {
        CO_SetValue(e->group, e->key, currentNormVal);
    }
}

bool MIDI_SaveMapping(MidiMapping *map, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "<?xml version='1.0' encoding='utf-8'?>\n");
    fprintf(f, "<UNXMIDIPreset schemaVersion=\"1\">\n");
    fprintf(f, "    <info>\n");
    fprintf(f, "        <name>%s</name>\n", map->name);
    fprintf(f, "        <author>%s</author>\n", map->author);
    fprintf(f, "        <description>%s</description>\n", map->description);
    fprintf(f, "    </info>\n");
    fprintf(f, "    <controller id=\"%s\">\n", map->name);
    fprintf(f, "        <controls>\n");

    for (int i = 0; i < map->count; i++) {
        MappingEntry *e = &map->entries[i];
        fprintf(f, "            <control>\n");
        fprintf(f, "                <group>%s</group>\n", e->group);
        fprintf(f, "                <key>%s</key>\n", e->key);
        fprintf(f, "                <status>0x%02X</status>\n", e->status);
        fprintf(f, "                <midino>0x%02X</midino>\n", e->midino);
        fprintf(f, "                <options>");
        bool hasOpt = false;
        if (e->options & 1) { fprintf(f, "<SelectKnob/>"); hasOpt = true; }
        if (e->options & 4) { fprintf(f, "<Script-Binding/>"); hasOpt = true; }
        if (e->options & 8) { fprintf(f, "<fourteen-bit-msb/>"); hasOpt = true; }
        if (e->options & 16) { fprintf(f, "<fourteen-bit-lsb/>"); hasOpt = true; }
        if (!hasOpt) fprintf(f, "<Normal/>");
        fprintf(f, "</options>\n");
        fprintf(f, "            </control>\n");
    }

    fprintf(f, "        </controls>\n");
    fprintf(f, "    </controller>\n");
    fprintf(f, "</UNXMIDIPreset>\n");

    fclose(f);
    return true;
}

void MIDI_CreateTemplate(MidiMapping *out) {
    memset(out, 0, sizeof(MidiMapping));
    memset(out->lookupTable, -1, sizeof(out->lookupTable));
    strncpy(out->name, "New Custom Mapping", 127);
    strncpy(out->author, "User", 127);
    strncpy(out->description, "Created using UNX MIDI Learn", 255);
    
    int coCount = CO_GetCount();
    for (int i = 0; i < coCount && i < 512; i++) {
        ControlObject *co = CO_GetByIndex(i);
        MappingEntry *e = &out->entries[out->count++];
        strncpy(e->group, co->group, 63);
        strncpy(e->key, co->key, 63);
        e->status = 0x00;
        e->midino = 0x00;
        e->options = 0;
        e->cachedCO = co;
    }
}
