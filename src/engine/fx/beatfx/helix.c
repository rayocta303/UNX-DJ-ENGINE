#include "helix.h"
#include <math.h>

void Helix_Init(BeatFX_Helix *fx) {
  DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
  DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
  fx->currentMs = 0.0f;
  fx->holding = false;
}

void Helix_Free(BeatFX_Helix *fx) {
  DelayLine_Free(&fx->delayL);
  DelayLine_Free(&fx->delayR);
}

void Helix_Process(BeatFX_Helix *fx, float *outL, float *outR, float inL,
                   float inR, float beatMs, float levelDepth, float sampleRate,
                   bool isFxOn) {
  if (isFxOn && !fx->holding) {
    fx->holding = true;
    fx->currentMs = beatMs;
  } else if (!isFxOn && fx->holding) {
    fx->holding = false;
  }

  if (!fx->holding) {
    DelayLine_Write(&fx->delayL, inL);
    DelayLine_Write(&fx->delayR, inR);
    *outL = 0; // Wet-Only return silence if not active
    *outR = 0;
    return;
  }

  // Shrinking loop size simulates pitch up and acceleration
  fx->currentMs *= 0.999f;
  if (fx->currentMs < 10.0f)
    fx->currentMs = 10.0f;

  float readSamples = (fx->currentMs / 1000.0f) * sampleRate;
  float dl = DelayLine_Read(&fx->delayL, readSamples);
  float dr = DelayLine_Read(&fx->delayR, readSamples);

  DelayLine_Write(&fx->delayL, dl * 0.995f); // Faster decay for finite release
  DelayLine_Write(&fx->delayR, dr * 0.995f);

  // Mix: Output is ONLY the wet signal (Tail)
  float wetGain = powf(levelDepth, 0.7f) * 1.5f;
  if (wetGain > 1.0f)
    wetGain = 1.0f;

  *outL = dl * wetGain;
  *outR = dr * wetGain;
}
