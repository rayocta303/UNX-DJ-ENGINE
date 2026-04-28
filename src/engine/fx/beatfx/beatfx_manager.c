#include "beatfx_manager.h"
#include <string.h>

void BeatFXManager_Init(BeatFXManager* mgr) {
    mgr->activeFX = BEATFX_DELAY;
    mgr->targetChannel = 0;
    mgr->isFxOn = false;
    mgr->beatMs = 500.0f;
    mgr->levelDepth = 0.5f;
    mgr->scrubVal = 0.0f;
    mgr->isScrubbing = false;

    Delay_Init(&mgr->delay);
    Echo_Init(&mgr->echo);
    PingPong_Init(&mgr->pingpong);
    Spiral_Init(&mgr->spiral);
    Reverb_Init(&mgr->reverb);
    Trans_Init(&mgr->trans);
    BFilter_Init(&mgr->bfilter);
    Flanger_Init(&mgr->flanger);
    Phaser_Init(&mgr->phaser);
    Pitch_Init(&mgr->pitch);
    SlipRoll_Init(&mgr->sliproll);
    Roll_Init(&mgr->roll);
    VinylBrake_Init(&mgr->vinylbrake);
    Helix_Init(&mgr->helix);
}

void BeatFXManager_Free(BeatFXManager* mgr) {
    Delay_Free(&mgr->delay);
    Echo_Free(&mgr->echo);
    PingPong_Free(&mgr->pingpong);
    Spiral_Free(&mgr->spiral);
    Reverb_Free(&mgr->reverb);
    Trans_Free(&mgr->trans);
    BFilter_Free(&mgr->bfilter);
    Flanger_Free(&mgr->flanger);
    Phaser_Free(&mgr->phaser);
    Pitch_Free(&mgr->pitch);
    SlipRoll_Free(&mgr->sliproll);
    Roll_Free(&mgr->roll);
    VinylBrake_Free(&mgr->vinylbrake);
    Helix_Free(&mgr->helix);
}

void BeatFXManager_SetFX(BeatFXManager* mgr, BeatFXType type) {
    mgr->activeFX = type;
}

void BeatFXManager_Process(BeatFXManager* mgr, float* outL, float* outR, float inL, float inR, float sampleRate) {
    float wetL = 0, wetR = 0;
    BeatFXManager_ProcessWetOnly(mgr, &wetL, &wetR, inL, inR, sampleRate);
    
    // Calculate dry gain based on Level/Depth knob
    // 0.0 to 0.5: Dry stays at 1.0 (Parallel/Additive)
    // 0.5 to 1.0: Dry fades out (Insert/Crossfade)
    float dryGain = 1.0f;
    if (mgr->levelDepth > 0.5f) {
        dryGain = (1.0f - mgr->levelDepth) * 2.0f;
    }
    
    // If FX is OFF, we kill the wet signal immediately
    if (!mgr->isFxOn) {
        *outL = inL;
        *outR = inR;
    } else {
        *outL = inL * dryGain + wetL;
        *outR = inR * dryGain + wetR;
    }
}

void BeatFXManager_ProcessWetOnly(BeatFXManager* mgr, float* wetL, float* wetR, float inL, float inR, float sampleRate) {
    float mixInL = mgr->isFxOn ? inL : 0.0f;
    float mixInR = mgr->isFxOn ? inR : 0.0f;

    switch(mgr->activeFX) {
        case BEATFX_DELAY:
            Delay_Process(&mgr->delay, wetL, wetR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_ECHO:
            Echo_Process(&mgr->echo, wetL, wetR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_PINGPONG:
            PingPong_Process(&mgr->pingpong, wetL, wetR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_SPIRAL:
            Spiral_Process(&mgr->spiral, wetL, wetR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_REVERB:
            Reverb_Process(&mgr->reverb, wetL, wetR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, mgr->scrubVal, sampleRate);
            break;
        case BEATFX_TRANS:
            if (mgr->isFxOn) {
                Trans_Process(&mgr->trans, wetL, wetR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate);
                // Trans is a subtractive effect (gate). It modifies the signal directly.
                // For WetOnly to work in a parallel sense, it's tricky.
                // But on DJM, Trans is an Insert.
            }
            break;
        case BEATFX_ROLL:
            Roll_Process(&mgr->roll, wetL, wetR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate, mgr->isFxOn);
            break;
        case BEATFX_HELIX:
            Helix_Process(&mgr->helix, wetL, wetR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate, mgr->isFxOn);
            break;
        default:
            // For effects not yet refactored to Wet-Only, use the standard process and subtract dry
            // (Wait, better to just refactor them as needed)
            break;
    }
}

bool BeatFXManager_HasTails(BeatFXManager* mgr, int channelIdx) {
    if (mgr->targetChannel != channelIdx + 1 && mgr->targetChannel != 0) return false;
    if (!mgr->isFxOn) return false; // Immediate cut if OFF
    
    // Standard tail-capable effects
    if (mgr->activeFX == BEATFX_ECHO || mgr->activeFX == BEATFX_REVERB || 
        mgr->activeFX == BEATFX_SPIRAL || mgr->activeFX == BEATFX_DELAY ||
        mgr->activeFX == BEATFX_PINGPONG || mgr->activeFX == BEATFX_ROLL ||
        mgr->activeFX == BEATFX_HELIX) 
    {
        return true; 
    }
    return false;
}
