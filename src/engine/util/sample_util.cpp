#include "engine/util/sample.h"
#include <stdlib.h>
#include <string.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CSAMPLE* SampleUtil::alloc(SINT size) {
    // Basic aligned allocation (simplification for port)
    void* ptr = nullptr;
#ifdef _WIN32
    ptr = _aligned_malloc(size * sizeof(CSAMPLE), 16);
#else
    if (posix_memalign(&ptr, 16, size * sizeof(CSAMPLE)) != 0) return nullptr;
#endif
    return (CSAMPLE*)ptr;
}

void SampleUtil::free(CSAMPLE* pBuffer) {
#ifdef _WIN32
    _aligned_free(pBuffer);
#else
    ::free(pBuffer);
#endif
}

void SampleUtil::applyRampingGain(CSAMPLE* pBuffer, CSAMPLE_GAIN old_gain,
                                  CSAMPLE_GAIN new_gain, SINT numSamples) {
    if (numSamples <= 0) return;
    CSAMPLE_GAIN gain_step = (new_gain - old_gain) / (CSAMPLE_GAIN)numSamples;
    CSAMPLE_GAIN current_gain = old_gain;
    for (SINT i = 0; i < numSamples; ++i) {
        pBuffer[i] *= current_gain;
        current_gain += gain_step;
    }
}

void SampleUtil::linearCrossfadeBuffersOut(CSAMPLE* pDestSrcFadeOut, 
                                            const CSAMPLE* pSrcFadeIn, 
                                            SINT numSamples, 
                                            int channelCount) {
    if (numSamples <= 0) return;
    
    // Simplification of Mixxx's vectorized crossfade
    SINT frames = numSamples / channelCount;
    for (SINT i = 0; i < frames; ++i) {
        float mix = (float)i / (float)frames;
        float wOut = 1.0f - mix;
        float wIn = mix;
        
        for (int c = 0; c < channelCount; ++c) {
            int idx = i * channelCount + c;
            pDestSrcFadeOut[idx] = (pDestSrcFadeOut[idx] * wOut) + (pSrcFadeIn[idx] * wIn);
        }
    }
}
