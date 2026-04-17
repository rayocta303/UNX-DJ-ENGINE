#ifndef SIMPLE_CACHING_READER_H
#define SIMPLE_CACHING_READER_H

#include "engine/shim.h"
#include "audio/types.h"
#include "engine/util/sample.h"

#include "engine/cachingreader/cachingreader.h"

// Minimal bridge for Mixxx's CachingReader interface
class SimpleCachingReader : public CachingReader {
public:
    enum class ReadResult {
        UNAVAILABLE,
        PARTIALLY_AVAILABLE,
        AVAILABLE,
    };

    SimpleCachingReader(float* pPCMBuffer, uint32_t totalSamples, uint32_t sampleRate)
        : m_pPCMBuffer(pPCMBuffer), 
          m_totalSamples(totalSamples), 
          m_sampleRate(sampleRate) {}

    virtual ~SimpleCachingReader() {}

    virtual ReadResult read(SINT startSample,
                          SINT numSamples,
                          bool reverse,
                          CSAMPLE* buffer,
                          unx::audio::ChannelCount channelCount) {
        if (!m_pPCMBuffer || numSamples <= 0) {
            SampleUtil::clear(buffer, numSamples);
            return ReadResult::UNAVAILABLE;
        }

        SINT frames = numSamples / channelCount;
        
        if (reverse) {
            // Mixxx reverse read logic
            for (SINT i = 0; i < frames; ++i) {
                SINT pos = startSample - (i * channelCount);
                if (pos >= 0 && pos < (SINT)m_totalSamples - (SINT)channelCount + 1) {
                    for (int c = 0; c < (int)channelCount; ++c) {
                        buffer[i * channelCount + c] = m_pPCMBuffer[pos + c];
                    }
                } else {
                    for (int c = 0; c < (int)channelCount; ++c) {
                        buffer[i * channelCount + c] = 0.0f;
                    }
                }
            }
        } else {
            // Forward read logic
            for (SINT i = 0; i < frames; ++i) {
                SINT pos = startSample + (i * channelCount);
                if (pos >= 0 && pos < (SINT)m_totalSamples - (SINT)channelCount + 1) {
                    for (int c = 0; c < (int)channelCount; ++c) {
                        buffer[i * channelCount + c] = m_pPCMBuffer[pos + c];
                    }
                } else {
                    for (int c = 0; c < (int)channelCount; ++c) {
                        buffer[i * channelCount + c] = 0.0f;
                    }
                }
            }
        }

        return ReadResult::AVAILABLE;
    }

private:
    float* m_pPCMBuffer;
    uint32_t m_totalSamples;
    uint32_t m_sampleRate;
};

#endif // SIMPLE_CACHING_READER_H
