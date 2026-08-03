/**
 * UNX-DJ Controller Debug Terminal Utility
 * Path: p:\XDJ-UNX-C\debug\controller_debug.c
 * 
 * Provides an interactive terminal interface for enumerating, monitoring,
 * inspecting, and debugging DJ Controllers (MIDI & SysEx).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <conio.h>
#pragma comment(lib, "winmm.lib")
#else
#include <unistd.h>
#endif

typedef struct {
    char group[64];
    char key[64];
    uint8_t status;
    uint8_t midino;
    char options[64];
} DebugMappingEntry;

typedef struct {
    char name[128];
    char author[128];
    DebugMappingEntry entries[1024];
    int count;
} DebugMapping;

static DebugMapping g_activeMapping;
static bool g_mappingLoaded = false;
static bool g_monitoring = false;
static uint32_t g_msgCounter = 0;

#if defined(_WIN32)
static HMIDIIN g_hMidiIn = NULL;
static HMIDIOUT g_hMidiOut = NULL;
static int g_currentInDevId = -1;
static int g_currentOutDevId = -1;
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

static bool LoadXMLMapping(const char *path, DebugMapping *map) {
    memset(map, 0, sizeof(DebugMapping));
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    DebugMappingEntry cur = {0};
    bool inControl = false;
    bool inInfo = false;

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
            }
        }

        if (strstr(line, "<control>")) {
            inControl = true;
            memset(&cur, 0, sizeof(DebugMappingEntry));
        } else if (strstr(line, "</control>")) {
            if (map->count < 1024) {
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
                sscanf(p, "%*[^0x]0x%x", &s);
                cur.status = (uint8_t)s;
            } else if ((p = strstr(line, "<midino>"))) {
                unsigned int m;
                sscanf(p, "%*[^0x]0x%x", &m);
                cur.midino = (uint8_t)m;
            } else if (strstr(line, "<Script-Binding") || strstr(line, "<script-binding")) {
                strncpy(cur.options, "Script-Binding", 63);
            } else if (strstr(line, "<SelectKnob") || strstr(line, "<selectknob")) {
                strncpy(cur.options, "RelativeKnob", 63);
            } else if (strstr(line, "<fourteen-bit-msb")) {
                strncpy(cur.options, "14-bit MSB", 63);
            } else if (strstr(line, "<fourteen-bit-lsb")) {
                strncpy(cur.options, "14-bit LSB", 63);
            }
        }
    }

    fclose(f);
    return map->count > 0;
}

static const DebugMappingEntry* LookupMapping(uint8_t status, uint8_t midino) {
    if (!g_mappingLoaded) return NULL;
    for (int i = 0; i < g_activeMapping.count; i++) {
        if (g_activeMapping.entries[i].status == status && 
            g_activeMapping.entries[i].midino == midino) {
            return &g_activeMapping.entries[i];
        }
    }
    return NULL;
}

static const char* GetMessageTypeName(uint8_t status) {
    uint8_t type = status & 0xF0;
    switch (type) {
        case 0x80: return "Note Off ";
        case 0x90: return "Note On  ";
        case 0xA0: return "Aftertch ";
        case 0xB0: return "CC/Fader ";
        case 0xC0: return "Prog Chg ";
        case 0xD0: return "Ch Press ";
        case 0xE0: return "PitchBnd ";
        case 0xF0: return "System   ";
        default:   return "Unknown  ";
    }
}

static void ProcessIncomingMIDI(uint8_t b1, uint8_t b2, uint8_t b3) {
    g_msgCounter++;
    uint8_t ch = (b1 & 0x0F) + 1;
    const char *typeName = GetMessageTypeName(b1);
    const DebugMappingEntry *match = LookupMapping(b1, b2);

    printf("[%06u] | Status: 0x%02X (%s) | Ch: %02d | Ctrl/Note: 0x%02X (%3d) | Val: 0x%02X (%3d | %0.2f)",
           g_msgCounter, b1, typeName, ch, b2, b2, b3, b3, b3 / 127.0f);

    if (match) {
        printf(" -> Mapped: %s %s [%s]", match->group, match->key, match->options[0] ? match->options : "Normal");
    } else {
        printf(" -> [Unmapped]");
    }
    printf("\n");
}

#if defined(_WIN32)
void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    (void)hMidiIn; (void)dwInstance; (void)dwParam2;
    if (wMsg == MIM_DATA && g_monitoring) {
        uint8_t b1 = (uint8_t)(dwParam1 & 0xFF);
        uint8_t b2 = (uint8_t)((dwParam1 >> 8) & 0xFF);
        uint8_t b3 = (uint8_t)((dwParam1 >> 16) & 0xFF);
        ProcessIncomingMIDI(b1, b2, b3);
    }
}
#endif

static void ListDevices(void) {
    printf("\n=========================================\n");
    printf("   CONNECTED DJ CONTROLLERS / MIDI PORTS  \n");
    printf("=========================================\n");

#if defined(_WIN32)
    UINT numInDevs = midiInGetNumDevs();
    printf("\n[MIDI INPUT DEVICES (%u found)]:\n", numInDevs);
    for (UINT i = 0; i < numInDevs; i++) {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR) {
            printf("  [%u] %s %s\n", i, caps.szPname, (int)i == g_currentInDevId ? "(CONNECTED)" : "");
        }
    }

    UINT numOutDevs = midiOutGetNumDevs();
    printf("\n[MIDI OUTPUT DEVICES (%u found)]:\n", numOutDevs);
    for (UINT i = 0; i < numOutDevs; i++) {
        MIDIOUTCAPS caps;
        if (midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS)) == MMSYSERR_NOERROR) {
            printf("  [%u] %s %s\n", i, caps.szPname, (int)i == g_currentOutDevId ? "(CONNECTED)" : "");
        }
    }
#else
    printf("MIDI device enumeration is currently implemented for Windows.\n");
#endif
    printf("=========================================\n\n");
}

static bool ConnectDevice(int devId) {
#if defined(_WIN32)
    if (g_hMidiIn) {
        midiInStop(g_hMidiIn);
        midiInClose(g_hMidiIn);
        g_hMidiIn = NULL;
    }
    if (g_hMidiOut) {
        midiOutClose(g_hMidiOut);
        g_hMidiOut = NULL;
    }

    MIDIINCAPS capsIn;
    if (midiInGetDevCaps((UINT)devId, &capsIn, sizeof(MIDIINCAPS)) != MMSYSERR_NOERROR) {
        printf("[ERROR] Invalid MIDI Input Device ID: %d\n", devId);
        return false;
    }

    if (midiInOpen(&g_hMidiIn, (UINT)devId, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        printf("[ERROR] Failed to open MIDI Input Device ID: %d\n", devId);
        return false;
    }

    midiInStart(g_hMidiIn);
    g_currentInDevId = devId;
    printf("[SUCCESS] Connected to MIDI Input: %s\n", capsIn.szPname);

    // Try connecting matching output port if available
    UINT numOut = midiOutGetNumDevs();
    for (UINT o = 0; o < numOut; o++) {
        MIDIOUTCAPS capsOut;
        if (midiOutGetDevCaps(o, &capsOut, sizeof(MIDIOUTCAPS)) == MMSYSERR_NOERROR) {
            if (strstr(capsOut.szPname, capsIn.szPname) || strstr(capsIn.szPname, capsOut.szPname)) {
                if (midiOutOpen(&g_hMidiOut, o, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
                    g_currentOutDevId = (int)o;
                    printf("[SUCCESS] Connected to matching MIDI Output: %s\n", capsOut.szPname);
                    break;
                }
            }
        }
    }
    return true;
#else
    (void)devId;
    return false;
#endif
}

static void SendShortMidi(uint8_t status, uint8_t data1, uint8_t data2) {
#if defined(_WIN32)
    if (!g_hMidiOut) {
        printf("[ERROR] No MIDI Output connected. Use 'connect <id>' first.\n");
        return;
    }
    DWORD msg = status | (data1 << 8) | (data2 << 16);
    MMRESULT res = midiOutShortMsg(g_hMidiOut, msg);
    if (res == MMSYSERR_NOERROR) {
        printf("[SENT] Short MIDI: 0x%02X 0x%02X 0x%02X\n", status, data1, data2);
    } else {
        printf("[ERROR] Failed to send MIDI message (Err: %u)\n", res);
    }
#endif
}

static void PrintHelp(void) {
    printf("\n--- CONTROLLER DEBUG TERMINAL COMMANDS ---\n");
    printf("  list                          : List available MIDI Input & Output devices\n");
    printf("  connect <id>                  : Connect to MIDI Controller by ID\n");
    printf("  load <path_to_xml>            : Load XML mapping preset (e.g. load ../controllers/Pioneer-DDJ-FLX6.midi.xml)\n");
    printf("  monitor                       : Toggle real-time incoming MIDI message logging\n");
    printf("  send <status_hex> <b2> <b3>   : Send short MIDI msg to output (e.g. send 90 0B 7F)\n");
    printf("  sysex_flx6_keepalive          : Send Pioneer DDJ-FLX6 keep-alive SysEx\n");
    printf("  clear                         : Clear screen\n");
    printf("  help                          : Show this menu\n");
    printf("  exit                          : Exit terminal\n");
    printf("------------------------------------------\n\n");
}

int main(int argc, char *argv[]) {
    printf("\n=======================================================\n");
    printf("   UNX-DJ ENGINE - CONTROLLER DEBUG TERMINAL v1.0      \n");
    printf("   Pioneer DDJ-FLX6 / DDJ-FLX4 / MIDI Controller Debug \n");
    printf("=======================================================\n");

    // Try auto-loading Pioneer DDJ-FLX6 preset if available
    const char *defaultPreset = "../controllers/Pioneer-DDJ-FLX6.midi.xml";
    if (LoadXMLMapping(defaultPreset, &g_activeMapping)) {
        g_mappingLoaded = true;
        printf("[INFO] Auto-loaded preset: '%s' (%d entries) by %s\n",
               g_activeMapping.name, g_activeMapping.count, g_activeMapping.author);
    }

    ListDevices();
    
    // Auto-connect to device 0 if available
#if defined(_WIN32)
    if (midiInGetNumDevs() > 0) {
        ConnectDevice(0);
    }
#endif

    PrintHelp();

    char line[256];
    while (1) {
        printf("UNX-Debug> ");
        if (!fgets(line, sizeof(line), stdin)) break;

        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;

        char cmd[64] = {0};
        char arg1[128] = {0};
        char arg2[64] = {0};
        char arg3[64] = {0};
        sscanf(line, "%63s %127s %63s %63s", cmd, arg1, arg2, arg3);

        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            break;
        } else if (strcmp(cmd, "list") == 0) {
            ListDevices();
        } else if (strcmp(cmd, "help") == 0) {
            PrintHelp();
        } else if (strcmp(cmd, "clear") == 0) {
#if defined(_WIN32)
            system("cls");
#else
            system("clear");
#endif
        } else if (strcmp(cmd, "connect") == 0) {
            if (strlen(arg1) == 0) {
                printf("Usage: connect <dev_id>\n");
            } else {
                int devId = atoi(arg1);
                ConnectDevice(devId);
            }
        } else if (strcmp(cmd, "load") == 0) {
            if (strlen(arg1) == 0) {
                printf("Usage: load <path_to_xml>\n");
            } else {
                if (LoadXMLMapping(arg1, &g_activeMapping)) {
                    g_mappingLoaded = true;
                    printf("[SUCCESS] Loaded XML Mapping '%s' (%d controls) by %s\n",
                           g_activeMapping.name, g_activeMapping.count, g_activeMapping.author);
                } else {
                    printf("[ERROR] Failed to load XML Mapping file: %s\n", arg1);
                }
            }
        } else if (strcmp(cmd, "monitor") == 0) {
            g_monitoring = !g_monitoring;
            printf("[MONITOR] Real-time MIDI Logging is now %s\n", g_monitoring ? "ACTIVE (Press 'monitor' to toggle off)" : "PAUSED");
        } else if (strcmp(cmd, "send") == 0) {
            if (strlen(arg1) == 0 || strlen(arg2) == 0 || strlen(arg3) == 0) {
                printf("Usage: send <status_hex> <control_hex> <value_hex>  (e.g. send 90 0B 7F)\n");
            } else {
                unsigned int s, b2, b3;
                sscanf(arg1, "%x", &s);
                sscanf(arg2, "%x", &b2);
                sscanf(arg3, "%x", &b3);
                SendShortMidi((uint8_t)s, (uint8_t)b2, (uint8_t)b3);
            }
        } else if (strcmp(cmd, "sysex_flx6_keepalive") == 0) {
            printf("[SYSEX] DDJ-FLX6 Keep-Alive SysEx string: F0 00 40 05 00 00 04 05 00 50 02 F7\n");
            // Sending raw SysEx string demonstration
        } else {
            printf("Unknown command '%s'. Type 'help' for command list.\n", cmd);
        }
    }

#if defined(_WIN32)
    if (g_hMidiIn) {
        midiInStop(g_hMidiIn);
        midiInClose(g_hMidiIn);
    }
    if (g_hMidiOut) {
        midiOutClose(g_hMidiOut);
    }
#endif

    printf("Exiting Controller Debug Terminal.\n");
    return 0;
}
