#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#include <conio.h>

#pragma comment(lib, "winmm.lib")

int total_ticks = 0;
bool is_recording = false;

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (wMsg == MIM_DATA && is_recording) {
        uint8_t status = (uint8_t)(dwParam1 & 0xFF);
        uint8_t data1 = (uint8_t)((dwParam1 >> 8) & 0xFF);
        uint8_t data2 = (uint8_t)((dwParam1 >> 16) & 0xFF);
        
        // Pioneer sends jog turn on CC 0x21, 0x22, 0x23 (Decimal: 33, 34, 35)
        if ((status & 0xF0) == 0xB0) {
            if (data1 == 0x21 || data1 == 0x22 || data1 == 0x23) { 
                int delta = (int)data2 - 64;
                total_ticks += delta;
            }
        }
    }
}

void UpdateConfig(int ticks) {
    char path[512];
    snprintf(path, sizeof(path), "..\\..\\build\\windows\\jog_config.json");
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Error: Could not open %s\n", path);
        return;
    }
    
    char buffer[8192];
    char out_buffer[8192] = {0};
    bool found = false;
    
    while (fgets(buffer, sizeof(buffer), f)) {
        if (strstr(buffer, "\"TicksPerRev\"")) {
            char new_line[256];
            snprintf(new_line, sizeof(new_line), "  \"TicksPerRev\": %d.00,\n", ticks);
            strcat(out_buffer, new_line);
            found = true;
        } else {
            strcat(out_buffer, buffer);
        }
    }
    fclose(f);
    
    if (found) {
        f = fopen(path, "w");
        if (f) {
            fputs(out_buffer, f);
            fclose(f);
            printf("Successfully updated %s with TicksPerRev = %d.00\n", path, ticks);
            printf("Please restart XDJ-UNX to apply the changes.\n");
        } else {
            printf("Error: Could not write to %s\n", path);
        }
    } else {
        printf("Error: Could not find \"TicksPerRev\" in %s\n", path);
    }
}

void wait_for_enter() {
    char buf[128];
    fgets(buf, sizeof(buf), stdin);
}

int main() {
    printf("========================================\n");
    printf("  XDJ-UNX Jogwheel Calibration Tool (C)\n");
    printf("========================================\n\n");
    
    UINT numDevs = midiInGetNumDevs();
    if (numDevs == 0) {
        printf("No MIDI input devices found!\n");
        printf("Press any key to exit...\n");
        _getch();
        return 1;
    }
    
    printf("--- Available MIDI Input Devices ---\n");
    for (UINT i = 0; i < numDevs; i++) {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR) {
            printf("[%d] %s\n", i, caps.szPname);
        }
    }
    
    printf("\nSelect the MIDI device for your controller [0]: ");
    char input_str[16];
    fgets(input_str, sizeof(input_str), stdin);
    int dev_id = 0;
    if (input_str[0] != '\n') {
        dev_id = atoi(input_str);
    }
    
    if (dev_id < 0 || dev_id >= numDevs) {
        printf("Invalid selection.\n");
        return 1;
    }
    
    HMIDIIN hMidiIn;
    if (midiInOpen(&hMidiIn, dev_id, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        printf("Failed to open MIDI port.\n");
        return 1;
    }
    
    midiInStart(hMidiIn);
    
    printf("\n[STEP 1]\n");
    printf("Let's calibrate the Jogwheel Ticks Per Revolution.\n");
    printf("1. Place your finger on the jogwheel at the 12 o'clock position.\n");
    printf("2. Press ENTER when ready to start recording...\n");
    wait_for_enter();
    
    is_recording = true;
    total_ticks = 0;
    
    printf("\n[RECORDING ACTIVE]\n");
    printf("Rotate the jogwheel EXACTLY 1 FULL REVOLUTION CLOCKWISE (back to 12 o'clock).\n");
    printf("Press ENTER when you are done rotating...\n");
    wait_for_enter();
    
    is_recording = false;
    midiInStop(hMidiIn);
    midiInClose(hMidiIn);
    
    printf("\n--- CALIBRATION RESULTS ---\n");
    printf("Total Ticks Accumulated: %d\n", total_ticks);
    
    if (total_ticks == 0) {
        printf("Error: No jogwheel movement detected.\n");
        printf("Press any key to exit...\n");
        _getch();
        return 1;
    }
    
    int target_ticks = abs(total_ticks);
    printf("\nCalculated TicksPerRev: %d\n", target_ticks);
    
    printf("\nDo you want to automatically update jog_config.json with TicksPerRev = %d? (y/n): ", target_ticks);
    fgets(input_str, sizeof(input_str), stdin);
    if (input_str[0] == 'y' || input_str[0] == 'Y') {
        UpdateConfig(target_ticks);
    }
    
    printf("\nPress any key to exit...\n");
    _getch();
    return 0;
}
