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
    if (mgr->activeFX != type) {
        mgr->activeFX = type;
        // Optional: Reset when switching effect types? 
        // BeatFXManager_Reset(mgr);
    }
}

void BeatFXManager_SetFXOn(BeatFXManager* mgr, bool on) {
    if (on && !mgr->isFxOn) {
        // Just turned ON: Reset the active effect state to clear any old tails
        BeatFXManager_Reset(mgr);
    }
    mgr->isFxOn = on;
}

void BeatFXManager_Reset(BeatFXManager* mgr) {
    // Clear all delay-based effects to ensure a clean start
    DelayLine_Clear(&mgr->delay.delayL);
    DelayLine_Clear(&mgr->delay.delayR);
    
    DelayLine_Clear(&mgr->echo.delayL);
    DelayLine_Clear(&mgr->echo.delayR);
    mgr->echo.lastOutL = 0;
    mgr->echo.lastOutR = 0;

    DelayLine_Clear(&mgr->pingpong.delayL);
    DelayLine_Clear(&mgr->pingpong.delayR);

    DelayLine_Clear(&mgr->spiral.delayL);
    DelayLine_Clear(&mgr->spiral.delayR);

    for (int i = 0; i < 8; i++) {
        DelayLine_Clear(&mgr->reverb.delayL[i]);
        DelayLine_Clear(&mgr->reverb.delayR[i]);
        mgr->reverb.lastL[i] = 0;
        mgr->reverb.lastR[i] = 0;
    }

    // Roll and SlipRoll might need specialized reset if they are mid-loop
    // For now, clearing their buffers
    DelayLine_Clear(&mgr->roll.delayL);
    DelayLine_Clear(&mgr->roll.delayR);
    
    DelayLine_Clear(&mgr->sliproll.delayL);
    DelayLine_Clear(&mgr->sliproll.delayR);

    DelayLine_Clear(&mgr->helix.delayL);
    DelayLine_Clear(&mgr->helix.delayR);
}

void BeatFXManager_Process(BeatFXManager* mgr, float* outL, float* outR, float inL, float inR, float sampleRate) {
    float wetL = 0, wetR = 0;
    BeatFXManager_ProcessWetOnly(mgr, &wetL, &wetR, inL, inR, sampleRate);
    
    // Calculate dry gain based on Level/Depth knob
    // 0.0 to 0.5: Dry stays at 1.0 (Parallel/Additive)
    // 0.5 to 1.0: Dry fades out (Insert/Crossfade)
    float dryGain = 1.0f;
    if (mgr->isFxOn && mgr->levelDepth > 0.5f) {
        dryGain = (1.0f - mgr->levelDepth) * 2.0f;
    }
    
    *outL = inL * dryGain + wetL;
    *outR = inR * dryGain + wetR;
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
            }
            break;
        case BEATFX_ROLL:
            Roll_Process(&mgr->roll, wetL, wetR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate, mgr->isFxOn);
            break;
        case BEATFX_HELIX:
            Helix_Process(&mgr->helix, wetL, wetR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate, mgr->isFxOn);
            break;
        default:
            break;
    }
}

bool BeatFXManager_HasTails(BeatFXManager* mgr, int channelIdx) {
    if (mgr->targetChannel != channelIdx + 1 && mgr->targetChannel != 0) return false;
    
    // Standard tail-capable effects continue outputting tail even when deck playback stops or track ejects
    if (mgr->activeFX == BEATFX_ECHO || mgr->activeFX == BEATFX_REVERB || 
        mgr->activeFX == BEATFX_SPIRAL || mgr->activeFX == BEATFX_DELAY ||
        mgr->activeFX == BEATFX_PINGPONG || mgr->activeFX == BEATFX_ROLL ||
        mgr->activeFX == BEATFX_HELIX) 
    {
        return true; 
    }
    return false;
}
