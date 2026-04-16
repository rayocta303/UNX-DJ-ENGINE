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
typedef void (*MidiCallback)(uint8_t, uint8_t, uint8_t);
static MidiCallback g_callback = NULL;

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    (void)hMidiIn; (void)dwInstance; (void)dwParam2;
    if (wMsg == MIM_DATA && g_callback) {
        g_callback((uint8_t)(dwParam1 & 0xFF), (uint8_t)((dwParam1 >> 8) & 0xFF), (uint8_t)((dwParam1 >> 16) & 0xFF));
    }
}

bool WinMIDI_Init(MidiCallback cb, char* outDeviceName) {
    g_callback = cb;
    if (midiInGetNumDevs() > 0) {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(0, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR) {
            if (outDeviceName) {
                for(int i=0; i<31 && caps.szPname[i]; i++) outDeviceName[i] = caps.szPname[i];
                outDeviceName[31] = '\0';
            }
        }
        if (midiInOpen(&hMidiIn, 0, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
            midiInStart(hMidiIn);
            return true;
        }
    }
    return false;
}

void WinMIDI_Close() {
    if (hMidiIn) {
        midiInStop(hMidiIn);
        midiInClose(hMidiIn);
        hMidiIn = NULL;
    }
}
#endif
