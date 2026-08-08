#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdbool.h>

#define MIDI_RING_SIZE 1024

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
} MidiRawEvent;

#define MIDI_OUT_RING_SIZE 2048

typedef struct {
    uint8_t type; // 0 = short msg, 1 = sysex
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    uint8_t sysexBuf[32];
    uint32_t sysexLen;
} MidiOutMsg;

static HMIDIIN hMidiIn = NULL;
static HMIDIOUT hMidiOut = NULL;

static MidiRawEvent g_midiRingBuffer[MIDI_RING_SIZE];
static volatile uint32_t g_midiRingHead = 0;
static volatile uint32_t g_midiRingTail = 0;

static MidiOutMsg g_midiOutRing[MIDI_OUT_RING_SIZE];
static volatile uint32_t g_midiOutHead = 0;
static volatile uint32_t g_midiOutTail = 0;
static HANDLE hMidiOutThread = NULL;
static HANDLE hMidiOutEvent = NULL;
static volatile bool g_midiOutRunning = false;

static DWORD WINAPI MidiOutWorkerThread(LPVOID lpParam) {
    (void)lpParam;
    while (g_midiOutRunning) {
        // Drain all pending messages in lockless queue
        bool hadWork = false;
        while (g_midiOutTail != g_midiOutHead) {
            hadWork = true;
            MidiOutMsg msg = g_midiOutRing[g_midiOutTail];
            g_midiOutTail = (g_midiOutTail + 1) % MIDI_OUT_RING_SIZE;

            if (hMidiOut != NULL) {
                if (msg.type == 0) {
                    DWORD dwMsg = ((DWORD)msg.status) | (((DWORD)msg.data1) << 8) | (((DWORD)msg.data2) << 16);
                    midiOutShortMsg(hMidiOut, dwMsg);
                } else if (msg.type == 1 && msg.sysexLen > 0) {
                    MIDIHDR header;
                    memset(&header, 0, sizeof(MIDIHDR));
                    header.lpData = (LPSTR)msg.sysexBuf;
                    header.dwBufferLength = msg.sysexLen;
                    header.dwBytesRecorded = msg.sysexLen;
                    if (midiOutPrepareHeader(hMidiOut, &header, sizeof(MIDIHDR)) == MMSYSERR_NOERROR) {
                        midiOutLongMsg(hMidiOut, &header, sizeof(MIDIHDR));
                        int tries = 0;
                        while (!(header.dwFlags & MHDR_DONE) && tries < 10) {
                            Sleep(1);
                            tries++;
                        }
                        midiOutUnprepareHeader(hMidiOut, &header, sizeof(MIDIHDR));
                    }
                }
            }
        }
        if (!hadWork) {
            // Wait up to 10ms for next signal from UI thread
            WaitForSingleObject(hMidiOutEvent, 10);
        }
    }
    return 0;
}

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
    if (g_midiOutRunning) {
        g_midiOutRunning = false;
        if (hMidiOutEvent) {
            SetEvent(hMidiOutEvent);
        }
        if (hMidiOutThread) {
            WaitForSingleObject(hMidiOutThread, 200);
            CloseHandle(hMidiOutThread);
            hMidiOutThread = NULL;
        }
        if (hMidiOutEvent) {
            CloseHandle(hMidiOutEvent);
            hMidiOutEvent = NULL;
        }
    }
    if (hMidiIn) {
        midiInStop(hMidiIn);
        midiInClose(hMidiIn);
        hMidiIn = NULL;
    }
    if (hMidiOut) {
        midiOutReset(hMidiOut);
        midiOutClose(hMidiOut);
        hMidiOut = NULL;
    }
    g_midiRingHead = 0;
    g_midiRingTail = 0;
    g_midiOutHead = 0;
    g_midiOutTail = 0;
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
    }

    if (midiInOpen(&hMidiIn, (UINT)devId, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
        midiInStart(hMidiIn);

        // Match and Open MIDI Output Device
        UINT numDevsOut = midiOutGetNumDevs();
        int matchedOutIdx = -1;
        for (UINT i = 0; i < numDevsOut; i++) {
            MIDIOUTCAPS capsOut;
            if (midiOutGetDevCaps(i, &capsOut, sizeof(MIDIOUTCAPS)) == MMSYSERR_NOERROR) {
                if (strstr(capsOut.szPname, capsIn.szPname) || strstr(capsIn.szPname, capsOut.szPname)) {
                    matchedOutIdx = (int)i;
                    break;
                }
            }
        }

        // Fallback to devId if within bounds
        if (matchedOutIdx < 0 && (UINT)devId < numDevsOut) {
            matchedOutIdx = devId;
        }

        if (matchedOutIdx >= 0) {
            if (midiOutOpen(&hMidiOut, (UINT)matchedOutIdx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
                hMidiOutEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
                g_midiOutRunning = true;
                hMidiOutThread = CreateThread(NULL, 0, MidiOutWorkerThread, NULL, 0, NULL);
                if (hMidiOutThread) {
                    SetThreadPriority(hMidiOutThread, THREAD_PRIORITY_HIGHEST);
                }
            }
        }

        return true;
    }
    return false;
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
    if (!hMidiOut || !g_midiOutRunning) return false;
    uint32_t next = (g_midiOutHead + 1) % MIDI_OUT_RING_SIZE;
    if (next == g_midiOutTail) return false; // Lockless Ring Buffer drop on overflow

    g_midiOutRing[g_midiOutHead].type = 0;
    g_midiOutRing[g_midiOutHead].status = status;
    g_midiOutRing[g_midiOutHead].data1 = data1;
    g_midiOutRing[g_midiOutHead].data2 = data2;
    g_midiOutHead = next;

    if (hMidiOutEvent) {
        SetEvent(hMidiOutEvent);
    }
    return true;
}

bool WinMIDI_SendSysEx(const uint8_t *data, uint32_t length) {
    if (!hMidiOut || !g_midiOutRunning || !data || length == 0 || length > 32) return false;
    uint32_t next = (g_midiOutHead + 1) % MIDI_OUT_RING_SIZE;
    if (next == g_midiOutTail) return false;

    g_midiOutRing[g_midiOutHead].type = 1;
    memcpy(g_midiOutRing[g_midiOutHead].sysexBuf, data, length);
    g_midiOutRing[g_midiOutHead].sysexLen = length;
    g_midiOutHead = next;

    if (hMidiOutEvent) {
        SetEvent(hMidiOutEvent);
    }
    return true;
}

bool WinMIDI_Init(void (*cb)(uint8_t, uint8_t, uint8_t), char* outDeviceName) {
    (void)cb;
    return WinMIDI_OpenDevice(0, outDeviceName);
}
#endif
