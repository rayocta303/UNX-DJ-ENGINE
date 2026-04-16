#include "dsp_utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Biquad Filter ---

void Biquad_Init(BiquadFilter* filter) {
    memset(filter, 0, sizeof(BiquadFilter));
    filter->b0 = 1.0f;
}

static inline void Biquad_SetCoeffs(BiquadFilter* filter, float b0, float b1, float b2, float a0, float a1, float a2) {
    filter->b0 = b0 / a0;
    filter->b1 = b1 / a0;
    filter->b2 = b2 / a0;
    filter->a1 = a1 / a0;
    filter->a2 = a2 / a0;
}

void Biquad_SetLowpass(BiquadFilter* filter, float cutoff, float q, float sampleRate) {
    float w0 = 2.0f * (float)M_PI * cutoff / sampleRate;
    float alpha = sinf(w0) / (2.0f * q);
    float cs = cosf(w0);

    float b0 = (1.0f - cs) / 2.0f;
    float b1 = 1.0f - cs;
    float b2 = (1.0f - cs) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha;

    Biquad_SetCoeffs(filter, b0, b1, b2, a0, a1, a2);
}

void Biquad_SetHighpass(BiquadFilter* filter, float cutoff, float q, float sampleRate) {
    float w0 = 2.0f * (float)M_PI * cutoff / sampleRate;
    float alpha = sinf(w0) / (2.0f * q);
    float cs = cosf(w0);

    float b0 = (1.0f + cs) / 2.0f;
    float b1 = -(1.0f + cs);
    float b2 = (1.0f + cs) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha;

    Biquad_SetCoeffs(filter, b0, b1, b2, a0, a1, a2);
}

void Biquad_SetBandpass(BiquadFilter* filter, float cutoff, float q, float sampleRate) {
    float w0 = 2.0f * (float)M_PI * cutoff / sampleRate;
    float alpha = sinf(w0) / (2.0f * q);
    float cs = cosf(w0);

    float b0 = alpha;
    float b1 = 0.0f;
    float b2 = -alpha;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha;

    Biquad_SetCoeffs(filter, b0, b1, b2, a0, a1, a2);
}

void Biquad_SetPeak(BiquadFilter* filter, float cutoff, float q, float gainDb, float sampleRate) {
    float A = powf(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * (float)M_PI * cutoff / sampleRate;
    float alpha = sinf(w0) / (2.0f * q);
    float cs = cosf(w0);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cs;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha / A;

    Biquad_SetCoeffs(filter, b0, b1, b2, a0, a1, a2);
}

void Biquad_SetNotch(BiquadFilter* filter, float cutoff, float q, float sampleRate) {
    float w0 = 2.0f * (float)M_PI * cutoff / sampleRate;
    float alpha = sinf(w0) / (2.0f * q);
    float cs = cosf(w0);

    float b0 = 1.0f;
    float b1 = -2.0f * cs;
    float b2 = 1.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha;

    Biquad_SetCoeffs(filter, b0, b1, b2, a0, a1, a2);
}

float Biquad_Process(BiquadFilter* filter, float in) {
    float out = filter->b0 * in + filter->b1 * filter->x1 + filter->b2 * filter->x2 
                - filter->a1 * filter->y1 - filter->a2 * filter->y2;
    
    // Denormal fix (Flush to zero)
    if (fabsf(out) < 1e-15f) out = 0.0f;

    filter->x2 = filter->x1;
    filter->x1 = in;
    filter->y2 = filter->y1;
    filter->y1 = out;

    return out;
}

// --- Delay Line ---

bool DelayLine_Init(DelayLine* delay, uint32_t maxLength) {
    delay->buffer = (float*)calloc(maxLength, sizeof(float));
    if (!delay->buffer) return false;
    delay->length = maxLength;
    delay->writePos = 0;
    return true;
}

void DelayLine_Free(DelayLine* delay) {
    if (delay->buffer) {
        free(delay->buffer);
        delay->buffer = NULL;
    }
    delay->length = 0;
    delay->writePos = 0;
}

void DelayLine_Clear(DelayLine* delay) {
    if (delay->buffer) {
        memset(delay->buffer, 0, delay->length * sizeof(float));
    }
}

void DelayLine_Write(DelayLine* delay, float sample) {
    if (!delay->buffer) return;
    delay->buffer[delay->writePos] = sample;
    delay->writePos++;
    if (delay->writePos >= delay->length) {
        delay->writePos = 0;
    }
}

// Engine Hermite4 is used via Engine_InterpolateHermite4

// Reads from the delay line. Returns interpolated sample at `delaySamples` in the past.
float DelayLine_Read(DelayLine* delay, float delaySamples) {
    if (!delay->buffer || delay->length == 0 || delaySamples < 0) return 0.0f;
    if (delaySamples >= delay->length - 3) {
        delaySamples = (float)(delay->length - 3);
    } // Prevent buffer underrun/overrun on extreme modulation

    float readPosF = (float)delay->writePos - delaySamples;
    while (readPosF < 0.0f) {
        readPosF += (float)delay->length;
    }

    uint32_t readPos = (uint32_t)readPosF;
    float frac = readPosF - (float)readPos;

    // Hermite 4-point requires readPos-1, readPos, readPos+1, readPos+2 
    // Wait, the order of timeline relative to write:
    // older -> newer = moving backwards from writePos.
    // readPosF goes backwards in time.
    // So 'x0' is at readPos. 'xm1' is older (readPos - 1), 'x1' is newer (readPos + 1).
    // Let's implement wrap-around indices.
    int i0 = (int)readPos;
    int im1 = (i0 == 0) ? (int)delay->length - 1 : i0 - 1;
    int i1 = (i0 + 1 >= (int)delay->length) ? 0 : i0 + 1;
    int i2 = (i0 + 2 >= (int)delay->length) ? (i0 + 2) - (int)delay->length : i0 + 2;

    float x0 = delay->buffer[i0];
    float xm1 = delay->buffer[im1];
    float x1 = delay->buffer[i1];
    float x2 = delay->buffer[i2];

    return Engine_InterpolateHermite4(frac, xm1, x0, x1, x2);
}

// --- LFO ---

void LFO_Init(LFO* lfo) {
    lfo->type = LFO_SINE;
    lfo->phase = 0.0f;
    lfo->phaseInc = 0.0f;
}

void LFO_SetFreq(LFO* lfo, float freq, float sampleRate) {
    if (sampleRate > 0.0f) {
        lfo->phaseInc = freq / sampleRate;
    }
}

float LFO_Process(LFO* lfo) {
    float out = 0.0f;
    float currentPhase = lfo->phase;

    switch (lfo->type) {
        case LFO_SINE:
            out = sinf(2.0f * (float)M_PI * currentPhase);
            break;
        case LFO_TRIANGLE:
            out = 2.0f * fabsf(2.0f * currentPhase - 1.0f) - 1.0f;
            break;
        case LFO_SAW:
            out = 2.0f * currentPhase - 1.0f;
            break;
        case LFO_SQUARE:
            out = (currentPhase < 0.5f) ? 1.0f : -1.0f;
            break;
    }

    lfo->phase += lfo->phaseInc;
    if (lfo->phase >= 1.0f) lfo->phase -= 1.0f;
    if (lfo->phase < 0.0f) lfo->phase += 1.0f;

    return out;
}

// --- Noise Generator ---
// Simple 32-bit xorshift PRNG + float conversion
static uint32_t xorshift_state = 2463534242;

float WhiteNoise_Process(void) {
    uint32_t x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xorshift_state = x;
    return ((float)x / 4294967296.0f) * 2.0f - 1.0f;
}

// --- Engine Engine Bridge Implementations ---
void Crossover_Init(CrossoverLR4* lr, float freq, float sampleRate, bool highpass) {
    EngineLR4_Init(lr);
    if (highpass) EngineLR4_SetHighpass(lr, freq, sampleRate);
    else EngineLR4_SetLowpass(lr, freq, sampleRate);
}

float Crossover_Process(CrossoverLR4* lr, float in) {
    return EngineLR4_Process(lr, in);
}
