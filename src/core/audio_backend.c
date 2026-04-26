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

static const char* ma_backend_to_string(ma_backend backend) {
    switch (backend) {
        case ma_backend_wasapi: return "WASAPI";
        case ma_backend_dsound: return "DirectSound";
        case ma_backend_winmm: return "WinMM";
        case ma_backend_alsa: return "ALSA";
        case ma_backend_pulseaudio: return "PulseAudio";
        case ma_backend_jack: return "JACK";
        case ma_backend_coreaudio: return "CoreAudio";
        case ma_backend_sndio: return "sndio";
        case ma_backend_audio4: return "audio4";
        case ma_backend_oss: return "OSS";
        case ma_backend_aaudio: return "AAudio";
        case ma_backend_opensl: return "OpenSL ES";
        case ma_backend_webaudio: return "WebAudio";
        case ma_backend_null: return "Null";
        default: return "Unknown";
    }
}

static AudioBackendCallback g_appCallback = NULL;
static AudioBackendConfig g_currentConfig;
static ma_device_info g_deviceInfos[MAX_AUDIO_DEVICES];
static int g_deviceCount = 0;

void AudioBackend_GetActiveInfo(int* outChannels, int* outSampleRate, char* outBackendName, char* outDeviceName) {
    if (g_isDeviceInitialized) {
        if (outChannels) *outChannels = g_maDevice.playback.channels;
        if (outSampleRate) *outSampleRate = g_maDevice.sampleRate;
        if (outBackendName) {
            strcpy(outBackendName, ma_backend_to_string(g_maContext.backend));
        }
        if (outDeviceName) {
            if (g_currentConfig.DeviceIndex == -1) {
                strcpy(outDeviceName, "System Default");
            } else {
                // Return the actual name of the opened hardware
                ma_device_info info;
                if (ma_context_get_device_info(&g_maContext, ma_device_type_playback, &g_maDevice.playback.id, &info) == MA_SUCCESS) {
                    strcpy(outDeviceName, info.name);
                } else {
                    strcpy(outDeviceName, "Hardware Link");
                }
            }
        }
    } else {
        if (outChannels) *outChannels = 0;
        if (outSampleRate) *outSampleRate = 0;
        if (outBackendName) strcpy(outBackendName, "None");
        if (outDeviceName) strcpy(outDeviceName, "Not connected");
    }
}

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    float* out = (float*)pOutput;
    if (!out || !g_appCallback) return;
    
    int destChannels = pDevice->playback.channels;
    
    static float temp4Ch[4096 * 4];
    if (frameCount > 4096) frameCount = 4096;
    
    // Fill 4-channel intermediate buffer from engine
    memset(temp4Ch, 0, sizeof(float) * frameCount * 4);
    g_appCallback(temp4Ch, frameCount);
    
    // Map intermediate channels (Master L/R, Cue L/R) to hardware outputs
    // Note: pOutput is already zeroed by miniaudio if we requested MA_FALSE for noPreZeroedOutputBuffer.
    // However, to be extra safe for all backends, we zero it here.
    memset(out, 0, sizeof(float) * frameCount * destChannels);
    
    for (ma_uint32 s = 0; s < frameCount; s++) {
        // Route Master
        if (g_currentConfig.MasterOutL >= 0 && g_currentConfig.MasterOutL < destChannels) 
            out[s*destChannels + g_currentConfig.MasterOutL] += temp4Ch[s*4 + 0];
        if (g_currentConfig.MasterOutR >= 0 && g_currentConfig.MasterOutR < destChannels) 
            out[s*destChannels + g_currentConfig.MasterOutR] += temp4Ch[s*4 + 1];
        
        // Route Cue
        if (g_currentConfig.CueOutL >= 0 && g_currentConfig.CueOutL < destChannels) 
            out[s*destChannels + g_currentConfig.CueOutL] += temp4Ch[s*4 + 2];
        if (g_currentConfig.CueOutR >= 0 && g_currentConfig.CueOutR < destChannels) 
            out[s*destChannels + g_currentConfig.CueOutR] += temp4Ch[s*4 + 3];
        
        // Limiter/Hard clip per hardware channel
        for(int c=0; c<destChannels; c++) {
            float val = out[s*destChannels + c];
            if (val > 1.0f) out[s*destChannels + c] = 1.0f;
            else if (val < -1.0f) out[s*destChannels + c] = -1.0f;
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
    printf("[AUDIO] Initialized Backend: %s\n", ma_backend_to_string(g_maContext.backend));
    
    // Enumerate devices
    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    if (ma_context_get_devices(&g_maContext, &pPlaybackInfos, &playbackCount, NULL, NULL) == MA_SUCCESS) {
        g_deviceCount = playbackCount > MAX_AUDIO_DEVICES ? MAX_AUDIO_DEVICES : playbackCount;
        for (int i = 0; i < g_deviceCount; i++) {
            g_deviceInfos[i] = pPlaybackInfos[i];
            
            ma_device_info info;
            if (ma_context_get_device_info(&g_maContext, ma_device_type_playback, &g_deviceInfos[i].id, &info) == MA_SUCCESS) {
                printf("[AUDIO] Found Device: %s (Channels: %d, Native SR: %dHz)\n", 
                        g_deviceInfos[i].name, info.nativeDataFormats[0].channels, info.nativeDataFormats[0].sampleRate);
            } else {
                printf("[AUDIO] Found Device: %s\n", g_deviceInfos[i].name);
            }
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
        strcpy(outDevices[i].BackendName, ma_backend_to_string(g_maContext.backend));
        outDevices[i].ID = &g_deviceInfos[i].id;
        outDevices[i].IsDefault = g_deviceInfos[i].isDefault;
        
        // We can request more info from miniaudio if needed, but for now we'll fill from what we have.
        // Miniaudio may not know maxChannels reliably until open on some backends, 
        // but we can retrieve basic info.
        outDevices[i].NativeChannels = 2; // Default fallback
        outDevices[i].MinChannels = 1;
        outDevices[i].MaxChannels = 8;
        outDevices[i].PreferredSampleRate = 48000;

        // Try to get detailed info
        ma_device_info info;
        if (ma_context_get_device_info(&g_maContext, ma_device_type_playback, &g_deviceInfos[i].id, &info) == MA_SUCCESS) {
            outDevices[i].NativeChannels = info.nativeDataFormats[0].channels;
            outDevices[i].PreferredSampleRate = info.nativeDataFormats[0].sampleRate;
        }
    }
    return count;
}

bool AudioBackend_Start(AudioBackendConfig config, AudioBackendCallback callback) {
    if (!g_isContextInitialized) return false;
    
    // Optimization: If hardware-critical settings haven't changed, 
    // just update routing and return to avoid glitches.
    if (g_isDeviceInitialized &&
        g_currentConfig.DeviceIndex == config.DeviceIndex &&
        g_currentConfig.SampleRate == config.SampleRate &&
        g_currentConfig.BufferSizeFrames == config.BufferSizeFrames) 
    {
        g_currentConfig = config;
        return true;
    }

    if (g_isDeviceInitialized) {
        ma_device_stop(&g_maDevice); // Safe stop first
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
    
    // Request channels: We try to request what the user wants, but we will accept whatever the device can do.
    // If the device has fewer channels, we'll just "blank" the ones that don't exist in the callback.
    int requestedChannels = 2; 
    int userMaxIdx = config.MasterOutL;
    if (config.MasterOutR > userMaxIdx) userMaxIdx = config.MasterOutR;
    if (config.CueOutL > userMaxIdx) userMaxIdx = config.CueOutL;
    if (config.CueOutR > userMaxIdx) userMaxIdx = config.CueOutR;
    
    requestedChannels = userMaxIdx + 1;
    if (requestedChannels < 2) requestedChannels = 2;
    if (requestedChannels > 8) requestedChannels = 8; // Safety limit
    
#if defined(PLATFORM_IOS)
    // Force Stereo on iOS for stability with internal speakers/standard headphones
    requestedChannels = 2;
#endif

    devConfig.playback.channels = requestedChannels;
    devConfig.sampleRate = config.SampleRate;
    devConfig.periodSizeInFrames = config.BufferSizeFrames;
    devConfig.dataCallback = data_callback;
    
    // We allow miniaudio to handle sample rate conversion if needed, 
    // but on iOS we prefer to match hardware.
    ma_result result = ma_device_init(&g_maContext, &devConfig, &g_maDevice);
    if (result != MA_SUCCESS) {
        printf("[AUDIO] Warning: User settings for device %d (ch:%d, sr:%d) failed. Retrying with hardware native settings...\n", 
               config.DeviceIndex, requestedChannels, config.SampleRate);
        
        // Strategy: Progressive fallback
        // Attempt 1: Native channels, user SR
        devConfig.playback.channels = 0; 
        result = ma_device_init(&g_maContext, &devConfig, &g_maDevice);
        
        if (result != MA_SUCCESS) {
            // Attempt 2: Native channels, Native SR
            devConfig.sampleRate = 0;
            result = ma_device_init(&g_maContext, &devConfig, &g_maDevice);
        }
    }
    g_isDeviceInitialized = true;
    
    if (ma_device_start(&g_maDevice) != MA_SUCCESS) {
        printf("[AUDIO] Failed to start device.\n");
        ma_device_uninit(&g_maDevice);
        g_isDeviceInitialized = false;
        return false;
    }
    
    printf("[AUDIO] Started device: %s, Channels: %d (Requested %d), SR: %d, Buffer: %d\n", 
            g_maDevice.playback.name, g_maDevice.playback.channels, requestedChannels, g_maDevice.sampleRate, g_currentConfig.BufferSizeFrames);
            
    return true;
}

void AudioBackend_Stop(void) {
    if (g_isDeviceInitialized) {
        ma_device_stop(&g_maDevice);
    }
}
