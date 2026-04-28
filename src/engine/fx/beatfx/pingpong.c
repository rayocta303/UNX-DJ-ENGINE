#include "pingpong.h"
#include <math.h>

void PingPong_Init(BeatFX_PingPong *fx) {
  DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
  DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
}

void PingPong_Free(BeatFX_PingPong *fx) {
  DelayLine_Free(&fx->delayL);
  DelayLine_Free(&fx->delayR);
}

void PingPong_Process(BeatFX_PingPong *fx, float *outL, float *outR, float inL,
                      float inR, float beatMs, float levelDepth,
                      float sampleRate) {
  float delaySamples = (beatMs / 1000.0f) * sampleRate;

  // Read
  float dl = DelayLine_Read(&fx->delayL, delaySamples);
  float dr = DelayLine_Read(&fx->delayR, delaySamples);

  // Professional finite Ping Pong feedback
  float feedback = 0.4f + powf(levelDepth, 0.5f) * 0.55f;
  if (levelDepth < 0.001f) feedback = 0.0f;

  // Cross feedback for Ping Pong effect
  DelayLine_Write(&fx->delayL, inL + dr * feedback);
  DelayLine_Write(&fx->delayR, inR + dl * feedback);

  // Mix: Output is ONLY the wet signal (Tail)
  float wetGain = powf(levelDepth, 0.7f) * 1.5f;
  if (wetGain > 1.0f) wetGain = 1.0f;

  *outL = dl * wetGain;
  *outR = dr * wetGain;
}
