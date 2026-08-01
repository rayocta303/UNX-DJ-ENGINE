#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdbool.h>

// We don't include midi_handler.h here to avoid raylib conflicts.
// We only use basic types.

static HMIDIIN hMidiIn = NULL;
static HMIDIOUT hMidiOut = NULL;
typedef void (*MidiCallback)(uint8_t, uint8_t, uint8_t);
static MidiCallback g_callback = NULL;

void WinMIDI_SetCallback(MidiCallback cb) {
    g_callback = cb;
}

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    (void)hMidiIn; (void)dwInstance; (void)dwParam2;
    if (wMsg == MIM_DATA && g_callback) {
        g_callback((uint8_t)(dwParam1 & 0xFF), (uint8_t)((dwParam1 >> 8) & 0xFF), (uint8_t)((dwParam1 >> 16) & 0xFF));
    }
}

void WinMIDI_Close() {
    if (hMidiIn) {
        midiInStop(hMidiIn);
        midiInClose(hMidiIn);
        hMidiIn = NULL;
    }
    if (hMidiOut) {
        midiOutClose(hMidiOut);
        hMidiOut = NULL;
    }
}

int WinMIDI_GetDevices(char outNames[16][64]) {
    UINT numDevs = midiInGetNumDevs();
    int count = 0;
    for (UINT i = 0; i < numDevs && count < 16; i++) {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR) {
            for (int j = 0; j < 63 && caps.szPname[j]; j++) {
                outNames[count][j] = caps.szPname[j];
                outNames[count][j + 1] = '\0';
            }
            count++;
        }
    }
    return count;
}

bool WinMIDI_OpenDevice(int devId, char* outDeviceName) {
    WinMIDI_Close();

    UINT numDevs = midiInGetNumDevs();
    if (numDevs == 0 || devId < 0 || (UINT)devId >= numDevs) {
        return false;
    }

    MIDIINCAPS capsIn;
    if (midiInGetDevCaps((UINT)devId, &capsIn, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR) {
        if (outDeviceName) {
            for (int i = 0; i < 63 && capsIn.szPname[i]; i++) {
                outDeviceName[i] = capsIn.szPname[i];
                outDeviceName[i + 1] = '\0';
            }
        }
    }

    if (midiInOpen(&hMidiIn, (UINT)devId, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
        midiInStart(hMidiIn);

        // Attempt to find & connect matching output device (for keepalive/LEDs)
        UINT numOut = midiOutGetNumDevs();
        for (UINT o = 0; o < numOut; o++) {
            MIDIOUTCAPS capsOut;
            if (midiOutGetDevCaps(o, &capsOut, sizeof(MIDIOUTCAPS)) == MMSYSERR_NOERROR) {
                if (strstr(capsOut.szPname, capsIn.szPname) || strstr(capsIn.szPname, capsOut.szPname)) {
                    if (midiOutOpen(&hMidiOut, o, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
                        break;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

bool WinMIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2) {
    if (!hMidiOut) return false;
    DWORD msg = (DWORD)status | ((DWORD)data1 << 8) | ((DWORD)data2 << 16);
    return (midiOutShortMsg(hMidiOut, msg) == MMSYSERR_NOERROR);
}

bool WinMIDI_Init(MidiCallback cb, char* outDeviceName) {
    g_callback = cb;
    return WinMIDI_OpenDevice(0, outDeviceName);
}
#endif

