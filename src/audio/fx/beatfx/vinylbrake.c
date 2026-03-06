#include "vinylbrake.h"
#include <math.h>

void VinylBrake_Init(BeatFX_VinylBrake* fx) {
    // Need a tiny buffer for variable-speed resampling
    DelayLine_Init(&fx->delayL, 4096);
    DelayLine_Init(&fx->delayR, 4096);
    fx->currentSpeed = 1.0f;
    fx->breaking = false;
}

void VinylBrake_Free(BeatFX_VinylBrake* fx) {
    DelayLine_Free(&fx->delayL);
    DelayLine_Free(&fx->delayR);
}

void VinylBrake_Process(BeatFX_VinylBrake* fx, float* outL, float* outR, float inL, float inR, float beatMs, float levelDepth, float sampleRate, bool isFxOn) {
    DelayLine_Write(&fx->delayL, inL);
    DelayLine_Write(&fx->delayR, inR);

    // Determines how fast it stops / starts
    float delta = 1000.0f / (beatMs * sampleRate);

    if (isFxOn) {
        fx->breaking = true;
        fx->currentSpeed -= delta;
        if (fx->currentSpeed < 0.0f) fx->currentSpeed = 0.0f;
    } else {
        if (fx->breaking) {
            fx->currentSpeed += delta;
            if (fx->currentSpeed >= 1.0f) {
                fx->currentSpeed = 1.0f;
                fx->breaking = false; // Re-engaged
            }
        } else {
            fx->currentSpeed = 1.0f;
        }
    }

    if (!fx->breaking) {
        *outL = inL;
        *outR = inR;
        return;
    }

    // A true vinyl brake changes pitch and playback speed. 
    // This requires reading from a constantly accumulating delay offset
    // For a generic real-time buffer, this will just pitch down over a tiny buffer, 
    // effectively stretching a tiny grain.
    static float readOffset = 0.0f;
    readOffset += (1.0f - fx->currentSpeed);
    if (readOffset > 2000.0f) readOffset -= 2000.0f;

    float pL = DelayLine_Read(&fx->delayL, readOffset);
    float pR = DelayLine_Read(&fx->delayR, readOffset);

    // LevelDepth acts as Dry/Wet
    *outL = inL * (1.0f - levelDepth) + pL * levelDepth;
    *outR = inR * (1.0f - levelDepth) + pR * levelDepth;
}
