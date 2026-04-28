#include "delay.h"
#include <math.h>

void Delay_Init(BeatFX_Delay *fx) {
  DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
  DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
}

void Delay_Free(BeatFX_Delay *fx) {
  DelayLine_Free(&fx->delayL);
  DelayLine_Free(&fx->delayR);
}

void Delay_Process(BeatFX_Delay *fx, float *outL, float *outR, float inL,
                   float inR, float beatMs, float levelDepth,
                   float sampleRate) {
  float delaySamples = (beatMs / 1000.0f) * sampleRate;

  // Read
  float dl = DelayLine_Read(&fx->delayL, delaySamples);
  float dr = DelayLine_Read(&fx->delayR, delaySamples);

  // Professional finite Delay feedback
  float feedback = 0.4f + powf(levelDepth, 0.5f) * 0.585f;
  if (levelDepth < 0.001f) feedback = 0.0f;

  DelayLine_Write(&fx->delayL, inL + dl * feedback);
  DelayLine_Write(&fx->delayR, inR + dr * feedback);

  // Mix: Output is ONLY the wet signal (Tail)
  float wetGain = powf(levelDepth, 0.7f) * 1.5f;
  if (wetGain > 1.0f)
    wetGain = 1.0f;

  *outL = dl * wetGain;
  *outR = dr * wetGain;
}
