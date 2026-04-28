#include "spiral.h"
#include <math.h>

void Spiral_Init(BeatFX_Spiral *fx) {
  DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
  DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
  fx->currentFeedback = 0.0f;
  fx->targetFeedback = 0.0f;
}

void Spiral_Free(BeatFX_Spiral *fx) {
  DelayLine_Free(&fx->delayL);
  DelayLine_Free(&fx->delayR);
}

void Spiral_Process(BeatFX_Spiral *fx, float *outL, float *outR, float inL,
                    float inR, float beatMs, float levelDepth,
                    float sampleRate) {
  float delaySamples = (beatMs / 1000.0f) * sampleRate;

  // Read
  float dl = DelayLine_Read(&fx->delayL, delaySamples);
  float dr = DelayLine_Read(&fx->delayR, delaySamples);

  // Spiral Persistence (finite release)
  // min: 0.7 (natural), max: 0.992 (massive)
  fx->targetFeedback = 0.7f + powf(levelDepth, 0.5f) * 0.292f;
  if (levelDepth < 0.001f)
    fx->targetFeedback = 0.0f;

  // Smooth feedback to prevent clicks
  fx->currentFeedback += (fx->targetFeedback - fx->currentFeedback) * 0.001f;

  DelayLine_Write(&fx->delayL, inL + dl * fx->currentFeedback);
  DelayLine_Write(&fx->delayR, inR + dr * fx->currentFeedback);

  // Mix: Output is ONLY the wet signal (Tail)
  float wetGain = powf(levelDepth, 0.7f) * 1.5f;
  if (wetGain > 1.0f)
    wetGain = 1.0f;

  *outL = dl * wetGain;
  *outR = dr * wetGain;
}
