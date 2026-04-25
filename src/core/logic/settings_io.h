#include "ui/player/player_state.h"
#include "core/audio_backend.h"

// Load settings from settings.json
void Settings_Load(WaveformSettings *wfmA, WaveformSettings *wfmB, AudioBackendConfig *audio, char *controllerPath);

// Save settings to settings.json
void Settings_Save(WaveformSettings wfmA, WaveformSettings wfmB, AudioBackendConfig audio, const char *controllerPath);
