#include "audio_backend.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DECODING
#define MA_API static
#include "miniaudio.h"

#include <stdio.h>
#include <string.h>

static ma_context g_maContext;
static ma_device g_maDevice;
static bool g_isContextInitialized = false;
static bool g_isDeviceInitialized = false;

static AudioBackendCallback g_appCallback = NULL;
static AudioBackendConfig g_currentConfig;

static ma_device_info g_deviceInfos[MAX_AUDIO_DEVICES];
static int g_deviceCount = 0;

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    float* out = (float*)pOutput;
    
    if (g_appCallback) {
        static float temp4Ch[4096 * 4];
        if (frameCount > 4096) frameCount = 4096;
        
        // Zero out the temp buffer (to avoid garbage if engine underfills)
        memset(temp4Ch, 0, sizeof(float) * frameCount * 4);
        
        // Ask engine to fill standard 4-Channel Matrix (MasterL, MasterR, CueL, CueR)
        g_appCallback(temp4Ch, frameCount);
        
        int destChannels = pDevice->playback.channels;
        
        for (ma_uint32 s = 0; s < frameCount; s++) {
            for(int c=0; c<destChannels; c++) {
                out[s*destChannels + c] = 0.0f;
            }
            
            // Route Master
            if (g_currentConfig.MasterOutL < destChannels) 
                out[s*destChannels + g_currentConfig.MasterOutL] += temp4Ch[s*4 + 0];
            if (g_currentConfig.MasterOutR < destChannels) 
                out[s*destChannels + g_currentConfig.MasterOutR] += temp4Ch[s*4 + 1];
            
            // Route Cue
            if (g_currentConfig.CueOutL < destChannels) 
                out[s*destChannels + g_currentConfig.CueOutL] += temp4Ch[s*4 + 2];
            if (g_currentConfig.CueOutR < destChannels) 
                out[s*destChannels + g_currentConfig.CueOutR] += temp4Ch[s*4 + 3];
            
            // Hard clip
            for(int c=0; c<destChannels; c++) {
                if (out[s*destChannels + c] > 1.0f) out[s*destChannels + c] = 1.0f;
                if (out[s*destChannels + c] < -1.0f) out[s*destChannels + c] = -1.0f;
            }
        }
    }
}

bool AudioBackend_Init(void) {
    if (g_isContextInitialized) return true;
    
    if (ma_context_init(NULL, 0, NULL, &g_maContext) != MA_SUCCESS) {
        printf("[AUDIO] Failed to initialize miniaudio context.\n");
        return false;
    }
    g_isContextInitialized = true;
    
    // Enumerate devices
    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    if (ma_context_get_devices(&g_maContext, &pPlaybackInfos, &playbackCount, NULL, NULL) == MA_SUCCESS) {
        g_deviceCount = playbackCount > MAX_AUDIO_DEVICES ? MAX_AUDIO_DEVICES : playbackCount;
        for (int i = 0; i < g_deviceCount; i++) {
            g_deviceInfos[i] = pPlaybackInfos[i];
            printf("[AUDIO] Found Device: %s\n", g_deviceInfos[i].name);
        }
    }
    
    return true;
}

void AudioBackend_Terminate(void) {
    if (g_isDeviceInitialized) {
        ma_device_uninit(&g_maDevice);
        g_isDeviceInitialized = false;
    }
    if (g_isContextInitialized) {
        ma_context_uninit(&g_maContext);
        g_isContextInitialized = false;
    }
}

int AudioBackend_GetDevices(AudioDeviceInfo* outDevices, int maxDevices) {
    int count = g_deviceCount > maxDevices ? maxDevices : g_deviceCount;
    for(int i=0; i<count; i++) {
        strcpy(outDevices[i].Name, g_deviceInfos[i].name);
        outDevices[i].ID = &g_deviceInfos[i].id;
        outDevices[i].IsDefault = g_deviceInfos[i].isDefault;
        // Miniaudio may not know maxChannels reliably until open, but some backends populate it.
        // We defer channel assumption to the UI logic (assume up to 8).
        outDevices[i].MinChannels = 2; 
        outDevices[i].MaxChannels = 8;
    }
    return count;
}

bool AudioBackend_Start(AudioBackendConfig config, AudioBackendCallback callback) {
    if (!g_isContextInitialized) return false;
    
    if (g_isDeviceInitialized) {
        ma_device_uninit(&g_maDevice);
        g_isDeviceInitialized = false;
    }
    
    g_appCallback = callback;
    g_currentConfig = config;
    
    ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
    if (config.DeviceIndex >= 0 && config.DeviceIndex < g_deviceCount) {
        devConfig.playback.pDeviceID = &g_deviceInfos[config.DeviceIndex].id;
    }
    
    devConfig.playback.format = ma_format_f32;
    // Request highest required channel index + 1
    int maxCh = config.MasterOutL;
    if (config.MasterOutR > maxCh) maxCh = config.MasterOutR;
    if (config.CueOutL > maxCh) maxCh = config.CueOutL;
    if (config.CueOutR > maxCh) maxCh = config.CueOutR;
    
    devConfig.playback.channels = maxCh + 1; // e.g. if Cue is on CH3 (index 2), we need at least 3 channels.
    // Force at least stereo to not break standard output routing.
    if (devConfig.playback.channels < 2) devConfig.playback.channels = 2;
    
    devConfig.sampleRate = config.SampleRate;
    devConfig.periodSizeInFrames = config.BufferSizeFrames;
    devConfig.dataCallback = data_callback;
    
    if (ma_device_init(&g_maContext, &devConfig, &g_maDevice) != MA_SUCCESS) {
        printf("[AUDIO] Failed to initialize device %d (requested %d channels).\n", config.DeviceIndex, devConfig.playback.channels);
        return false;
    }
    g_isDeviceInitialized = true;
    
    if (ma_device_start(&g_maDevice) != MA_SUCCESS) {
        printf("[AUDIO] Failed to start device.\n");
        ma_device_uninit(&g_maDevice);
        g_isDeviceInitialized = false;
        return false;
    }
    
    printf("[AUDIO] Started device: %s, Channels: %d, SR: %d, Buffer: %d\n", 
            g_maDevice.playback.name, g_maDevice.playback.channels, g_maDevice.sampleRate, g_currentConfig.BufferSizeFrames);
            
    return true;
}

void AudioBackend_Stop(void) {
    if (g_isDeviceInitialized) {
        ma_device_stop(&g_maDevice);
    }
}
