#include "engine/scalers/enginebufferscalest.h"
#include <cmath>
#include <algorithm>

namespace {
constexpr SINT kBackBufferFrameSize = 512;
}

EngineBufferScaleST::EngineBufferScaleST(ReadAheadManager* pReadAheadManager)
    : m_pReadAheadManager(pReadAheadManager),
      m_pSoundTouch(std::make_unique<soundtouch::SoundTouch>()),
      m_bufferBack(nullptr),
      m_bufferBackSize(0),
      m_bBackwards(false) {
    m_pSoundTouch->setRate(m_dBaseRate);
    m_pSoundTouch->setPitch(1.0);
    m_pSoundTouch->setSetting(SETTING_USE_QUICKSEEK, 0); // Disable quickseek for hi-fi
    
    // Professional DJ parameters for high-fidelity stretching
    m_pSoundTouch->setSetting(SETTING_SEQUENCE_MS, 82);
    m_pSoundTouch->setSetting(SETTING_SEEKWINDOW_MS, 28);
    m_pSoundTouch->setSetting(SETTING_OVERLAP_MS, 12);
    
    // Initial setup with project defaults
    m_pSoundTouch->setSampleRate(44100);
    m_pSoundTouch->setChannels(2);
    
    onSignalChanged();
}

EngineBufferScaleST::~EngineBufferScaleST() {
    if (m_bufferBack) {
        SampleUtil::free(m_bufferBack);
    }
}

void EngineBufferScaleST::setScaleParameters(double base_rate,
                                             double* pTempoRatio,
                                             double* pPitchRatio) {
    m_bBackwards = *pTempoRatio < 0;

    double speed_abs = fabs(*pTempoRatio);
    if (speed_abs > MAX_SEEK_SPEED) {
        speed_abs = MAX_SEEK_SPEED;
    } else if (speed_abs < MIN_SEEK_SPEED) {
        speed_abs = 0;
    }

    *pTempoRatio = m_bBackwards ? -speed_abs : speed_abs;

    if (speed_abs != m_dTempoRatio) {
        m_pSoundTouch->setTempo(speed_abs > 0 ? speed_abs : 0.001);
        m_dTempoRatio = speed_abs;
    }
    if (base_rate != m_dBaseRate) {
        m_pSoundTouch->setRate(base_rate);
        m_dBaseRate = base_rate;
    }

    if (*pPitchRatio != m_dPitchRatio) {
        double pitch = fabs(*pPitchRatio);
        if (pitch > 0.0) {
            m_pSoundTouch->setPitch(pitch);
        }
        m_dPitchRatio = *pPitchRatio;
    }
}

void EngineBufferScaleST::onSignalChanged() {
    // For now, assume always 2 channels and 44100 SR as per engine.h
    uint32_t channels = 2;
    uint32_t newSize = kBackBufferFrameSize * channels;
    
    if (m_bufferBackSize != newSize) {
        if (m_bufferBack) SampleUtil::free(m_bufferBack);
        m_bufferBack = SampleUtil::alloc(newSize);
        m_bufferBackSize = newSize;
    }

    m_pSoundTouch->setSampleRate(44100);
    m_pSoundTouch->setChannels(channels);

    // Warm up/Pre-allocate
    m_pSoundTouch->setTempo(0.1);
    m_pSoundTouch->setTempo(m_dTempoRatio > 0 ? m_dTempoRatio : 1.0);
    clear();
}

void EngineBufferScaleST::clear() {
    m_pSoundTouch->clear();
    m_effectiveRate = m_dBaseRate * m_dTempoRatio;
}

double EngineBufferScaleST::scaleBuffer(CSAMPLE* pOutputBuffer,
                                         SINT iOutputBufferSize) {
    if (m_dBaseRate == 0.0 || m_dTempoRatio == 0.0 || m_dPitchRatio == 0.0) {
        SampleUtil::clear(pOutputBuffer, iOutputBufferSize);
        return 0.0;
    }

    double readFramesProcessed = 0;
    uint32_t channels = 2; // Assuming stereo
    SINT remaining_frames = iOutputBufferSize / channels;
    CSAMPLE* readPtr = pOutputBuffer;
    bool last_read_failed = false;

    while (remaining_frames > 0) {
        SINT received_frames = m_pSoundTouch->receiveSamples(readPtr, remaining_frames);
        remaining_frames -= received_frames;
        readFramesProcessed += m_effectiveRate * received_frames;
        readPtr += received_frames * channels;

        if (remaining_frames > 0) {
            m_effectiveRate = m_dBaseRate * m_dTempoRatio;
            SINT iAvailSamples = m_pReadAheadManager->getNextSamples(
                (m_bBackwards ? -1.0 : 1.0) * m_effectiveRate,
                m_bufferBack,
                m_bufferBackSize,
                unx::audio::ChannelCount(static_cast<int>(channels)));
            
            SINT iAvailFrames = iAvailSamples / channels;

            if (iAvailFrames > 0) {
                last_read_failed = false;
                m_pSoundTouch->putSamples(m_bufferBack, iAvailFrames);
            } else {
                if (last_read_failed) {
                    // Padding with silence to flush the SoundTouch pipeline
                    SampleUtil::clear(m_bufferBack, m_bufferBackSize);
                    m_pSoundTouch->putSamples(m_bufferBack, kBackBufferFrameSize);
                }
                last_read_failed = true;
            }
        }
    }

    return readFramesProcessed;
}
