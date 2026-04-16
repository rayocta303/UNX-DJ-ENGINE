#pragma once

#include <stdbool.h>

#define MAX_AUDIO_DEVICES 32

typedef struct {
    char Name[128];
    void* ID; // Pointer to ma_device_id internals
    int MinChannels;
    int MaxChannels;
    bool IsDefault;
} AudioDeviceInfo;

typedef struct {
    int DeviceIndex;      // Index in the enumerated array, -1 for default
    int MasterOutL;       // Index of channel (e.g. 0 for CH 1)
    int MasterOutR;       // Index of channel (e.g. 1 for CH 2)
    int CueOutL;          // Index of channel (e.g. 2 for CH 3)
    int CueOutR;          // Index of channel (e.g. 3 for CH 4)
    int SampleRate;       // 44100, 48000, etc.
    int BufferSizeFrames; // 128, 256, 512, 1024
} AudioBackendConfig;

typedef void (*AudioBackendCallback)(float* buffer, unsigned int frames);

bool AudioBackend_Init(void);
void AudioBackend_Terminate(void);

// Returns number of devices found
int AudioBackend_GetDevices(AudioDeviceInfo* outDevices, int maxDevices);

// Starts or Restarts the audio backend with the given configuration
bool AudioBackend_Start(AudioBackendConfig config, AudioBackendCallback callback);

// Stops the audio backend processing
void AudioBackend_Stop(void);
