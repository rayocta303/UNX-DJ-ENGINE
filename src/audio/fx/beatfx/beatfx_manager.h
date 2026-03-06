#ifndef BEATFX_MANAGER_H
#define BEATFX_MANAGER_H

#include "delay.h"
#include "echo.h"
#include "pingpong.h"
#include "spiral.h"
#include "roll.h"
#include "sliproll.h"
#include "reverb.h"
#include "helix.h"
#include "flanger.h"
#include "phaser.h"
#include "bfilter.h"
#include "trans.h"
#include "pitch.h"
#include "vinylbrake.h"

typedef enum {
    BEATFX_DELAY = 0,
    BEATFX_ECHO = 1,
    BEATFX_PINGPONG = 2,
    BEATFX_SPIRAL = 3,
    BEATFX_REVERB = 4,
    BEATFX_TRANS = 5,
    BEATFX_FILTER = 6,
    BEATFX_FLANGER = 7,
    BEATFX_PHASER = 8,
    BEATFX_PITCH = 9,
    BEATFX_SLIPROLL = 10,
    BEATFX_ROLL = 11,
    BEATFX_VINYLBRAKE = 12,
    BEATFX_HELIX = 13
} BeatFXType;

typedef struct {
    BeatFXType activeFX;
    int targetChannel;   // 0 = Master, 1 = Deck 1, 2 = Deck 2
    bool isFxOn;
    float beatMs;        // X-PAD time param
    float levelDepth;    // 0.0 to 1.0 Mix
    float scrubVal;      // For Filter/Pitch scrubbing (-1.0 to 1.0)
    bool isScrubbing;

    BeatFX_Delay delay;
    BeatFX_Echo echo;
    BeatFX_PingPong pingpong;
    BeatFX_Spiral spiral;
    BeatFX_Reverb reverb;
    BeatFX_Trans trans;
    BeatFX_BFilter bfilter;
    BeatFX_Flanger flanger;
    BeatFX_Phaser phaser;
    BeatFX_Pitch pitch;
    BeatFX_SlipRoll sliproll;
    BeatFX_Roll roll;
    BeatFX_VinylBrake vinylbrake;
    BeatFX_Helix helix;
} BeatFXManager;

void BeatFXManager_Init(BeatFXManager* mgr);
void BeatFXManager_Free(BeatFXManager* mgr);
void BeatFXManager_SetFX(BeatFXManager* mgr, BeatFXType type);
void BeatFXManager_Process(BeatFXManager* mgr, float* outL, float* outR, float inL, float inR, float sampleRate);

#endif
