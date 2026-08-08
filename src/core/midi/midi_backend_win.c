#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MIDI_RING_SIZE 1024

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
} MidiRawEvent;

static HMIDIIN hMidiIn = NULL;
static HMIDIOUT hMidiOut = NULL;
static MidiRawEvent g_midiRingBuffer[MIDI_RING_SIZE];
static volatile uint32_t g_midiRingHead = 0;
static volatile uint32_t g_midiRingTail = 0;

void CALLBACK MidiInProc(HMIDIIN hMidiInDev, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    (void)hMidiInDev; (void)dwInstance; (void)dwParam2;
    if (wMsg == MIM_DATA) {
        uint32_t next = (g_midiRingHead + 1) % MIDI_RING_SIZE;
        if (next != g_midiRingTail) {
            g_midiRingBuffer[g_midiRingHead].status = (uint8_t)(dwParam1 & 0xFF);
            g_midiRingBuffer[g_midiRingHead].data1  = (uint8_t)((dwParam1 >> 8) & 0xFF);
            g_midiRingBuffer[g_midiRingHead].data2  = (uint8_t)((dwParam1 >> 16) & 0xFF);
            g_midiRingHead = next;
        }
    }
}

void WinMIDI_Close(void) {
    if (hMidiIn) {
        midiInStop(hMidiIn);
        midiInClose(hMidiIn);
        hMidiIn = NULL;
    }
    if (hMidiOut) {
        midiOutClose(hMidiOut);
        hMidiOut = NULL;
    }
    g_midiRingHead = 0;
    g_midiRingTail = 0;
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

    UINT numDevsIn = midiInGetNumDevs();
    if (numDevsIn == 0 || devId < 0 || (UINT)devId >= numDevsIn) {
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
    } else {
        return false;
    }

    if (midiInOpen(&hMidiIn, (UINT)devId, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        return false;
    }
    midiInStart(hMidiIn);

    // Matching MIDI OUT device by name (controller_debug logic)
    UINT numOut = midiOutGetNumDevs();
    bool outOpened = false;
    for (UINT o = 0; o < numOut; o++) {
        MIDIOUTCAPS capsOut;
        if (midiOutGetDevCaps(o, &capsOut, sizeof(MIDIOUTCAPS)) == MMSYSERR_NOERROR) {
            if (strstr(capsOut.szPname, capsIn.szPname) || strstr(capsIn.szPname, capsOut.szPname)) {
                if (midiOutOpen(&hMidiOut, o, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
                    outOpened = true;
                    break;
                }
            }
        }
    }

    // Fallback: search for non-synth hardware MIDI output
    if (!outOpened && numOut > 0) {
        for (UINT o = 0; o < numOut; o++) {
            MIDIOUTCAPS capsOut;
            if (midiOutGetDevCaps(o, &capsOut, sizeof(MIDIOUTCAPS)) == MMSYSERR_NOERROR) {
                if (strstr(capsOut.szPname, "Mapper") == NULL && strstr(capsOut.szPname, "Synth") == NULL) {
                    if (midiOutOpen(&hMidiOut, o, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
                        outOpened = true;
                        break;
                    }
                }
            }
        }
        if (!outOpened) {
            midiOutOpen(&hMidiOut, 0, 0, 0, CALLBACK_NULL);
        }
    }

    // Dispatch Pioneer LED & VU Meter Enable Handshake immediately upon connecting
    if (hMidiOut) {
        midiOutShortMsg(hMidiOut, (DWORD)(0x9F | (0x00 << 8) | (0x7F << 16)));
        midiOutShortMsg(hMidiOut, (DWORD)(0x90 | (0x7F << 8) | (0x7F << 16)));
        midiOutShortMsg(hMidiOut, (DWORD)(0x91 | (0x7F << 8) | (0x7F << 16)));
        midiOutShortMsg(hMidiOut, (DWORD)(0x93 | (0x7F << 8) | (0x7F << 16)));
        midiOutShortMsg(hMidiOut, (DWORD)(0x94 | (0x7F << 8) | (0x7F << 16)));
    }

    return true;
}

bool WinMIDI_PopEvent(uint8_t *status, uint8_t *data1, uint8_t *data2) {
    if (g_midiRingTail == g_midiRingHead) return false;
    *status = g_midiRingBuffer[g_midiRingTail].status;
    *data1  = g_midiRingBuffer[g_midiRingTail].data1;
    *data2  = g_midiRingBuffer[g_midiRingTail].data2;
    g_midiRingTail = (g_midiRingTail + 1) % MIDI_RING_SIZE;
    return true;
}

bool WinMIDI_SendShortMsg(uint8_t status, uint8_t data1, uint8_t data2) {
    if (hMidiOut) {
        DWORD msg = (DWORD)(status | (data1 << 8) | (data2 << 16));
        return (midiOutShortMsg(hMidiOut, msg) == MMSYSERR_NOERROR);
    }
    return false;
}

bool WinMIDI_SendSysEx(const uint8_t *data, uint32_t length) {
    if (hMidiOut && data && length > 0) {
        MIDIHDR midiHdr;
        memset(&midiHdr, 0, sizeof(MIDIHDR));
        midiHdr.lpData = (LPSTR)data;
        midiHdr.dwBufferLength = length;
        midiHdr.dwBytesRecorded = length;

        if (midiOutPrepareHeader(hMidiOut, &midiHdr, sizeof(MIDIHDR)) == MMSYSERR_NOERROR) {
            midiOutLongMsg(hMidiOut, &midiHdr, sizeof(MIDIHDR));
            while ((midiHdr.dwFlags & MHDR_DONE) == 0) {
                Sleep(1);
            }
            midiOutUnprepareHeader(hMidiOut, &midiHdr, sizeof(MIDIHDR));
            return true;
        }
    }
    return false;
}

bool WinMIDI_Init(void (*cb)(uint8_t, uint8_t, uint8_t), char* outDeviceName) {
    (void)cb;
    return WinMIDI_OpenDevice(0, outDeviceName);
}
#endif

