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
    // If FX is OFF and not an echo-tail effect, pass dry signal
    // Echo trails (Echo, PingPong, Spiral, Reverb, Roll) might need processing even when OFF, 
    // but the input should be 0.0f or dry only to the output.
    
    float mixInL = mgr->isFxOn ? inL : 0.0f;
    float mixInR = mgr->isFxOn ? inR : 0.0f;

    // Output variables for the FX block
    float fxOutL = inL;
    float fxOutR = inR;

    switch(mgr->activeFX) {
        case BEATFX_DELAY:
            if (!mgr->isFxOn) { *outL = inL; *outR = inR; return; }
            Delay_Process(&mgr->delay, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_ECHO:
            // Echo trails require sending 0 input when OFF
            Echo_Process(&mgr->echo, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            if (!mgr->isFxOn) { fxOutL += inL; fxOutR += inR; } // Mix dry
            break;
        case BEATFX_PINGPONG:
            PingPong_Process(&mgr->pingpong, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            if (!mgr->isFxOn) { fxOutL += inL; fxOutR += inR; }
            break;
        case BEATFX_SPIRAL:
            Spiral_Process(&mgr->spiral, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            if (!mgr->isFxOn) { fxOutL += inL; fxOutR += inR; }
            break;
        case BEATFX_REVERB:
            Reverb_Process(&mgr->reverb, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, mgr->scrubVal, sampleRate);
            if (!mgr->isFxOn) { fxOutL += inL; fxOutR += inR; }
            break;
        case BEATFX_TRANS:
            if (!mgr->isFxOn) { *outL = inL; *outR = inR; return; }
            Trans_Process(&mgr->trans, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_FILTER:
            if (!mgr->isFxOn) { *outL = inL; *outR = inR; return; }
            BFilter_Process(&mgr->bfilter, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_FLANGER:
            if (!mgr->isFxOn) { *outL = inL; *outR = inR; return; }
            Flanger_Process(&mgr->flanger, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, mgr->scrubVal, mgr->isScrubbing, sampleRate);
            break;
        case BEATFX_PHASER:
            if (!mgr->isFxOn) { *outL = inL; *outR = inR; return; }
            Phaser_Process(&mgr->phaser, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_PITCH:
            if (!mgr->isFxOn) { *outL = inL; *outR = inR; return; }
            Pitch_Process(&mgr->pitch, &fxOutL, &fxOutR, mixInL, mixInR, mgr->beatMs, mgr->levelDepth, sampleRate);
            break;
        case BEATFX_SLIPROLL:
            SlipRoll_Process(&mgr->sliproll, &fxOutL, &fxOutR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate, mgr->isFxOn);
            // Internal dry/wet handling
            break;
        case BEATFX_ROLL: // Tail effect
            Roll_Process(&mgr->roll, &fxOutL, &fxOutR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate, mgr->isFxOn);
            break;
        case BEATFX_VINYLBRAKE:
            VinylBrake_Process(&mgr->vinylbrake, &fxOutL, &fxOutR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate, mgr->isFxOn);
            break;
        case BEATFX_HELIX: // Tail effect
            Helix_Process(&mgr->helix, &fxOutL, &fxOutR, inL, inR, mgr->beatMs, mgr->levelDepth, sampleRate, mgr->isFxOn);
            break;
        default: break;
    }

    *outL = fxOutL;
    *outR = fxOutR;
}
