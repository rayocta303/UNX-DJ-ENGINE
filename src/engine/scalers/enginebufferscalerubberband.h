#pragma once
#include "engine/scalers/enginebufferscale.h"
#include <memory>

class ReadAheadManager;
class EngineBufferScaleRubberBandPrivate;

class EngineBufferScaleRubberBand : public EngineBufferScale {
public:
    EngineBufferScaleRubberBand(ReadAheadManager* pReadAheadManager);
    ~EngineBufferScaleRubberBand() override;

    void clear() override;
    double scaleBuffer(CSAMPLE* pOutputBuffer, SINT iOutputBufferSize) override;

    static bool isEngineFinerAvailable() { return true; }

protected:
    void onSignalChanged() override;

private:
    ReadAheadManager* m_pReadAheadManager;
    std::unique_ptr<EngineBufferScaleRubberBandPrivate> d;
};
