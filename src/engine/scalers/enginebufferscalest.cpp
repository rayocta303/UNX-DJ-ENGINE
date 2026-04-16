#include "engine/scalers/enginebufferscalest.h"
#include "audio/scalers.h"
#include <cstring>
#include <cmath>

class EngineBufferScaleSTPrivate {
public:
    WSOLA wsola;
    float* backBuffer;
    int backBufferSize;
};

EngineBufferScaleST::EngineBufferScaleST(ReadAheadManager* pReadAheadManager)
    : m_pReadAheadManager(pReadAheadManager),
      d(new EngineBufferScaleSTPrivate()) 
{
    WSOLA_Init(&d->wsola, 44100);
    d->backBuffer = nullptr;
    d->backBufferSize = 0;
}

EngineBufferScaleST::~EngineBufferScaleST() {
    if (d->backBuffer) free(d->backBuffer);
    delete d;
}

void EngineBufferScaleST::clear() {
    WSOLA_Init(&d->wsola, d->wsola.sampleRate);
}

void EngineBufferScaleST::onSignalChanged() {
    d->wsola.sampleRate = getOutputSignal().getSampleRate();
    if (d->backBuffer) free(d->backBuffer);
    d->backBufferSize = 2048 * getOutputSignal().getChannelCount();
    d->backBuffer = (float*)malloc(d->backBufferSize * sizeof(float));
}

// Minimal implementation matching Mixxx interface
double EngineBufferScaleST::scaleBuffer(CSAMPLE* pOutputBuffer, SINT iOutputBufferSize) {
    if (m_dTempoRatio == 0.0) {
        memset(pOutputBuffer, 0, iOutputBufferSize * sizeof(CSAMPLE));
        return 0.0;
    }

    uint32_t frames = getOutputSignal().samples2frames(iOutputBufferSize);
    WSOLA_Process(&d->wsola, nullptr, pOutputBuffer, frames, m_dTempoRatio, 
                 nullptr, nullptr, 0.0); // Simplified for this shim
                 
    return frames * m_dTempoRatio;
}
