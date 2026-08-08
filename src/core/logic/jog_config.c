#include "jog_config.h"
#include "control_object.h"
#include "core/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global instance definition
JogConfig g_JogConfig;

void JogConfig_InitDefaults(JogConfig *config) {
    if (!config) return;

    // 1. Jog Encoder & Calibration
    config->DefaultRPM = 33.333333f;
    config->TicksPerRev = 720.0f;

    // 2. Exponential Moving Average (EMA) Filtering
    config->EmaRawWeight = 0.75f;
    config->EmaPrevWeight = 0.25f;

    // 3. Vinyl Touch Release Inertia (Spin-Down Glide)
    config->VinylReleaseFriction = 0.965f;
    config->VinylReleaseCutoff = 0.005f;
    config->VinylReleaseMinVelocity = 0.01f;

    // 4. CDJ Mode Pitch Bend Nudge
    config->PitchBendFriction = 0.92f;
    config->PitchBendCutoff = 0.005f;
    config->PitchBendScale = 1.0f;

    // 5. Waveform Touch Drag Nudge
    config->WaveformNudgeScale = 0.5f;

    // 6. Backspin Release FX
    config->BackspinShortSpeed = -7.0f;
    config->BackspinLongSpeed = -15.0f;
    config->BackspinDecay = 0.96f;
}

bool JogConfig_Load(JogConfig *config, const char *filePath) {
    if (!config) return false;
    
    // Start with clean defaults first
    JogConfig_InitDefaults(config);

    const char *path = (filePath && filePath[0] != '\0') ? filePath : "jog_config.json";
    FILE *f = fopen(path, "r");
    if (!f) {
        // File does not exist yet; save default config file for easy editing
        UNX_LOG_INFO("[JOG_CONFIG] No config found at '%s'. Creating default configuration file.", path);
        JogConfig_Save(config, path);
        return true;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        return false;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t readBytes = fread(buf, 1, sz, f);
    buf[readBytes] = '\0';
    fclose(f);

    float val;
    if (sscanf(strstr(buf, "\"DefaultRPM\""), "\"DefaultRPM\": %f", &val) == 1) config->DefaultRPM = val;
    if (sscanf(strstr(buf, "\"TicksPerRev\""), "\"TicksPerRev\": %f", &val) == 1) config->TicksPerRev = val;
    if (sscanf(strstr(buf, "\"EmaRawWeight\""), "\"EmaRawWeight\": %f", &val) == 1) config->EmaRawWeight = val;
    if (sscanf(strstr(buf, "\"EmaPrevWeight\""), "\"EmaPrevWeight\": %f", &val) == 1) config->EmaPrevWeight = val;

    if (sscanf(strstr(buf, "\"VinylReleaseFriction\""), "\"VinylReleaseFriction\": %f", &val) == 1) config->VinylReleaseFriction = val;
    if (sscanf(strstr(buf, "\"VinylReleaseCutoff\""), "\"VinylReleaseCutoff\": %f", &val) == 1) config->VinylReleaseCutoff = val;
    if (sscanf(strstr(buf, "\"VinylReleaseMinVelocity\""), "\"VinylReleaseMinVelocity\": %f", &val) == 1) config->VinylReleaseMinVelocity = val;

    if (sscanf(strstr(buf, "\"PitchBendFriction\""), "\"PitchBendFriction\": %f", &val) == 1) config->PitchBendFriction = val;
    if (sscanf(strstr(buf, "\"PitchBendCutoff\""), "\"PitchBendCutoff\": %f", &val) == 1) config->PitchBendCutoff = val;
    if (sscanf(strstr(buf, "\"PitchBendScale\""), "\"PitchBendScale\": %f", &val) == 1) config->PitchBendScale = val;

    if (sscanf(strstr(buf, "\"WaveformNudgeScale\""), "\"WaveformNudgeScale\": %f", &val) == 1) config->WaveformNudgeScale = val;

    if (sscanf(strstr(buf, "\"BackspinShortSpeed\""), "\"BackspinShortSpeed\": %f", &val) == 1) config->BackspinShortSpeed = val;
    if (sscanf(strstr(buf, "\"BackspinLongSpeed\""), "\"BackspinLongSpeed\": %f", &val) == 1) config->BackspinLongSpeed = val;
    if (sscanf(strstr(buf, "\"BackspinDecay\""), "\"BackspinDecay\": %f", &val) == 1) config->BackspinDecay = val;

    free(buf);
    UNX_LOG_INFO("[JOG_CONFIG] Loaded jogwheel configuration from '%s'.", path);
    return true;
}

bool JogConfig_Save(const JogConfig *config, const char *filePath) {
    if (!config) return false;

    const char *path = (filePath && filePath[0] != '\0') ? filePath : "jog_config.json";
    FILE *f = fopen(path, "w");
    if (!f) {
        UNX_LOG_ERR("[JOG_CONFIG] Failed to open '%s' for writing.", path);
        return false;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"// Encoder Calibration\": \"Settings for physical jog encoder\",\n");
    fprintf(f, "  \"DefaultRPM\": %.6f,\n", config->DefaultRPM);
    fprintf(f, "  \"TicksPerRev\": %.2f,\n", config->TicksPerRev);
    fprintf(f, "\n");
    fprintf(f, "  \"// Smoothing Filter\": \"Exponential Moving Average (EMA) weights\",\n");
    fprintf(f, "  \"EmaRawWeight\": %.4f,\n", config->EmaRawWeight);
    fprintf(f, "  \"EmaPrevWeight\": %.4f,\n", config->EmaPrevWeight);
    fprintf(f, "\n");
    fprintf(f, "  \"// Vinyl Touch Release Inertia\": \"Momentum spin-down glide when releasing top platter\",\n");
    fprintf(f, "  \"VinylReleaseFriction\": %.4f,\n", config->VinylReleaseFriction);
    fprintf(f, "  \"VinylReleaseCutoff\": %.6f,\n", config->VinylReleaseCutoff);
    fprintf(f, "  \"VinylReleaseMinVelocity\": %.4f,\n", config->VinylReleaseMinVelocity);
    fprintf(f, "\n");
    fprintf(f, "  \"// CDJ Pitch Bend Nudge\": \"Side ring pitch bend settings\",\n");
    fprintf(f, "  \"PitchBendFriction\": %.4f,\n", config->PitchBendFriction);
    fprintf(f, "  \"PitchBendCutoff\": %.6f,\n", config->PitchBendCutoff);
    fprintf(f, "  \"PitchBendScale\": %.4f,\n", config->PitchBendScale);
    fprintf(f, "\n");
    fprintf(f, "  \"// Waveform Touch Drag\": \"Waveform UI touch drag nudge scale\",\n");
    fprintf(f, "  \"WaveformNudgeScale\": %.4f,\n", config->WaveformNudgeScale);
    fprintf(f, "\n");
    fprintf(f, "  \"// Backspin FX\": \"Special Release FX speeds\",\n");
    fprintf(f, "  \"BackspinShortSpeed\": %.2f,\n", config->BackspinShortSpeed);
    fprintf(f, "  \"BackspinLongSpeed\": %.2f,\n", config->BackspinLongSpeed);
    fprintf(f, "  \"BackspinDecay\": %.4f\n", config->BackspinDecay);
    fprintf(f, "}\n");

    fclose(f);
    UNX_LOG_INFO("[JOG_CONFIG] Saved jogwheel configuration to '%s'.", path);
    return true;
}

void JogConfig_RegisterControlObjects(void) {
    CO_Register("[JogConfig]", "DefaultRPM", CO_TYPE_FLOAT, &g_JogConfig.DefaultRPM, 1.0f, 100.0f);
    CO_Register("[JogConfig]", "TicksPerRev", CO_TYPE_FLOAT, &g_JogConfig.TicksPerRev, 100.0f, 4096.0f);
    CO_Register("[JogConfig]", "EmaRawWeight", CO_TYPE_FLOAT, &g_JogConfig.EmaRawWeight, 0.05f, 1.0f);
    CO_Register("[JogConfig]", "EmaPrevWeight", CO_TYPE_FLOAT, &g_JogConfig.EmaPrevWeight, 0.0f, 0.95f);
    CO_Register("[JogConfig]", "VinylReleaseFriction", CO_TYPE_FLOAT, &g_JogConfig.VinylReleaseFriction, 0.80f, 0.999f);
    CO_Register("[JogConfig]", "VinylReleaseCutoff", CO_TYPE_FLOAT, &g_JogConfig.VinylReleaseCutoff, 0.0001f, 0.05f);
    CO_Register("[JogConfig]", "PitchBendFriction", CO_TYPE_FLOAT, &g_JogConfig.PitchBendFriction, 0.80f, 0.999f);
    CO_Register("[JogConfig]", "PitchBendCutoff", CO_TYPE_FLOAT, &g_JogConfig.PitchBendCutoff, 0.0001f, 0.05f);
    CO_Register("[JogConfig]", "PitchBendScale", CO_TYPE_FLOAT, &g_JogConfig.PitchBendScale, 0.1f, 5.0f);
    CO_Register("[JogConfig]", "WaveformNudgeScale", CO_TYPE_FLOAT, &g_JogConfig.WaveformNudgeScale, 0.1f, 5.0f);
}
