#include "echo.h"
#include <math.h>

void Echo_Init(BeatFX_Echo *fx) {
  DelayLine_Init(&fx->delayL, MAX_DELAY_SAMPLES);
  DelayLine_Init(&fx->delayR, MAX_DELAY_SAMPLES);
  fx->lastOutL = 0.0f;
  fx->lastOutR = 0.0f;
}

void Echo_Free(BeatFX_Echo *fx) {
  DelayLine_Free(&fx->delayL);
  DelayLine_Free(&fx->delayR);
}

void Echo_Process(BeatFX_Echo *fx, float *outL, float *outR, float inL,
                  float inR, float beatMs, float levelDepth, float sampleRate) {
  float delaySamples = (beatMs / 1000.0f) * sampleRate;

  // Read
  float dl = DelayLine_Read(&fx->delayL, delaySamples);
  float dr = DelayLine_Read(&fx->delayR, delaySamples);

  // Echo Persistence (finite release)
  // min: 0.5 (short), center: 0.84 (natural), max: 0.985 (very long)
  float feedback = 0.5f + powf(levelDepth, 0.5f) * 0.485f;
  if (levelDepth < 0.001f)
    feedback = 0.0f;

  // Feedback Path with Soft-Clipping
  float fbL = inL + dl * feedback;
  float fbR = inR + dr * feedback;

  // Soft-Limiter to prevent harsh clipping while maintaining tails
  if (fbL > 1.0f)
    fbL = 1.0f + (fbL - 1.0f) / (1.0f + (fbL - 1.0f) * (fbL - 1.0f));
  else if (fbL < -1.0f)
    fbL = -1.0f + (fbL + 1.0f) / (1.0f + (fbL + 1.0f) * (fbL + 1.0f));
  if (fbR > 1.0f)
    fbR = 1.0f + (fbR - 1.0f) / (1.0f + (fbR - 1.0f) * (fbR - 1.0f));
  else if (fbR < -1.0f)
    fbR = -1.0f + (fbR + 1.0f) / (1.0f + (fbR + 1.0f) * (fbR + 1.0f));

  DelayLine_Write(&fx->delayL, fbL);
  DelayLine_Write(&fx->delayR, fbR);

  // Mix: Output is ONLY the wet signal (Tail)
  // Adjusted curve: stays louder longer even at lower knob settings
  float wetGain = powf(levelDepth, 0.7f) * 1.5f;
  if (wetGain > 1.0f)
    wetGain = 1.0f;

  *outL = dl * wetGain;
  *outR = dr * wetGain;
}
