#pragma once
#include "ui/player/player_state.h"

// Load settings from settings.json
void Settings_Load(WaveformSettings *wfmA, WaveformSettings *wfmB);

// Save settings to settings.json
void Settings_Save(WaveformSettings wfmA, WaveformSettings wfmB);
