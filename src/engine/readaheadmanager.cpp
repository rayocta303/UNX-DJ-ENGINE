#include "engine/readaheadmanager.h"
#include <cmath>
#include <algorithm>

ReadAheadManager::ReadAheadManager(SimpleCachingReader* pReader)
    : m_pReader(pReader),
      m_currentPosition(0),
      m_loopStart(kNoTrigger),
      m_loopEnd(kNoTrigger),
      m_loopEnabled(false),
      m_pCrossFadeBuffer(SampleUtil::alloc(4096 * 2)),
      m_cacheMissCount(0),
      m_cacheMissExpected(false) {
}

ReadAheadManager::~ReadAheadManager() {
    SampleUtil::free(m_pCrossFadeBuffer);
}

SINT ReadAheadManager::getNextSamples(double dRate,
                                       CSAMPLE* pOutput,
                                       SINT requested_samples,
                                       unx::audio::ChannelCount channelCount) {
    
    // Safety check for alignment
    int modSamples = requested_samples % channelCount;
    if (modSamples != 0) {
        requested_samples -= modSamples;
    }

    bool in_reverse = dRate < 0;
    SINT samples_from_reader = requested_samples;
    bool reachedTrigger = false;
    double target = kNoTrigger;

    // Simplified Loop Trigger Logic
    if (m_loopEnabled && m_loopStart != kNoTrigger && m_loopEnd != kNoTrigger) {
        if (!in_reverse && m_currentPosition < m_loopEnd) {
            double samplesToTrigger = m_loopEnd - m_currentPosition;
            if (samplesToTrigger < (double)requested_samples) {
                samples_from_reader = SampleUtil::ceilPlayPosToFrameStart(samplesToTrigger, channelCount);
                reachedTrigger = true;
                target = m_loopStart;
            }
        } else if (in_reverse && m_currentPosition > m_loopStart) {
            double samplesToTrigger = m_currentPosition - m_loopStart;
            if (samplesToTrigger < (double)requested_samples) {
                samples_from_reader = SampleUtil::ceilPlayPosToFrameStart(samplesToTrigger, channelCount);
                reachedTrigger = true;
                target = m_loopEnd;
            }
        }
    }

    SINT start_sample = SampleUtil::roundPlayPosToFrameStart(m_currentPosition, channelCount);
    
    auto readResult = m_pReader->read(start_sample, samples_from_reader, in_reverse, pOutput, channelCount);
    
    if (readResult == SimpleCachingReader::ReadResult::UNAVAILABLE) {
        SampleUtil::clear(pOutput, samples_from_reader);
        m_cacheMissCount++;
    } else if (m_cacheMissCount > 0) {
        SampleUtil::applyRampingGain(pOutput, 0.0f, 1.0f, samples_from_reader);
        m_cacheMissCount = 0;
        m_cacheMissExpected = false;
    }

    // Update position
    if (in_reverse) {
        m_currentPosition -= samples_from_reader;
    } else {
        m_currentPosition += samples_from_reader;
    }

    // Handle Wrap Around (Looping)
    if (reachedTrigger) {
        double overshoot = samples_from_reader - (in_reverse ? (m_currentPosition + samples_from_reader - m_loopStart) : (m_loopEnd - (m_currentPosition - samples_from_reader)));
        m_currentPosition = target + (in_reverse ? -overshoot : overshoot);

        // Crossfade logic (Ported from Mixxx)
        int crossFadeSamples = samples_from_reader;
        if (crossFadeSamples > 0) {
            SINT seek_pos = SampleUtil::roundPlayPosToFrameStart(m_currentPosition + (in_reverse ? crossFadeSamples : -crossFadeSamples), channelCount);
            
            // Read into crossfade buffer
            auto res = m_pReader->read(seek_pos, crossFadeSamples, in_reverse, m_pCrossFadeBuffer, channelCount);
            
            if (res == SimpleCachingReader::ReadResult::AVAILABLE) {
                // do crossfade from the current buffer into the new loop beginning
                SampleUtil::linearCrossfadeBuffersOut(pOutput, m_pCrossFadeBuffer, crossFadeSamples, (int)channelCount);
            } else {
                // No samples for crossfading, ramp to zero to avoid pop
                SampleUtil::applyRampingGain(pOutput, 1.0f, 0.0f, samples_from_reader);
            }
        }
    }

    return samples_from_reader;
}

void ReadAheadManager::notifySeek(double seekPosition) {
    m_currentPosition = seekPosition;
    m_cacheMissCount = 0;
    m_cacheMissExpected = true;
    m_readAheadLog.clear();
}
