#include "reverb.h"
#include <math.h>

void Reverb_Init(BeatFX_Reverb *fx) {
  DelayLine_Init(&fx->delayL[0], 1553);
  DelayLine_Init(&fx->delayL[1], 2011);
  DelayLine_Init(&fx->delayL[2], 2657);
  DelayLine_Init(&fx->delayL[3], 3181);
  DelayLine_Init(&fx->delayL[4], 3733);
  DelayLine_Init(&fx->delayL[5], 4273);
  DelayLine_Init(&fx->delayL[6], 4937);
  DelayLine_Init(&fx->delayL[7], 5651);

  DelayLine_Init(&fx->delayR[0], 1621);
  DelayLine_Init(&fx->delayR[1], 2131);
  DelayLine_Init(&fx->delayR[2], 2713);
  DelayLine_Init(&fx->delayR[3], 3251);
  DelayLine_Init(&fx->delayR[4], 3821);
  DelayLine_Init(&fx->delayR[5], 4349);
  DelayLine_Init(&fx->delayR[6], 5011);
  DelayLine_Init(&fx->delayR[7], 5737);
  
  for(int i=0; i<8; i++) { fx->lastL[i] = 0; fx->lastR[i] = 0; }

  Biquad_Init(&fx->lpfL);
  Biquad_Init(&fx->lpfR);
  Biquad_Init(&fx->hpfL);
  Biquad_Init(&fx->hpfR);
}

void Reverb_Free(BeatFX_Reverb *fx) {
  for (int i = 0; i < 8; i++) {
    DelayLine_Free(&fx->delayL[i]);
    DelayLine_Free(&fx->delayR[i]);
  }
}

void Reverb_Process(BeatFX_Reverb *fx, float *outL, float *outR, float inL,
                    float inR, float beatMs, float levelDepth, float scrubVal,
                    float sampleRate) {
  // Room size (decay) - Adjusted for slightly faster release
  // min: 0.85, max: 0.985
  float decay = 0.85f + (beatMs / 10000.0f) * 0.01f + levelDepth * 0.125f;
  if (decay > 0.985f)
    decay = 0.985f;
  
  float damp = 0.35f + levelDepth * 0.15f; // High-frequency damping

  float wetL = 0, wetR = 0;

  for (int i = 0; i < 8; i++) {
    float dl = DelayLine_Read(&fx->delayL[i], fx->delayL[i].length - 1);
    float dr = DelayLine_Read(&fx->delayR[i], fx->delayR[i].length - 1);

    wetL += dl;
    wetR += dr;

    // Soft-clipping feedback for safety in large spaces
    float fbL = inL + dl * decay;
    float fbR = inR + dr * decay;

    // Soft-limiter (as in Echo)
    if (fbL > 1.0f)
      fbL = 1.0f + (fbL - 1.0f) / (1.0f + (fbL - 1.0f) * (fbL - 1.0f));
    else if (fbL < -1.0f)
      fbL = -1.0f + (fbL + 1.0f) / (1.0f + (fbL + 1.0f) * (fbL + 1.0f));
    if (fbR > 1.0f)
      fbR = 1.0f + (fbR - 1.0f) / (1.0f + (fbR - 1.0f) * (fbR - 1.0f));
    else if (fbR < -1.0f)
      fbR = -1.0f + (fbR + 1.0f) / (1.0f + (fbR + 1.0f) * (fbR + 1.0f));

    // Feedback damping (one-pole LPF) for smoother tail
    fbL = fbL * (1.0f - damp) + fx->lastL[i] * damp;
    fbR = fbR * (1.0f - damp) + fx->lastR[i] * damp;
    fx->lastL[i] = fbL;
    fx->lastR[i] = fbR;

    DelayLine_Write(&fx->delayL[i], fbL);
    DelayLine_Write(&fx->delayR[i], fbR);
  }

  wetL /= 8.0f;
  wetR /= 8.0f;

  // Filter processing via X-PAD scrub
  if (scrubVal < -0.05f) { // LPF
    float cutoff = 20000.0f * powf(0.01f, fabsf(scrubVal));
    Biquad_SetLowpass(&fx->lpfL, cutoff, 0.707f, sampleRate);
    Biquad_SetLowpass(&fx->lpfR, cutoff, 0.707f, sampleRate);
    wetL = Biquad_Process(&fx->lpfL, wetL);
    wetR = Biquad_Process(&fx->lpfR, wetR);
  } else if (scrubVal > 0.05f) { // HPF
    float cutoff = 20.0f * powf(400.0f, fabsf(scrubVal));
    Biquad_SetHighpass(&fx->hpfL, cutoff, 0.707f, sampleRate);
    Biquad_SetHighpass(&fx->hpfR, cutoff, 0.707f, sampleRate);
    wetL = Biquad_Process(&fx->hpfL, wetL);
    wetR = Biquad_Process(&fx->hpfR, wetR);
  }

  // Mix: Output is ONLY the wet signal (Tail)
  float wetGain = powf(levelDepth, 0.7f) * 1.5f;
  if (wetGain > 1.0f)
    wetGain = 1.0f;

  *outL = wetL * wetGain;
  *outR = wetR * wetGain;
}
