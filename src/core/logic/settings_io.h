#include "ui/player/player_state.h"
#include "core/audio_backend.h"
#include "engine/fx/colorfx/colorfx_manager.h"

// Load settings from settings.json
void Settings_Load(WaveformSettings *wfmA, WaveformSettings *wfmB, AudioBackendConfig *audio, BeatFXState *fx, ColorFXManager *cfxA, ColorFXManager *cfxB, char *controllerPath);

// Save settings to settings.json
void Settings_Save(WaveformSettings wfmA, WaveformSettings wfmB, AudioBackendConfig audio, BeatFXState fx, ColorFXManager cfxA, ColorFXManager cfxB, const char *controllerPath);
