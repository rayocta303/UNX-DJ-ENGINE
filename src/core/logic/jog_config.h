#ifndef JOG_CONFIG_H
#define JOG_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // 1. Jog Encoder & Calibration
    float DefaultRPM;             // Base RPM for 1.0x speed calibration (default: 33.333333f)
    float TicksPerRev;            // Encoder ticks per full 360deg revolution (default: 720.0f)
    
    // 2. Exponential Moving Average (EMA) Filtering
    float EmaRawWeight;           // New raw MIDI rate weight in EMA filter (default: 0.75f)
    float EmaPrevWeight;          // Previous rate weight in EMA filter (default: 0.25f)
    
    // 3. Vinyl Touch Release Inertia (Spin-Down Glide)
    float VinylReleaseFriction;   // Per-frame exponential decay multiplier (default: 0.965f)
    float VinylReleaseCutoff;     // Rate cutoff threshold to end release inertia (default: 0.005f)
    float VinylReleaseMinVelocity;// Minimum speed offset required to start release glide (default: 0.01f)

    // 4. CDJ Mode Pitch Bend Nudge
    float PitchBendFriction;      // Per-frame decay multiplier for pitch bend (default: 0.92f)
    float PitchBendCutoff;        // Rate cutoff threshold for pitch bend (default: 0.005f)
    float PitchBendScale;         // Sensitivity scaling for outer ring pitch bend (default: 1.0f)
    
    // 5. Waveform Touch Drag Nudge
    float WaveformNudgeScale;     // Sensitivity scale for CDJ mode waveform drag nudge (default: 0.5f)

    // 6. Backspin Release FX
    float BackspinShortSpeed;     // Initial speed for short backspin (default: -7.0f)
    float BackspinLongSpeed;      // Initial speed for long backspin (default: -15.0f)
    float BackspinDecay;          // Per-frame decay factor for backspin (default: 0.96f)
} JogConfig;

// Global jogwheel configuration instance
extern JogConfig g_JogConfig;

// Methods
void JogConfig_InitDefaults(JogConfig *config);
bool JogConfig_Load(JogConfig *config, const char *filePath);
bool JogConfig_Save(const JogConfig *config, const char *filePath);

// Register control objects for runtime tuning/scripting
void JogConfig_RegisterControlObjects(void);

#ifdef __cplusplus
}
#endif

#endif // JOG_CONFIG_H
