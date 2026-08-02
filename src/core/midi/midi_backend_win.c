#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdbool.h>

#define MIDI_RING_SIZE 512

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
} MidiRawEvent;

static HMIDIIN hMidiIn = NULL;
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
    (void)status; (void)data1; (void)data2;
    // Read-only mode: MIDI OUT transmission disabled to prevent driver hangs
    return true;
}

bool WinMIDI_Init(void (*cb)(uint8_t, uint8_t, uint8_t), char* outDeviceName) {
    (void)cb;
    return WinMIDI_OpenDevice(0, outDeviceName);
}
#endif
