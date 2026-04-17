#include "engine/enginebuffer.h"
#include "engine/readaheadmanager.h"
#include "engine/scalers/enginebufferscalest.h"
#include "engine/cachingreader/simplecachingreader.h"
#include <iostream>

EngineBuffer::EngineBuffer(const QString& group,
                           UserSettingsPointer pConfig,
                           EngineChannel* pChannel,
                           EngineMixer* pMixingEngine,
                           unx::audio::ChannelCount maxSupportedChannel)
    : m_group(group),
      m_channelCount(maxSupportedChannel),
      m_pReadAheadManager(nullptr),
      m_pReader(nullptr),
      m_pScale(nullptr),
      m_pScaleST(nullptr),
      m_playPos(0) {
    
    // In our simplified port, we assume SimpleCachingReader is initialized elsewhere
    // or we'll wrap the deck's PCM buffer here.
}

EngineBuffer::~EngineBuffer() {
    delete m_pScaleST;
    delete m_pReadAheadManager;
    delete m_pReader;
}

void EngineBuffer::setScalerForTest(EngineBufferScale* pScaleVinyl, EngineBufferScale* pScaleKeylock) {
    m_pScaleVinyl = pScaleVinyl;
    m_pScaleKeylock = pScaleKeylock;
}

void EngineBuffer::process(CSAMPLE* pOut, const std::size_t bufferSize) {
    if (!m_pScale) {
        SampleUtil::clear(pOut, (SINT)bufferSize);
        return;
    }

    // Update Scaler Parameters
    double tempo = getSpeed();
    double pitch = 1.0; // Simplification, can be expanded to follow KeyControl
    double base = 1.0; 

    m_pScale->setScaleParameters(base, &tempo, &pitch);

    // Scaling process (SoundTouch)
    double framesRead = m_pScale->scaleBuffer(pOut, (SINT)bufferSize);

    // Update internal play position
    m_playPos += framesRead;
}

// Helper to bridge to existing DeckAudioState
void EngineBuffer::initializeWithPCM(float* pBuffer, uint32_t totalSamples, uint32_t sampleRate) {
    // Clean up old
    if (m_pScaleST) delete m_pScaleST;
    if (m_pReadAheadManager) delete m_pReadAheadManager;
    if (m_pReader) delete m_pReader;

    // Create new pipeline
    m_pReader = new SimpleCachingReader(pBuffer, totalSamples, sampleRate);
    m_pReadAheadManager = new ReadAheadManager(static_cast<SimpleCachingReader*>(m_pReader));
    m_pScaleST = new EngineBufferScaleST(m_pReadAheadManager);
    
    // Set active scaler
    m_pScale = m_pScaleST;
}

double EngineBuffer::getSpeed() const {
    // This should ideally pull from RateControl
    return m_speed_old; 
}

void EngineBuffer::setSpeed(double speed) {
    m_speed_old = speed;
}

void EngineBuffer::seekAbs(unx::audio::FramePos pos) {
    m_playPos = pos;
    if (m_pReadAheadManager) {
        m_pReadAheadManager->notifySeek(pos.value());
    }
    if (m_pScale) {
        m_pScale->clear();
    }
}

void EngineBuffer::collectFeatures(GroupFeatureState* pGroupFeatures) const {
    Q_UNUSED(pGroupFeatures);
}
